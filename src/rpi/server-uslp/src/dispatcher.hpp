#ifndef ITS_SERVER_USLP_SRC_DISPATCHER_HPP_
#define ITS_SERVER_USLP_SRC_DISPATCHER_HPP_


#include <tuple>
#include <optional>

#include "stack.hpp"
#include "bus_messages.hpp"
#include "bus_io.hpp"

#include <ccsds/uslp/events.hpp>
#include <ccsds/uslp/input_stack.hpp>


class dispatcher: public ccsds::uslp::input_stack_event_handler
{
public:
	dispatcher(istack & istack_, ostack & ostack_, bus_io & io_);

	void poll();

protected:
	// приём и обработка сообщений с шины
	virtual void _on_map_sdu_event(const ccsds::uslp::acceptor_event_map_sdu & event) override;

	// Приём и обработка собщений с шины
	void _dispatch_bus_message(const bus_input_message & message);

	void _on_sdu_uplink_request(const sdu_uplink_request & request);
	void _on_radio_downlink_frame(const radio_downlink_frame & frame);
	void _on_radio_uplink_state(const radio_uplink_state & state);

private:
	//! Кука для следующего отправляемого сообщения для радио (не должно быть нулём)
	uint64_t _next_rf_uplink_frame_cookie = 1;
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
