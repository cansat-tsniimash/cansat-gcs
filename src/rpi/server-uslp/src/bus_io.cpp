#include "bus_io.hpp"


#include "log.hpp"
#include "json.hpp"

#include <thread>

#include <boost/algorithm/string.hpp>

#include <ccsds/uslp/events.hpp>
#include <ccsds/uslp/common/ids_io.hpp>
#include <ccsds/epp/epp_header.hpp>

static auto _slg = build_source("bus-io");


#define ITS_GBUS_TOPIC_DOWNLINK_SDU "uslp.downlink_sdu"
#define ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST "uslp.uplink_sdu_request"
#define ITS_GBUS_TOPIC_UPLINK_SDU_EVENT "uslp.uplink_sdu_event"

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


static std::vector<std::string> _downlink_sdu_flags_to_string(uint64_t flags)
{
	using ccsds::uslp::acceptor_event_map_sdu;

	static const std::array<
		std::tuple<acceptor_event_map_sdu::data_flags_t, std::string>,
		6
	> possible_values = { // @suppress("Invalid arguments")
			std::make_tuple(acceptor_event_map_sdu::INCOMPLETE, "incomplete"), // @suppress("Invalid arguments")
			std::make_tuple(acceptor_event_map_sdu::IDLE, "idle"),  // @suppress("Invalid arguments")
			std::make_tuple(acceptor_event_map_sdu::CORRUPTED, "corrupted"),  // @suppress("Invalid arguments")
			std::make_tuple(acceptor_event_map_sdu::MAPA, "mapa"),  // @suppress("Invalid arguments")
			std::make_tuple(acceptor_event_map_sdu::MAPP, "mapp"),  // @suppress("Invalid arguments")
			std::make_tuple(acceptor_event_map_sdu::STRAY, "stray"),  // @suppress("Invalid arguments")
	};

	std::vector<std::string> retval;
	for (const auto & value: possible_values)
	{
		if (flags & std::get<0>(value))
			retval.push_back(std::get<1>(value));
	}

	return retval;
}


static std::string _qos_to_string(ccsds::uslp::qos_t qos)
{
	if (ccsds::uslp::qos_t::EXPEDITED == qos)
		return "expedited";
	else if (ccsds::uslp::qos_t::SEQUENCE_CONTROLLED == qos)
		return "sequence_controlled";
	else
	{
		LOG(error) << "invalid qos enum value " << static_cast<int>(qos);
		throw std::invalid_argument("invalid qos enum value");
	}
}


static ccsds::uslp::qos_t _qos_from_string(const std::string & string)
{
	if ("expedited" == string)
		return ccsds::uslp::qos_t::EXPEDITED;
	else if ("sequence_controlled" == string)
		return ccsds::uslp::qos_t::SEQUENCE_CONTROLLED;
	else
	{
		LOG(error) << "invalid qos string value \"" << string << "\"";
		throw std::invalid_argument("invalid qos string value");
	}
}


static std::string _uplink_sdu_event_kind_to_string(sdu_uplink_event::event_kind_t kind)
{
	switch (kind)
	{
	case sdu_uplink_event::event_kind_t::sdu_accepted: return "sdu_accepted";
	case sdu_uplink_event::event_kind_t::sdu_rejected: return "sdu_rejected";
	case sdu_uplink_event::event_kind_t::sdu_sent_to_radio: return "sdu_sent_to_radio";
	case sdu_uplink_event::event_kind_t::sdu_radiated: return "sdu_radiated";
	case sdu_uplink_event::event_kind_t::sdu_radiation_failed: return "sdu_radiation_failed";
	};

	std::stringstream error;
	error << "invalid SDU uplink event kind: \"" << std::to_string(static_cast<int>(kind)) << "\"";
	throw std::invalid_argument(error.str());
}


static std::string _channel_id_to_topic(const std::string & topic_base, ccsds::uslp::gmapid_t ch_id)
{
	std::stringstream stream;
	stream << topic_base << "."
		<< static_cast<int>(ch_id.sc_id()) << "."
		<< static_cast<int>(ch_id.vchannel_id()) << "."
		<< static_cast<int>(ch_id.map_id())
	;
	return stream.str();
}


