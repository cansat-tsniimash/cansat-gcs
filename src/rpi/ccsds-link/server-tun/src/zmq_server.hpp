#ifndef ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_
#define ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_


#include <vector>

#include <zmq.hpp>


struct downlink_packet
{
	std::vector<uint8_t> data;
};


struct uplink_packet
{
	uint32_t proto;
	uint32_t flags;
	std::vector<uint8_t> data;
};


class zmq_server
{
public:
	zmq_server();
	zmq_server(zmq::context_t * ctx);
	zmq_server(const zmq_server & other) = delete;
	zmq_server & operator=(const zmq_server && other) = delete;
	zmq_server(zmq_server && other);
	zmq_server & operator=(zmq_server && other);
	~zmq_server();

	void swap(zmq_server & other);

	void attach_to_context(zmq::context_t * ctx);

	void open();
	void open(const std::string & bpcs_endpoint, const std::string & bscp_endpoint);
	void close();

	void pop_downlink_packet(downlink_packet packet);
	void push_uplink_packet(uplink_packet & packet);

	void * bpcs_handle() { return _bpcs_socket.handle(); }
	void * bscp_handle() { return _bscp_socket.handle(); }

protected:
	void subscribe(const std::string & topic);
	void unsubscribe(const std::string & topic);

private:
	zmq::context_t * _ctx;

	zmq::socket_t _bpcs_socket;
	zmq::socket_t _bscp_socket;
};


inline void swap(zmq_server & left, zmq_server & right) { left.swap(right); }


#endif /* ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_ */
