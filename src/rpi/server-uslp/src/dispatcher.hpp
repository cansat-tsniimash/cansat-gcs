#ifndef ITS_SERVER_USLP_SRC_DISPATCHER_HPP_
#define ITS_SERVER_USLP_SRC_DISPATCHER_HPP_


#include <tuple>
#include <optional>
#include <deque>

#include "stack.hpp"
#include "bus_messages.hpp"
#include "bus_io.hpp"

#include <ccsds/uslp/events.hpp>
#include <ccsds/uslp/input_stack.hpp>


//! Информация о фрейме отправленном на радио
/*! Нужна для сопоставления кук SDU с куками фрейма и для оповещения
 *  клиентов о том, что там происходит с их SDU */
struct frame_queue_entry_t
{
	//! Состояние фрейма в его жизненом цикле
	enum class frame_state_t
	{
		sent_to_radio, in_wait, in_progress,
		radiated, failed // терминальные события
	};

	//! Кука фрейма
	uint64_t frame_cookie;
	//! Канал из которого был выгребен ccsds фрейм
	ccsds::uslp::gmapid_t sdu_mapid;
	//! Куки SDU, летящие этим фреймом
	std::vector<ccsds::uslp::payload_part_cookie_t> sdu_cookies;
	//! Время отправки фрейма в радио
	/*! Нужно для таймаутов чтобы очередь не вырастала до бесконечности */
	std::chrono::steady_clock::time_point send_time;
	//! Состояние фрейма
	frame_state_t state;
};


class dispatcher: public ccsds::uslp::input_stack_event_handler
{
public:
	dispatcher(istack & istack_, ostack & ostack_, bus_io & io_);

	void poll();

	template <typename DURATION>
	void frame_done_timeout(const DURATION & timeout)
	{
		_frame_done_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(timeout);
	}

	std::chrono::milliseconds frame_done_timeout() const { return _frame_done_timeout; }


protected:
	// приём и обработка сообщений с шины
	virtual void _on_map_sdu_event(const ccsds::uslp::acceptor_event_map_sdu & event) override;

	// Приём и обработка собщений с шины
	void _dispatch_bus_message(const bus_input_message & message);

	void _on_sdu_uplink_request(const sdu_uplink_request & request);
	void _on_radio_downlink_frame(const radio_downlink_frame & frame);

	void _on_radio_uplink_state(const radio_uplink_state & state);
	void _clear_frames_queue();
	void _update_frames_queue(const radio_uplink_state & state);
	void _decide_next_uplink_frame(const radio_uplink_state & state);

private:
	//! Кука для следующего отправляемого сообщения для радио (не должно быть нулём)
	uint64_t _next_rf_uplink_frame_cookie = 1;

	//! Информация о фреймах, которые мы отправили в большой мир и теперь следим за их
	//! судьбой
	std::deque<frame_queue_entry_t> _frames_in_wait;

	//! Таймаут, который мы даем фреймам на то, чтобы их судьба как-то решилась
	std::chrono::milliseconds _frame_done_timeout = std::chrono::milliseconds(5000);

	istack & _istack;
	ostack & _ostack;
	bus_io & _io;
};



#endif /* ITS_SERVER_USLP_SRC_DISPATCHER_HPP_ */
