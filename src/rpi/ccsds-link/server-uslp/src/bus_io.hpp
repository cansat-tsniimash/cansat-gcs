#ifndef ITS_SERVER_USLP_SRC_BUS_IO_HPP_
#define ITS_SERVER_USLP_SRC_BUS_IO_HPP_


#include <memory>

#include <zmq.hpp>

#include "bus_messages.hpp"


class bus_io
{
public:
	bus_io(zmq::context_t & ctx);

	void connect_bpcs(const std::string & endpoint);
	void connect_bscp(const std::string & endpoint);

	void close();

	void send_message(const bus_output_sdu_downlink & message);
	void send_message(const bus_output_sdu_event & message);
	std::unique_ptr<bus_input_message> recv_message();

	zmq::socket_t & sub_socket() { return _sub_socket; }
	zmq::socket_t & pub_socket() { return _pub_socket; }

	bool poll_sub_socket(std::chrono::milliseconds timeout);

private:
	std::unique_ptr<bus_input_sdu_uplink> recv_sdu_uplink_message();
	std::unique_ptr<bus_input_radio_downlink_frame> recv_downlink_frame_message();
	std::unique_ptr<bus_input_radio_uplink_state> recv_radio_uplink_state_message();

	zmq::context_t & _ctx;
	zmq::socket_t _sub_socket;
	zmq::socket_t _pub_socket;
};



#endif /* ITS_SERVER_USLP_SRC_BUS_IO_HPP_ */
