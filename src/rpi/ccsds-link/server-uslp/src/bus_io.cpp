#include "bus_io.hpp"


#include "log.hpp"
#include "json.hpp"

#include <ccsds/uslp/events.hpp>
#include <ccsds/epp/epp_header.hpp>

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


static std::string qos_to_string(ccsds::uslp::qos_t qos)
{
	if (ccsds::uslp::qos_t::EXPEDITED == qos)
		return "expedited";
	else if (ccsds::uslp::qos_t::SEQUENCE_CONTROLLED == qos)
		return "sequence_controlled";
	else
	{
		LOG_S(ERROR) << "invalid qos value " << static_cast<int>(qos);
		throw std::invalid_argument("invalid qos value");
	}
}


void bus_io::send_message(zmq::socket_t & socket, const bus_output_sdu_accepted & message)
{
	LOG_S(INFO+2) << "sending 'SDU accepted' bus message";

	const std::string topic = "uslp.sdu_accepted";

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

	std::string metadata = nlohmann::to_string(j);

	std::vector<uint8_t> data(data_begin, data_end);

	// Фигачим в сокет!
	socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	socket.send(zmq::const_buffer(metadata.data(), metadata.size()), zmq::send_flags::sndmore);
	socket.send(zmq::const_buffer(data.data(), data.size()));
}


void bus_io::send_message(zmq::socket_t & socket, const bus_output_sdu_emitted & message)
{
	LOG_S(INFO+2) << "sending 'SDU emitted' bus message";

	const std::string topic = "uslp.sdu_emitted";

	nlohmann::json j;

	j["sc_id"] = message.gmapid.mcid().sc_id();
	j["vchannel_id"] = message.gmapid.vchannel_id();
	j["map_id"] = message.gmapid.map_id();

	auto cookie = nlohmann::json();
	cookie["cookie"] = message.cookie.cookie;
	cookie["part_no"] = message.cookie.part_no;
	cookie["is_final_part"] = message.cookie.final;
	j["cookie"] = std::move(cookie);

	const std::string metadata = nlohmann::to_string(j);
	// данных тут нет.

	// В сокет!
	socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	socket.send(zmq::const_buffer(metadata.data(), metadata.size()));
}


std::unique_ptr<bus_input_message> bus_io::recv_message(zmq::socket_t & socket)
{
	return nullptr;
}

