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

	// Периодически чистим фреймы по таймауту
	_clear_frames_queue();
}


void dispatcher::_on_map_sdu_event(const ccsds::uslp::acceptor_event_map_sdu & event)
{
	std::stringstream flags_stream;

	if (event.flags & ccsds::uslp::acceptor_event_map_sdu::MAPA)
		flags_stream << "mapa, ";

	if (event.flags & ccsds::uslp::acceptor_event_map_sdu::MAPP)
		flags_stream << "mapp, ";

	if (event.flags & ccsds::uslp::acceptor_event_map_sdu::INCOMPLETE)
		flags_stream << "incomplete, ";

	if (event.flags & ccsds::uslp::acceptor_event_map_sdu::IDLE)
		flags_stream << "idle, ";

	if (event.flags & ccsds::uslp::acceptor_event_map_sdu::CORRUPTED)
		flags_stream << "corrupted, ";

	if (event.flags & ccsds::uslp::acceptor_event_map_sdu::STRAY)
		flags_stream << "stray, ";


	auto flags_string = flags_stream.str();
	if (flags_string.size())
		flags_string.resize(flags_string.size()-2); // Откусываем ", " с хвоста

	LOG(info) << "got downlink map SDU " << event.channel_id << " "
			<< event.data.size() << " bytes"
			<< (flags_string.size() ? ("; " + flags_string) : (""))
	;

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
	LOG(debug) << "got SDU uplink request for " << request.gmapid << ", "
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

		LOG(info) << "accepted SDU uplink " << request.gmapid << ", "
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
	LOG(trace) << "got radio downlink frame " << frame.frame_cookie;
	try
	{
		// Что там с контрольной суммой?
		if (!frame.checksum_valid)
			LOG(warning) << "downlink frame with invalid checksum";

		// Наконец то кормим фрейм в стек
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
		_decide_next_uplink_frame(state);
	}
	catch (std::exception & e)
	{
		LOG(error) << "unable to process radio uplink state message: " << e.what();
	}
}


void dispatcher::_clear_frames_queue()
{
	// Здесь мы будем чистить фреймы с которыми случился таймаут
	auto itt = _frames_in_wait.begin();
	while (itt != _frames_in_wait.end())
	{
		auto & finfo = *itt;
		const auto now = std::chrono::steady_clock::now();
		if (finfo.send_time + _frame_done_timeout > now)
		{
			// все ок, пропускаем
			itt = std::next(itt, 1);
			continue;
		}

		for (const auto & sdu_cookie: finfo.sdu_cookies)
		{
			LOG(error) << "payload part timed out: " << finfo.sdu_mapid << ", "
					<< "cookie: " << sdu_cookie.cookie << ", "
					<< "part: " << sdu_cookie.part_no // << " "
					<< (sdu_cookie.final ? " (final)" : "")
			;

			sdu_uplink_event event;
			event.gmapid = finfo.sdu_mapid;
			event.part_cookie = sdu_cookie;
			event.event_kind = sdu_uplink_event::event_kind_t::sdu_radiation_failed;
			_io.send_message(event);
		}

		// Удаляем эту фрейм из очереди
		itt = _frames_in_wait.erase(itt);
	}
}


