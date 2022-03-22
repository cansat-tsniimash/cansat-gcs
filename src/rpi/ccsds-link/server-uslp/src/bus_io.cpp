#include "bus_io.hpp"


#include "log.hpp"
#include "json.hpp"

#include <thread>

#include <ccsds/uslp/events.hpp>
#include <ccsds/epp/epp_header.hpp>


#define ITS_GBUS_TOPIC_DOWNLINK_SDU "uslp.downlink_sdu"
#define ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST "uslp.uplink_sdu_request"
#define ITS_GGUS_TOPIC_UPLINK_SDU_EVENT "uslp.uplink_sdu_event"

#define ITS_GBUS_TOPIC_UPLINK_FRAME "radio.uplink_frame"
#define ITS_GBUS_TOPIC_DOWNLINK_FRAME "radio.downlink_frame"
#define ITS_GBUS_TOPIC_UPLINK_STATE "radio.uplink_state"


namespace nlohmann {

	template <class T>
	void to_json(nlohmann::json& j, const std::optional<T>& v)
	{
		if (v.has_value())
			j = *v;
		else
			j = nullptr;
	}

	template <class T>
	void from_json(const nlohmann::json& j, std::optional<T>& v)
	{
		if (j.is_null())
			v = std::nullopt;
		else
			v = j.get<T>();
	}

} // namespace nlohmann


template <typename RESULT_TYPE>
RESULT_TYPE _get_or_die(const nlohmann::json & j, const std::string & key)
{
	auto itt = j.find(key);
	if (itt == j.end())
	{
		std::stringstream err_stream;
		err_stream << "there is no field \"" << key << "\" but it is required";
		throw std::runtime_error(err_stream.str());;
	}

	return itt->get<RESULT_TYPE>();
}


static std::string qos_to_string(ccsds::uslp::qos_t qos)
{
	if (ccsds::uslp::qos_t::EXPEDITED == qos)
		return "expedited";
	else if (ccsds::uslp::qos_t::SEQUENCE_CONTROLLED == qos)
		return "sequence_controlled";
	else
	{
		LOG_S(ERROR) << "invalid qos enum value " << static_cast<int>(qos);
		throw std::invalid_argument("invalid qos enum value");
	}
}


static ccsds::uslp::qos_t qos_from_string(const std::string & string)
{
	if ("expedited" == string)
		return ccsds::uslp::qos_t::EXPEDITED;
	else if ("sequence_controlled" == string)
		return ccsds::uslp::qos_t::SEQUENCE_CONTROLLED;
	else
	{
		LOG_S(ERROR) << "invalid qos string value \"" << string << "\"";
		throw std::invalid_argument("invalid qos string value");
	}
}


// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-


struct preparsed_message
{
	const nlohmann::json & metadata;
	const std::vector<uint8_t> && payload;
};


bus_io::bus_io(zmq::context_t & ctx)
	: _ctx(ctx), _pub_socket(), _sub_socket()
{
}


void bus_io::connect_bpcs(const std::string & endpoint)
{
	LOG_S(INFO) << "connection BPCS to \"" << endpoint << "\"";

	_sub_socket = zmq::socket_t(_ctx, zmq::socket_type::sub);
	_sub_socket.connect(endpoint);

	LOG_S(INFO) << "subscribing to topics";
	_sub_socket.set(zmq::sockopt::subscribe, ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST);
	_sub_socket.set(zmq::sockopt::subscribe, ITS_GBUS_TOPIC_DOWNLINK_FRAME);
	_sub_socket.set(zmq::sockopt::subscribe, ITS_GBUS_TOPIC_UPLINK_STATE);
}


void bus_io::connect_bscp(const std::string & endpoint)
{
	LOG_S(INFO) << "connection BSCP to \"" << endpoint << "\"";

	_pub_socket = zmq::socket_t(_ctx, zmq::socket_type::pub);
	_pub_socket.connect(endpoint);
}


void bus_io::close()
{
	_pub_socket.close();
	_sub_socket.close();
}


