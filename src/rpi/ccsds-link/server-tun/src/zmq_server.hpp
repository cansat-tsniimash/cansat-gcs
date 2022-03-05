#ifndef ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_
#define ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_

#include <zmq.hpp>


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
	void subscribe(const std::string & topic);
	void unsubscribe(const std::string & topic);
	void close();

	void * bpcs_handle() { return _bpcs_socket.handle(); }
	void * bscp_handle() { return _bscp_socket.handle(); }

private:
	zmq::context_t * _ctx;

	zmq::socket_t _bpcs_socket;
	zmq::socket_t _bscp_socket;
};


inline void swap(zmq_server & left, zmq_server & right) { left.swap(right); }


#endif /* ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_ */
