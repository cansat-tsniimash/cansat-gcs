#include "zmq_server.hpp"

#include <cassert>
#include <cstdlib>

#include <json.hpp>

#include <ccsds/epp/epp_header.hpp>

#include "log.hpp"


static auto _slg = build_source("zmq-server");


#define ITS_BSCP_ENDPOINT_KEY "ITS_GBUS_BSCP_ENDPOINT"
#define ITS_BPCS_ENDPOINT_KEY "ITS_GBUS_BPCS_ENDPOINT"

#define ITS_GBUS_TOPIC_DOWNLINK_SDU "uslp.downlink_sdu"
#define ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST "uslp.uplink_sdu_request"
#define ITS_GBUS_TOPIC_UPLINK_SDU_EVENT "uslp.uplink_sdu_event"


zmq_server::zmq_server()
	: _ctx(nullptr)
{}


zmq_server::zmq_server(zmq::context_t * ctx)
	: _ctx(ctx)
{

}


zmq_server::zmq_server(zmq_server && other)
	: _ctx(other._ctx),
	  _bpcs_socket(std::move(other._bpcs_socket)),
	  _bscp_socket(std::move(other._bscp_socket))
{

}


zmq_server & zmq_server::operator=(zmq_server && other)
{
	zmq_server tmp(std::move(other));
	this->swap(tmp);
	return *this;
}


zmq_server::~zmq_server()
{
	close();
}


void zmq_server::swap(zmq_server & other)
{
	std::swap(_ctx, other._ctx);
	_bpcs_socket.swap(other._bpcs_socket);
	_bscp_socket.swap(other._bscp_socket);
}


void zmq_server::set_uplink_channel(int sc_id, int vc_id, int map_id)
{
	_uplink_sc_id = sc_id;
	_uplink_vc_id = vc_id;
	_uplink_map_id = map_id;

	LOG(info) << "using uplink channel " << sc_id << "," << vc_id << "," << map_id;
}


void zmq_server::set_downlink_channel(int sc_id, int vc_id, int map_id)
{
	_downlink_sc_id = sc_id;
	_downlink_vc_id = vc_id;
	_downlink_map_id = map_id;

	LOG(info) << "using downlink channel " << sc_id << "," << vc_id << "," << map_id;
}


void zmq_server::attach_to_context(zmq::context_t * ctx)
{
	if (_bpcs_socket.handle() != ZMQ_NULLPTR || _bscp_socket.handle() != ZMQ_NULLPTR)
		throw std::runtime_error("unable to rebind context when socket are opened");

	_ctx = ctx;
}

void zmq_server::open()
{
	const char * bscp = std::getenv(ITS_BSCP_ENDPOINT_KEY);
	const char * bpcs = std::getenv(ITS_BPCS_ENDPOINT_KEY);

	if (bscp == nullptr)
	{
		LOG(error) << "unable to fetch BSCP endpoint address name from envvar "
				<< ITS_BPCS_ENDPOINT_KEY;

		throw std::runtime_error("unable to fetch BSCP endpoint address");
	}

	if (bpcs == nullptr)
	{
		LOG(error) << "unable to fetch BPCS endpoint address name from envvar "
				<< ITS_BSCP_ENDPOINT_KEY;

		throw std::runtime_error("unable to fetch BPCS endpoint address");
	}

	open(bpcs, bscp);
}


void zmq_server::open(const std::string & bpcs_endpoint, const std::string & bscp_endpoint)
{
	assert(_ctx != nullptr);

	_bpcs_socket = zmq::socket_t(*_ctx, ZMQ_SUB);
	LOG(info) << "connecting BPCS socket to " << bpcs_endpoint;
	_bpcs_socket.connect(bpcs_endpoint.c_str());

	_bscp_socket = zmq::socket_t(*_ctx, ZMQ_PUB);
	LOG(info) << "connecting BSCP socket to " << bscp_endpoint;
	_bscp_socket.connect(bscp_endpoint.c_str());

	// Подписываемся на единственное интересное нам сообщение
	std::stringstream topic_stream;
	topic_stream << ITS_GBUS_TOPIC_DOWNLINK_SDU << "."
			<< _downlink_sc_id << "."
			<< _downlink_vc_id << "."
			<< _downlink_map_id
	;
	const std::string topic = topic_stream.str();
	LOG(info) << "subscribing to \"" << topic << "\"";
	_bpcs_socket.set(zmq::sockopt::subscribe, topic);
}


void zmq_server::close()
{
	_bpcs_socket.close();
	_bscp_socket.close();
}


