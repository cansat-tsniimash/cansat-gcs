#ifndef ITS_SERVER_USLP_SRC_BUS_IO_HPP_
#define ITS_SERVER_USLP_SRC_BUS_IO_HPP_


#include <memory>

#include <zmq.hpp>

#include "bus_messages.hpp"


struct preparsed_message;


class bus_io
{
public:
	bus_io(zmq::context_t & ctx);

	void connect_bpcs(const std::string & endpoint);
	void connect_bscp(const std::string & endpoint);

	void close();

	void send_message(const sdu_downlink & message);
	void send_message(const sdu_uplink_event & message);
	void send_message(const radio_uplink_frame & message);

	std::unique_ptr<bus_input_message> recv_message();

	zmq::socket_t & sub_socket() { return _sub_socket; }
	zmq::socket_t & pub_socket() { return _pub_socket; }

	bool poll_sub_socket(std::chrono::milliseconds timeout);

private:
	std::unique_ptr<sdu_uplink_request> parse_sdu_uplink_request_message(
			std::string_view topic,
			const preparsed_message & message
	);
	std::unique_ptr<radio_downlink_frame> parse_downlink_frame_message(
			const preparsed_message & message
	);
	std::unique_ptr<radio_uplink_state> parse_radio_uplink_state_message(
			const preparsed_message & message
	);

	zmq::context_t & _ctx;
	zmq::socket_t _sub_socket;
	zmq::socket_t _pub_socket;
};



#endif /* ITS_SERVER_USLP_SRC_BUS_IO_HPP_ */
