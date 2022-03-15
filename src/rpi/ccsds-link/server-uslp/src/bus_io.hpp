#ifndef ITS_SERVER_USLP_SRC_BUS_IO_HPP_
#define ITS_SERVER_USLP_SRC_BUS_IO_HPP_


#include <memory>

#include <zmq.hpp>

#include "bus_messages.hpp"


class bus_io
{
public:
	void send_message(zmq::socket_t & socket, const bus_output_sdu_accepted & message);
	void send_message(zmq::socket_t & socket, const bus_output_sdu_emitted & message);
	std::unique_ptr<bus_input_message> recv_message(zmq::socket_t & socket);
};



#endif /* ITS_SERVER_USLP_SRC_BUS_IO_HPP_ */
