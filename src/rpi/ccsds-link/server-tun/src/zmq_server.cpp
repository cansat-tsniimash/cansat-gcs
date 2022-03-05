#include "zmq_server.hpp"

#include <cassert>
#include <cstdlib>

#define LOGURU_WITH_STREAMS 1
#include <loguru.hpp>

#define ITS_BSCP_ENDPOINT_KEY "ITS_GBUS_BSCP_ENDPOINT"
#define ITS_BPCS_ENDPOINT_KEY "ITS_GBUS_BPCS_ENDPOINT"



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
		LOG_S(ERROR) << "unable to fetch BSCP endpoint address name from envvar "
				<< ITS_BPCS_ENDPOINT_KEY;

		throw std::runtime_error("unable to fetch BSCP endpoint address");
	}

	if (bpcs == nullptr)
	{
		LOG_S(ERROR) << "unable to fetch BPCS endpoint address name from envvar "
				<< ITS_BSCP_ENDPOINT_KEY;

		throw std::runtime_error("unable to fetch BPCS endpoint address");
	}

	open(bpcs, bscp);
}


void zmq_server::open(const std::string & bpcs_endpoint, const std::string & bscp_endpoint)
{
	assert(_ctx != nullptr);

	_bpcs_socket = zmq::socket_t(*_ctx, ZMQ_SUB);
	LOG_S(INFO) << "connecting BPCS socket to " << bpcs_endpoint;
	_bpcs_socket.connect(bpcs_endpoint.c_str());

	_bscp_socket = zmq::socket_t(*_ctx, ZMQ_PUB);
	LOG_S(INFO) << "connecting BSCP socket to " << bscp_endpoint;
	_bscp_socket.connect(bscp_endpoint.c_str());
}


void zmq_server::subscribe(const std::string & topic)
{
	LOG_S(INFO) << "subscribing for \"" << topic << "\"";
	_bpcs_socket.set(zmq::sockopt::subscribe, topic);
}


void zmq_server::unsubscribe(const std::string & topic)
{
	LOG_S(INFO) << "unsubscribing from \"" << topic << "\"";
	_bpcs_socket.set(zmq::sockopt::unsubscribe, topic);
}


void zmq_server::close()
{
	_bpcs_socket.close();
	_bscp_socket.close();
}