void bus_io::send_message(const sdu_downlink & message)
{
	LOG_S(INFO+2) << "sending 'SDU accepted' bus message";

	const std::string topic = ITS_GBUS_TOPIC_DOWNLINK_SDU;

	nlohmann::json j;
	j["sc_id"] = message.gmapid.mcid().sc_id();
	j["vchannel_id"] = message.gmapid.vchannel_id();
	j["map_id"] = message.gmapid.map_id();

	j["qos"] = qos_to_string(message.qos);

	j["incomplete"] = static_cast<bool>(message.flags & ccsds::uslp::acceptor_event_map_sdu::INCOMPLETE);
	j["idle"] = static_cast<bool>(message.flags & ccsds::uslp::acceptor_event_map_sdu::IDLE);
	j["corrupted"] = static_cast<bool>(message.flags & ccsds::uslp::acceptor_event_map_sdu::CORRUPTED);
	j["mapa"] = static_cast<bool>(message.flags & ccsds::uslp::acceptor_event_map_sdu::MAPA);
	j["mapp"] = static_cast<bool>(message.flags & ccsds::uslp::acceptor_event_map_sdu::MAPP);

	const bool is_packet = message.flags & ccsds::uslp::acceptor_event_map_sdu::MAPP;
	auto data_begin = message.data.cbegin();
	auto data_end = message.data.cend();
	if (is_packet)
	{
		ccsds::epp::header_t header;
		header.read(message.data.begin(), message.data.end());
		j["protocol_id"] = header.protocol_id;
		j["user_defined_field"] = header.user_defined_field; // optional
		j["protocol_id_extension"] = header.protocol_id_extension; // optional

		// Снимем epp заголовок с пакета
		std::advance(data_begin, header.size());
	}

	std::string metadata = j.dump();

	std::vector<uint8_t> data(data_begin, data_end);

	// Фигачим в сокет!
	_pub_socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(metadata.data(), metadata.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(data.data(), data.size()));
}


void bus_io::send_message(const sdu_uplink_event & message)
{
	LOG_S(INFO+2) << "sending 'SDU emitted' bus message";

	const std::string topic = ITS_GGUS_TOPIC_UPLINK_SDU_EVENT;

	nlohmann::json j;

	j["sc_id"] = message.gmapid.mcid().sc_id();
	j["vchannel_id"] = message.gmapid.vchannel_id();
	j["map_id"] = message.gmapid.map_id();

	auto cookie = nlohmann::json();
	cookie["cookie"] = message.part_cookie.cookie;
	cookie["part_no"] = message.part_cookie.part_no;
	cookie["is_final_part"] = message.part_cookie.final;
	j["cookie"] = std::move(cookie);

	const std::string metadata = j.dump();
	// данных тут нет.

	// В сокет!
	_pub_socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(metadata.data(), metadata.size()));
}


void bus_io::send_message(const radio_uplink_frame & message)
{
	// TODO;
}


std::unique_ptr<bus_input_message> bus_io::recv_message()
{
	zmq::message_t topic_msg;
	zmq::message_t metadata_msg;
	zmq::message_t payload_msg;

	// Гребем топик
	auto result = _sub_socket.recv(topic_msg);
	if (0 == result)
	{
		LOG_S(ERROR) << "got empty topic message";
		return nullptr;
	}

	const std::string topic(reinterpret_cast<char*>(topic_msg.data()), topic_msg.size());
	LOG_S(INFO+1) << "got msg topic \"" << topic << "\"";
	if (!topic_msg.more())
	{
		LOG_S(ERROR) << "there is no zmq message parts after topic";
		throw std::runtime_error("there is no message parts after topic");
	}

	// Гребем метаданные
	result = _sub_socket.recv(metadata_msg);
	if (0 == result)
	{
		LOG_S(ERROR) << "got empty message metadata";;
		return nullptr;
	}
	const std::string raw_metadata(reinterpret_cast<char*>(metadata_msg.data()), metadata_msg.size());
	LOG_S(INFO+2) << "raw metadata as follows " << raw_metadata;
	const auto metadata = nlohmann::json::parse(raw_metadata);

	// Гребем пейлоад, если он есть
	std::vector<uint8_t> payload;
	if (metadata_msg.more())
	{
		result = _sub_socket.recv(payload_msg);
		if (result > 0)
		{
			// Ну тут уже никак не проверяем
			const auto * data_begin = reinterpret_cast<const uint8_t*>(payload_msg.data());
			const auto * data_end = data_begin + payload_msg.size();;
			payload.assign(data_begin, data_end);
			LOG_S(INFO+2) << "got payload of size " << payload.size();
		}
		else
		{
			LOG_S(WARNING) << "message have a zero size payload";
		}
	}
	else
	{
		LOG_S(INFO+2) << "this message have no payload";
	}

	// Сливаем что там осталось
	zmq::message_t flush = std::move(payload_msg);
	while(flush.more())
	{
		LOG_S(WARNING) << "flushing extra message data";
		auto result = _sub_socket.recv(flush);
	}

	std::unique_ptr<bus_input_message> retval;
	try
	{
		if (topic == ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST)
		{
			retval = parse_sdu_uplink_request_message(preparsed_message{metadata, std::move(payload)});
			LOG_S(INFO + 1) << "got an uplink sdu request message";
		}
		else if (topic == ITS_GBUS_TOPIC_DOWNLINK_FRAME)
		{
			retval = parse_downlink_frame_message(preparsed_message{metadata, std::move(payload)});
			LOG_S(INFO + 1) << "got a downlink frame message";
		}
		else if (topic == ITS_GBUS_TOPIC_UPLINK_STATE)
		{
			retval = parse_radio_uplink_state_message(preparsed_message{metadata, std::move(payload)});
			LOG_S(INFO + 1) << "got a radio uplink state message";
		}
		else
		{
			LOG_S(ERROR) << "unknown topic received";
			retval = nullptr;
		}
	}
	catch (std::exception & e)
	{
		LOG_S(ERROR) << "unable to parse message of topic " << topic << ": " << e.what();
		retval = nullptr;
	}

	return retval;
}


