#ifndef ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_
#define ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_


#include <vector>

#include <zmq.hpp>


struct downlink_packet
{
	bool bad;
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

	void set_uplink_channel(int sc_id, int vc_id, int map_id);
	void set_downlink_channel(int sc_id, int vc_id, int map_id);
	void attach_to_context(zmq::context_t * ctx);

	void open();
	void open(const std::string & bpcs_endpoint, const std::string & bscp_endpoint);
	void close();

	void recv_downlink_packet(downlink_packet & packet);
	void send_uplink_packet(const uplink_packet & packet);

	zmq::socket_t & bpcs_socket() { return _bpcs_socket; }
	zmq::socket_t & bscp_socket() { return _bscp_socket; }

private:
	int _uplink_sc_id = 0;
	int _uplink_vc_id = 0;
	int _uplink_map_id = 0;

	int _downlink_sc_id = 0;
	int _downlink_vc_id = 0;
	int _downlink_map_id = 0;

	uint64_t _uplink_cookie = 0;

	zmq::context_t * _ctx;

	zmq::socket_t _bpcs_socket;
	zmq::socket_t _bscp_socket;
};


inline void swap(zmq_server & left, zmq_server & right) { left.swap(right); }


#endif /* ITS_SERVER_TUN_SRC_ZMQ_SERVER_HPP_ */