void dispatcher::_update_frames_queue(const radio_uplink_state & state)
{
	// Пробегаем каждый фрейм, за которым мы следим и смотрим что там с ним происходит
	auto itt = _frames_in_wait.begin();
	while(itt != _frames_in_wait.end())
	{
		auto & finfo = *itt;
		if (state.cookie_in_wait && *state.cookie_in_wait == finfo.frame_cookie)
		{
			if (finfo.state != frame_queue_entry_t::frame_state_t::in_wait)
				LOG(debug) << "frame " << finfo.frame_cookie << " went to 'in_wait'";

			finfo.state = frame_queue_entry_t::frame_state_t::in_wait;
		}
		if (state.cookie_in_progress && *state.cookie_in_progress == finfo.frame_cookie)
		{
			if (finfo.state != frame_queue_entry_t::frame_state_t::in_wait)
				LOG(debug) << "frame " << finfo.frame_cookie << " went to 'in_progress";

			finfo.state = frame_queue_entry_t::frame_state_t::in_progress;
		}
		if (state.cookie_done && *state.cookie_done == finfo.frame_cookie)
		{
			LOG(debug) << "frame " << finfo.frame_cookie << " is radiated";
			finfo.state = frame_queue_entry_t::frame_state_t::radiated;

			for (const auto & sdu_cookie: finfo.sdu_cookies)
			{
				LOG(info) << "radiated payload part: " << finfo.sdu_mapid << ", "
						<< "cookie: " << sdu_cookie.cookie << ", "
						<< "part: " << sdu_cookie.part_no // << " "
						<< (sdu_cookie.final ? " (final)" : "")
				;

				sdu_uplink_event event;
				event.gmapid = finfo.sdu_mapid;
				event.part_cookie = sdu_cookie;
				event.event_kind = sdu_uplink_event::event_kind_t::sdu_radiated;
				_io.send_message(event);
			}
		}
		if (state.cookie_failed && *state.cookie_failed == finfo.frame_cookie)
		{
			LOG(error) << "frame " << finfo.frame_cookie << " radiation failed";
			finfo.state = frame_queue_entry_t::frame_state_t::failed;

			for (const auto & sdu_cookie: finfo.sdu_cookies)
			{
				LOG(error) << "payload part radiation failed: " << finfo.sdu_mapid << ", "
						<< "cookie: " << sdu_cookie.cookie << ", "
						<< "part: " << sdu_cookie.part_no // << " "
						<< (sdu_cookie.final ? " (final)" : "")
				;

				sdu_uplink_event event;
				event.gmapid = finfo.sdu_mapid;
				event.part_cookie = sdu_cookie;
				event.event_kind = sdu_uplink_event::event_kind_t::sdu_radiation_failed;
				_io.send_message(event);
			}
		}

		// Чистим фреймы, которым в очереди не место
		switch (finfo.state)
		{
		case frame_queue_entry_t::frame_state_t::failed:
		case frame_queue_entry_t::frame_state_t::radiated:
			itt = _frames_in_wait.erase(itt);
			break;

		default:
			itt = std::next(itt, 1);
			break;
		};
	}
}


void dispatcher::_decide_next_uplink_frame(const radio_uplink_state & state)
{
	// Сбрасываем фреймы по таймауту
	_clear_frames_queue();
	// Разгребаем что там нам пишло
	_update_frames_queue(state);

	// Принимаем решение об отправке следующего фрейма
	if (state.cookie_in_wait.has_value())
	{
		// Радио сообщает что его выходной буфер не свободен
		// Тут наши полномочия все
		LOG(trace) << "radio is not ready for uplink frame";
		return;
	}

	// смотрим нет ли среди ожидающих отправки фреймов кого-то
	// кто с нашей точки зрения должен лежать в отправном буфере
	// или находится по пути туда
	bool can_send = true;
	for (const auto & finfo: _frames_in_wait)
	{
		switch (finfo.state)
		{
		case frame_queue_entry_t::frame_state_t::sent_to_radio:
		case frame_queue_entry_t::frame_state_t::in_wait:
			can_send = false;
			goto breakout;
		}
	}
breakout:
	if (!can_send)
	{
		LOG(trace) << "radio is ready for next uplink frame, but it should not be";
		return;
	}

	LOG(trace) << "radio is ready to accept frame!";
	// Мы можем отправлять. Но хотим ли?
	ccsds::uslp::pchannel_frame_params_t frame_params;
	const bool output_frame_ready = _ostack.peek_frame(frame_params);
	if (!output_frame_ready)
	{
		LOG(trace) << "CCSDS stack is not ready to emit frame";
		return;
	}

	LOG(debug) << "ccsds stack is ready to emit frame for "
			<< "channel " << frame_params.channel_id << ", "
			<< "ccsds frame no " << (frame_params.frame_seq_no
						? std::to_string(frame_params.frame_seq_no->value())
						: std::string("<no-frame-seq-no>")
				)
	;

	// Отправляем!
	radio_uplink_frame message;
	message.frame_cookie = _next_rf_uplink_frame_cookie;
	message.data.resize(RADIO_FRAME_SIZE);
	_ostack.pop_frame(message.data.data(), message.data.size());
	_io.send_message(message);

	// К следующему номеру радиокуки
	if (0 == ++_next_rf_uplink_frame_cookie)
		_next_rf_uplink_frame_cookie = 1; // ноль запрещен

	// Запоминаем фрейм
	_frames_in_wait.push_back(frame_queue_entry_t{
		message.frame_cookie,
		frame_params.channel_id,
		frame_params.payload_cookies,
		std::chrono::steady_clock::now(),
		frame_queue_entry_t::frame_state_t::sent_to_radio
	});

	// готово
}