static ccsds::uslp::gmapid_t _channel_id_from_topic(const std::string_view topic)
{
	std::vector<std::string> parts;
	boost::algorithm::split(parts, topic, boost::is_any_of("."), boost::token_compress_on);

	if (parts.size() < 3)
		throw std::invalid_argument("bad topic for channel id extraction: " + std::string(topic));

	try
	{
		const int sc_id = std::stoi(parts[parts.size()-3], 0, 0);
		const int vc_id = std::stoi(parts[parts.size()-2], 0, 0);
		const int map_id = std::stoi(parts[parts.size()-1], 0, 0);
		return ccsds::uslp::gmapid_t(sc_id, vc_id, map_id);
	}
	catch (std::exception & e)
	{
		std::throw_with_nested(
				std::invalid_argument("bad topic for channel id extraction: " + std::string(topic))
		);
	}
}

bool _starts_with(std::string_view left, std::string_view right)
{
	if (left.size() < right.size())
		return false;

	const std::string_view left_in_size_of_right = left.substr(0, right.size());
	return left_in_size_of_right == right;
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
	_sub_socket = zmq::socket_t(_ctx, zmq::socket_type::sub);

	LOG(info) << "connecting BPCS to \"" << endpoint << "\"";
	_sub_socket.connect(endpoint);

	LOG(debug) << "subscribing to topics";
	_sub_socket.set(zmq::sockopt::subscribe, ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST);
	_sub_socket.set(zmq::sockopt::subscribe, ITS_GBUS_TOPIC_DOWNLINK_FRAME);
	_sub_socket.set(zmq::sockopt::subscribe, ITS_GBUS_TOPIC_UPLINK_STATE);
}


void bus_io::connect_bscp(const std::string & endpoint)
{
	LOG(info) << "connection BSCP to \"" << endpoint << "\"";

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
	LOG(trace) << "sending 'SDU accepted' bus message";

	const std::string topic = _channel_id_to_topic(ITS_GBUS_TOPIC_DOWNLINK_SDU, message.gmapid);

	nlohmann::json j;
	j["sc_id"] = message.gmapid.mcid().sc_id();
	j["vchannel_id"] = message.gmapid.vchannel_id();
	j["map_id"] = message.gmapid.map_id();

	j["qos"] = _qos_to_string(message.qos);
	j["flags"] = _downlink_sdu_flags_to_string(message.flags);

	std::string metadata = j.dump();

	const std::vector<uint8_t> & data = message.data;

	// Фигачим в сокет!
	_pub_socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(metadata.data(), metadata.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(data.data(), data.size()));
}


void bus_io::send_message(const sdu_uplink_event & message)
{
	LOG(trace) << "sending uplink sdu event bus message";

	const std::string topic = _channel_id_to_topic(ITS_GBUS_TOPIC_UPLINK_SDU_EVENT, message.gmapid);

	nlohmann::json j;
	j["sc_id"] = message.gmapid.mcid().sc_id();
	j["vchannel_id"] = message.gmapid.vchannel_id();
	j["map_id"] = message.gmapid.map_id();

	auto cookie = nlohmann::json();
	cookie["cookie"] = message.part_cookie.cookie;
	cookie["part_no"] = message.part_cookie.part_no;
	cookie["is_final_part"] = message.part_cookie.final;
	j["cookie"] = std::move(cookie);
	j["event"] = _uplink_sdu_event_kind_to_string(message.event_kind);
	j["comment"] = message.comment;

	const std::string metadata = j.dump();
	// данных тут нет.

	// В сокет!
	_pub_socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(metadata.data(), metadata.size()));
}