bool bus_io::poll_sub_socket(std::chrono::milliseconds timeout)
{
	zmq::pollitem_t items[] = {
			{ _sub_socket, 0, ZMQ_POLLIN, 0 }
	};

	try
	{
		int rv = zmq::poll(items, sizeof(items)/sizeof(*items), timeout);
		return rv > 0;
	}
	catch (zmq::error_t & e)
	{
		if (e.num() == EINTR)
		{
			LOG_S(INFO) << "poll interrupted by syscall";
			return false;
		}

		// Все остальное кидаем выше
		throw;
	}
}


std::unique_ptr<sdu_uplink_request>
bus_io::parse_sdu_uplink_request_message(
		const preparsed_message & message
)
{
	// разгребаем выгребенное
	const nlohmann::json & j = message.metadata;
	const auto sc_id = _get_or_die<int>(j, "sc_id");
	const auto vchannel_id = _get_or_die<int>(j, "vchannel_id");
	const auto map_id = _get_or_die<int>(j, "map_id");
	const auto qos = qos_from_string(_get_or_die<std::string>(j, "qos"));
	const auto cookie = _get_or_die<ccsds::uslp::payload_cookie_t>(j, "cookie");

	// Строим само сообщение
	auto retval = std::make_unique<sdu_uplink_request>();

	retval->gmapid.sc_id(sc_id);
	retval->gmapid.vchannel_id(vchannel_id);
	retval->gmapid.map_id(map_id);
	retval->cookie = cookie;
	retval->qos = qos;

	retval->data = std::move(message.payload);

	return retval;
}


std::unique_ptr<radio_downlink_frame>
bus_io::parse_downlink_frame_message(
		const preparsed_message & message
)
{
	// Разгребаем
	const auto & j = message.metadata;
	const bool checksum_valid = _get_or_die<bool>(j, "checksum_valid");
	uint64_t cookie = _get_or_die<uint64_t>(j, "cookie");
	uint64_t frame_no = _get_or_die<uint64_t>(j, "frame_no");

	// Формируем сообщение
	auto retval = std::make_unique<radio_downlink_frame>();
	retval->checksum_valid = checksum_valid;
	retval->frame_no = frame_no;
	retval->data = std::move(message.payload);

	return retval;
}


std::unique_ptr<radio_uplink_state>
bus_io::parse_radio_uplink_state_message(
		const preparsed_message & message
)
{
	const auto & j = message.metadata;
	auto get_optional_cookie = [&j](const std::string & key) -> std::optional<uint64_t>
	{
		const auto itt = j.find(key);
		if (j.end() == itt || itt->is_null())
			return {};

		return itt->get<uint64_t>();
	};

	const auto cookie_in_wait = get_optional_cookie("cookie_in_wait");
	const auto cookie_in_progress = get_optional_cookie("cookie_in_progress");
	const auto cookie_sent = get_optional_cookie("cookie_sent");
	const auto cookie_dropped = get_optional_cookie("cookie_dropped");

	// собираем объект
	auto retval = std::make_unique<radio_uplink_state>();
	retval->cookie_in_wait = cookie_in_wait;
	retval->cookie_in_progress = cookie_in_progress;
	retval->cookie_done = cookie_sent;
	retval->cookie_failed = cookie_dropped;

	return retval;
}

