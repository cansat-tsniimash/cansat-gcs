#include "dispatcher.hpp"

#include <tuple>
#include <array>
#include <chrono>

#include "log.hpp"


static auto _slg = build_source("dispatcher");


// Период пола событий на сокете (мс)
#define ITS_DISPATCHER_POLL_PERIOD 1000


dispatcher::dispatcher(istack & istack_, ostack & ostack_, bus_io & io_)
	: _istack(istack_), _ostack(ostack_), _io(io_)
{
	_istack.set_event_handler(this);
}


void dispatcher::poll()
{
	const auto timeout = std::chrono::milliseconds(ITS_DISPATCHER_POLL_PERIOD);

	LOG(trace) << "entering poll cycle";
	const bool have_msgs = _io.poll_sub_socket(timeout);
	LOG(trace) << "poll complete with " << have_msgs;

	if (have_msgs)
	{
		const auto message = _io.recv_message();
		if (message)
		{
			_dispatch_bus_message(*message);
		}
		else
		{
			LOG(trace) << "unable to read message?";
		}
	}
}


void dispatcher::_on_map_sdu_event(const ccsds::uslp::acceptor_event_map_sdu & event)
{
	LOG(info) << "got downlink map sdu frame on gmapid " << event.channel_id;

	// Собираем сообщение
	sdu_downlink message;
	message.gmapid = event.channel_id;
	message.qos = event.qos;
	message.flags = event.flags;
	message.data = event.data;

	_io.send_message(message);
}


void dispatcher::_dispatch_bus_message(const bus_input_message & message)
{
	switch (message.kind)
	{
	case bus_input_message::kind_t::sdu_uplink_request:
		_on_sdu_uplink_request(
				dynamic_cast<const sdu_uplink_request&>(message)
		);
		break;

	case bus_input_message::kind_t::radio_frame_downlink:
		_on_radio_downlink_frame(
				dynamic_cast<const radio_downlink_frame&>(message)
		);
		break;

	case bus_input_message::kind_t::radio_uplink_state:
		_on_radio_uplink_state(
				dynamic_cast<const radio_uplink_state&>(message)
		);
		break;

	default:
		LOG(error) << "unknown bus message type: " << to_string(message.kind);
		break;
	}
}


void dispatcher::_on_sdu_uplink_request(const sdu_uplink_request & request)
{
	LOG(debug) << "got sdu uplink request for " << request.gmapid << ", "
			<< "cookie " << request.cookie
	;

	try
	{
		auto * channel = _ostack.get_map_channel(request.gmapid);
		if (!channel)
		{
			// У нас нет канала, которому бы предназначался этот пакет
			std::stringstream error;
			error << "there is no map channel " << request.gmapid << " "
				<< "registered in output stack";
			throw std::runtime_error(error.str());
		}

		// Кормим данные стеку
		channel->push_sdu(
				request.cookie, request.data.data(), request.data.size(), request.qos
		);

		LOG(debug) << "accepted sdu uplink request for " << request.gmapid << ", "
				<< "cookie: " << request.cookie
		;

		// Сообщаем об этом клиенту
		sdu_uplink_event reply;
		reply.part_cookie.cookie = request.cookie;
		reply.part_cookie.part_no = 0;
		reply.part_cookie.final = true;

		reply.event_kind = sdu_uplink_event::event_kind_t::sdu_accepted;
		reply.gmapid = request.gmapid;
		_io.send_message(reply);
	}
	catch (std::exception & e)
	{
		LOG(error) << "SDU for " << request.gmapid << ", "
				<< "cookie: " << request.cookie << " "
				<< "rejected: " << e.what()
		;

		sdu_uplink_event reply;
		reply.part_cookie.cookie = request.cookie;
		reply.part_cookie.part_no = 0;
		reply.part_cookie.final = true;

		reply.event_kind = sdu_uplink_event::event_kind_t::sdu_rejected;
		reply.gmapid = request.gmapid;
		reply.comment = e.what();
		_io.send_message(reply);
	}
}