void bus_io::send_message(const radio_uplink_frame & message)
{
	LOG(trace) << "sending uplink frame bus message";

	const std::string topic = ITS_GBUS_TOPIC_UPLINK_FRAME;

	nlohmann::json j;
	j["cookie"] = message.frame_cookie;
	const std::string metadata = j.dump();

	// В сокет!
	_pub_socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(metadata.data(), metadata.size()), zmq::send_flags::sndmore);
	_pub_socket.send(zmq::const_buffer(message.data.data(), message.data.size()));
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
		LOG(error) << "got empty topic message";
		return nullptr;
	}

	const std::string topic(reinterpret_cast<char*>(topic_msg.data()), topic_msg.size());
	LOG(trace) << "got msg topic \"" << topic << "\"";
	if (!topic_msg.more())
	{
		LOG(error) << "there is no zmq message parts after topic";
		throw std::runtime_error("there is no message parts after topic");
	}

	// Гребем метаданные
	result = _sub_socket.recv(metadata_msg);
	if (0 == result)
	{
		LOG(error) << "got empty message metadata";;
		return nullptr;
	}
	const std::string raw_metadata(reinterpret_cast<char*>(metadata_msg.data()), metadata_msg.size());
	LOG(trace) << "raw metadata as follows " << raw_metadata;
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
			LOG(trace) << "got payload of size " << payload.size();
		}
		else
		{
			LOG(warning) << "message have a zero size payload";
		}
	}
	else
	{
		LOG(trace) << "this message have no payload";
	}

	// Сливаем что там осталось
	zmq::message_t flush = std::move(payload_msg);
	while(flush.more())
	{
		LOG(warning) << "flushing extra message data";
		auto result = _sub_socket.recv(flush);
	}

	std::unique_ptr<bus_input_message> retval;
	try
	{
		if (_starts_with(topic, ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST))
		{
			retval = parse_sdu_uplink_request_message(topic, preparsed_message{metadata, std::move(payload)});
			LOG(debug) << "got an uplink sdu request message";
		}
		else if (topic == ITS_GBUS_TOPIC_DOWNLINK_FRAME)
		{
			retval = parse_downlink_frame_message(preparsed_message{metadata, std::move(payload)});
			LOG(debug) << "got a downlink frame message";
		}
		else if (topic == ITS_GBUS_TOPIC_UPLINK_STATE)
		{
			retval = parse_radio_uplink_state_message(preparsed_message{metadata, std::move(payload)});
			LOG(trace) << "got a radio uplink state message";
		}
		else
		{
			LOG(error) << "unknown topic received";
			retval = nullptr;
		}
	}
	catch (std::exception & e)
	{
		LOG(error) << "unable to parse message of topic " << topic << ": " << e.what();
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
			LOG(info) << "poll interrupted by syscall";
			return false;
		}

		// Все остальное кидаем выше
		throw;
	}
}


std::unique_ptr<sdu_uplink_request>
bus_io::parse_sdu_uplink_request_message(
		std::string_view topic,
		const preparsed_message & message
)
{
	const ccsds::uslp::gmapid_t topic_ch_id = _channel_id_from_topic(topic);

	// разгребаем выгребенное
	const nlohmann::json & j = message.metadata;
	const auto sc_id = _get_or_die<int>(j, "sc_id");
	const auto vchannel_id = _get_or_die<int>(j, "vchannel_id");
	const auto map_id = _get_or_die<int>(j, "map_id");
	const auto qos = _qos_from_string(_get_or_die<std::string>(j, "qos"));
	const auto cookie = _get_or_die<ccsds::uslp::payload_cookie_t>(j, "cookie");

	// Строим само сообщение
	auto retval = std::make_unique<sdu_uplink_request>();

	retval->gmapid.sc_id(sc_id);
	retval->gmapid.vchannel_id(vchannel_id);
	retval->gmapid.map_id(map_id);
	retval->cookie = cookie;
	retval->qos = qos;

	retval->data = std::move(message.payload);

	if (topic_ch_id != retval->gmapid)
	{
		LOG(warning) << "channel id missmatch for sdu uplink request. "
				<< "in topic: " << topic_ch_id << "; in metadata: " << retval->gmapid << " "
				<< "assuming right one in metadata"
		;
	}

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
	const uint64_t frame_no = _get_or_die<uint64_t>(j, "frame_no");
	const auto cookie = _get_or_die<ccsds::uslp::payload_cookie_t >(j, "cookie");

	// Формируем сообщение
	auto retval = std::make_unique<radio_downlink_frame>();
	retval->checksum_valid = checksum_valid;
	retval->frame_no = frame_no;
	retval->data = std::move(message.payload);
	retval->frame_cookie = cookie;

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

