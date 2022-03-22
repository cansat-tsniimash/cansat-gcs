#ifndef ITS_SERVER_USLP_SRC_DISPATCHER_HPP_
#define ITS_SERVER_USLP_SRC_DISPATCHER_HPP_


#include <tuple>
#include <optional>

#include "stack.hpp"
#include "bus_messages.hpp"
#include "bus_io.hpp"


class dispatcher
{
public:
	dispatcher(istack & istack_, ostack & ostack_, bus_io & io_);

	void poll();

protected:
	void _dispatch_bus_message(const bus_input_message & message);

	void _on_sdu_uplink_request(const sdu_uplink_request & request);
	void _on_radio_downlink_frame(const radio_downlink_frame & frame);
	void _on_radio_uplink_state(const radio_uplink_state & state);

private:
	//! Кука для следующего отправляемого сообщения для радио (не должно быть нулём)
	uint64_t _next_rf_uplink_frame_cookie = 1;
	//! Номер предыдущего полученного сообщения по радио (в рамках его протокола)
	std::optional<uint16_t> _prev_rf_frame_no;
	//! Кука радио фрейма и соответствующая ему кука CCSDS фрейма
	/*! Заполняется при отправке фрейма в радио для отслеживания судьбы сообщения */
	std::optional<std::tuple<
		uint64_t, /* frame_cookie */
		ccsds::uslp::gmapid_t, /* channel_id */
		std::vector<ccsds::uslp::payload_part_cookie_t> /* SDU cookies */
	>> _sent_to_rf_frame;

	istack & _istack;
	ostack & _ostack;
	bus_io & _io;
};



#endif /* ITS_SERVER_USLP_SRC_DISPATCHER_HPP_ */