void zmq_server::recv_downlink_packet(downlink_packet & packet)
{
	zmq::message_t topic_msg;
	zmq::message_t meta_msg;
	zmq::message_t data_msg;
	bool bad_packet = false;

	auto rv = _bpcs_socket.recv(topic_msg);
	if (!topic_msg.more())
		throw std::runtime_error("there is no 'more' messages after topic");

	rv = _bpcs_socket.recv(meta_msg, zmq::recv_flags::dontwait);
	if (!meta_msg.more())
		throw std::runtime_error("there is no 'more' messages after metadata");

	rv = _bpcs_socket.recv(data_msg, zmq::recv_flags::dontwait);
	if (data_msg.more())
	{
		zmq::message_t flusher;
		do
		{
			LOG(warning) << "there is unexpected 'more' parts of zmq message";
			rv = _bpcs_socket.recv(flusher);
		} while (flusher.more());
	}

	// работаем с топиком
	// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	const char * topic_data_begin = reinterpret_cast<const char*>(topic_msg.data());
	const std::string_view topic(topic_data_begin, topic_msg.size());

	LOG(debug) << "got message with topic \"" << topic << "\"";
	const std::string_view desired_topic(ITS_GBUS_TOPIC_DOWNLINK_SDU);
	if (topic.size() < desired_topic.size())
	{
		LOG(error) << "unexpected topic: \"" << topic << "\"";
		throw std::runtime_error("unexpected message topic");
	}

	const auto topic_in_size_of_desired = topic.substr(0, desired_topic.size());
	if (desired_topic != topic_in_size_of_desired)
	{
		LOG(error) << "got unexpected topic \"" << topic << "\"";
		throw std::runtime_error("unexpected message topic");
	}

	// работаем с метаданными
	// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	const char * meta_data_begin = reinterpret_cast<const char*>(meta_msg.data());
	const std::string_view meta(meta_data_begin, meta_msg.size());
	const auto j = nlohmann::json::parse(meta);

	// Проверяем что это пакет для нас
	//   Доверяем zmq в наших подписках

	// Провеяем флаги
	const char * bad_flags[] = { "idle", "corrupted", "incomplete", "stray" };
	for (const nlohmann::json & flag: j["flags"])
	{
		for (const char * baddie: bad_flags)
		{
			if (baddie == flag.get<std::string_view>())
			{
				LOG(info) << "sdu got \"" << baddie << "\" flag";
				bad_packet = true;
				goto breakout;
			}
		}
	}
breakout:

	// работаем собственно с данными
	// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
	const uint8_t * data_begin = reinterpret_cast<const uint8_t*>(data_msg.data());
	const uint8_t * data_end = data_begin + data_msg.size();

	// Теперь скручиваем EPP заголовки
	ccsds::epp::header_t header;

	const auto header_size = ccsds::epp::header_t::probe_header_size(data_begin[0]);
	if (header_size > 0)
	{
		// Заглянем в заголовок
		ccsds::epp::header_t header;
		header.read(data_begin, data_end - data_begin);

		LOG(info) << "got a EPP packet PID " << static_cast<int>(header.protocol_id) << " "
				<< "of size " << header.payload_size()
		;

		// Сдвигаем указатель за заголовок
		std::advance(data_begin, header.size());
		// Проверим размер на всякий
		if (header.payload_size() != (data_end - data_begin))
			LOG(warning) << "bad size in epp packet header";
	}
	else
	{
		LOG(warning) << "got packet with bad epp header";
		bad_packet = true;
	}

	packet.bad = bad_packet;
	packet.data = std::vector<uint8_t>(data_begin, data_end);
}


void zmq_server::send_uplink_packet(const uplink_packet & packet)
{
	std::stringstream topic_stream;
	topic_stream << ITS_GBUS_TOPIC_UPLINK_SDU_REQUEST << "."
			<< _uplink_sc_id << "."
			<< _uplink_vc_id << "."
			<< _uplink_map_id
	;
	const std::string topic = topic_stream.str();

	nlohmann::json j;
	j["sc_id"] = _uplink_sc_id;
	j["vchannel_id"] = _uplink_vc_id;
	j["map_id"] = _uplink_map_id;
	j["qos"] = "expedited";
	j["cookie"] = _uplink_cookie++;

	// Дополнительная информация
	j["extra"] = {
		{ "proto", packet.proto },
		{ "flags", packet.flags }
	};
	const std::string metadata = j.dump();

	// Дорисовываем epp заголовок
	ccsds::epp::header_t header;
	header.protocol_id = static_cast<int>(ccsds::epp::protocol_id_t::IPE);
	header.accomadate_to_payload_size(packet.data.size());

	std::vector<uint8_t> data(header.size());
	header.write(data.begin(), data.end());
	data.insert(data.end(), packet.data.begin(), packet.data.end());

	LOG(info) << "sending uplink SDU cookie " << j["cookie"] << " "
			<< "of size " << header.payload_size();

	_bscp_socket.send(zmq::const_buffer(topic.data(), topic.size()), zmq::send_flags::sndmore);
	_bscp_socket.send(zmq::const_buffer(metadata.data(), metadata.size()), zmq::send_flags::sndmore);
	_bscp_socket.send(zmq::const_buffer(data.data(), data.size()));
}