void dispatcher::_on_radio_downlink_frame(const radio_downlink_frame & frame)
{
	LOG(info) << "got radio downlink frame " << frame.frame_cookie;
	try
	{
		// Что там с контрольной суммой?
		if (!frame.checksum_valid)
			LOG(warning) << "downlink frame with invalid checksum";

		// Что там с номером пакета?
		if (_prev_rf_frame_no && *_prev_rf_frame_no + 1 != frame.frame_no)
		{
			LOG(warning) << "frame no is unexpected. "
				<< "Expected " << *_prev_rf_frame_no + 1 << ", got " << frame.frame_no;
		}
		_prev_rf_frame_no = frame.frame_no;

		// Наконец то кормим фрейм в радио
		_istack.push_frame(frame.data.data(), frame.data.size());
		LOG(debug) << "accepted radio downlink frame cookie " << frame.frame_cookie;
	}
	catch (std::exception & e)
	{
		LOG(error) << "unable to receive radio frame " << frame.frame_cookie << ": "
				<< e.what();
	}
}


void dispatcher::_on_radio_uplink_state(const radio_uplink_state & state)
{
	LOG(trace) << "got radio uplink state";
	try
	{
		// Смотрим, не ждем ли мы каких событий с уже отправленной строкой
		if (_sent_to_rf_frame)
		{
			const auto sent_frame_cookie = std::get<0>(*_sent_to_rf_frame);
			const auto sent_gmapid = std::get<1>(*_sent_to_rf_frame);
			const auto & sent_part_sdu_cookies = std::get<2>(*_sent_to_rf_frame);

			if (state.cookie_done && sent_frame_cookie == *state.cookie_done)
			{
				for (const auto & sdu_cookie: sent_part_sdu_cookies)
				{
					LOG(trace) << "radiated payload part, mapid: " << sent_gmapid << ", "
							<< "cookie: " << sdu_cookie.cookie << ", "
							<< "part: " << sdu_cookie.part_no // << " "
							<< (sdu_cookie.final ? " (final)" : "")
					;

					sdu_uplink_event event;
					event.gmapid = sent_gmapid;
					event.part_cookie = sdu_cookie;
					event.event_kind = sdu_uplink_event::event_kind_t::sdu_radiated;
					_io.send_message(event);
				}
			}
			else if (state.cookie_failed && sent_frame_cookie == *state.cookie_failed)
			{
				for (const auto & sdu_cookie: sent_part_sdu_cookies)
				{
					LOG(trace) << "radiation failed. Payload part, mapid: " << sent_gmapid << ", "
							<< "cookie: " << sdu_cookie.cookie << ", "
							<< "part: " << sdu_cookie.part_no // << " "
							<< (sdu_cookie.final ? " (final)" : "")
					;
					sdu_uplink_event event;
					event.gmapid = sent_gmapid;
					event.part_cookie = sdu_cookie;
					event.event_kind = sdu_uplink_event::event_kind_t::sdu_radiation_failed;
					_io.send_message(event);
				}
			}
			// В любом случае сбрасываем наше ожидание
			_sent_to_rf_frame = {};
		}

		// Теперь самое интересное. Смотрим можем ли мы отправлять следующий фрейм!
		if (!state.cookie_in_wait.has_value())
		{
			// Можем!
			LOG(trace) << "radio is ready to accept frame!";
			ccsds::uslp::pchannel_frame_params_t frame_params;
			const bool output_frame_ready = _ostack.peek_frame(frame_params);
			if (output_frame_ready)
			{
				LOG(trace) << "ccsds stack is ready to emit frame for "
						<< "channel " << frame_params.channel_id << ", "
						<< "ccsds frame no " << (frame_params.frame_seq_no
									? std::to_string(frame_params.frame_seq_no->value())
									: std::string("<no-frame-seq-no>")
						) //<< ", "
				; // TODO: написать тут список отправляемых кук?

				// Отправляем!
				radio_uplink_frame message;
				message.frame_cookie = _next_rf_uplink_frame_cookie;
				message.data.resize(RADIO_FRAME_SIZE);
				_ostack.pop_frame(message.data.data(), message.data.size());
				_io.send_message(message);

				// К следующему номеру радиокуки
				if (0 == ++_next_rf_uplink_frame_cookie)
					_next_rf_uplink_frame_cookie = 1; // ноль запрещен

				// Запоминаем отправленное, чтобы отслеживать его судьбу дальше
				_sent_to_rf_frame = std::make_tuple( // @suppress("Invalid arguments")
						message.frame_cookie,
						frame_params.channel_id,
						frame_params.payload_cookies
				);

				LOG(trace) << "sent frame to radio";
			}
			else
			{
				LOG(trace) << "CCSDS stack is not ready to emit frame";
			}
		}
	}
	catch (std::exception & e)
	{
		LOG(error) << "unable to process radio uplink state message: " << e.what();
	}
}
