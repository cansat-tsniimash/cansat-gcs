#ifndef ITS_SERVER_USLP_SRC_BUS_MESSAGES_HPP_
#define ITS_SERVER_USLP_SRC_BUS_MESSAGES_HPP_


#include <string>
#include <cstdint>
#include <vector>
#include <optional>

#include <ccsds/uslp/common/defs.hpp>
#include <ccsds/uslp/common/ids.hpp>



//! Абстрактное входящее сообщение c шины
class bus_input_message
{
public:
	//! Тип сообщения
	enum class kind_t
	{
		sdu_uplink_request,		//!< клиенты хотят что-то отправить
		radio_frame_downlink,	//!< радио прислало новый фрейм
		radio_uplink_state,		//!< cостояние отправного буфера радио
	};

protected:
	bus_input_message(kind_t kind_): kind(kind_) {}

public:
	virtual ~bus_input_message() = default;

	const kind_t kind;
};


std::string to_string(bus_input_message::kind_t kind);


//! Сообщение с данными для отправки в USLP стек
/*! для mapa данные отправлются как есть, для mapp заворачиваются в epp пакет */
class sdu_uplink_request: public bus_input_message
{
public:
	sdu_uplink_request(): bus_input_message(kind_t::sdu_uplink_request) {} // @suppress("Class members should be properly initialized")

	//! Идентификатор канала по которому сообщение должно быть отправлено
	ccsds::uslp::gmapid_t gmapid;
	//! Желаемое качество отправки пакета
	ccsds::uslp::qos_t qos = ccsds::uslp::qos_t::EXPEDITED;
	//! Кука сообщения
	ccsds::uslp::payload_cookie_t cookie;
	//! Данные сообщения
	std::vector<uint8_t> data;
};


//! Сообщение о состоянии отправного буфера радио
class radio_uplink_state: public bus_input_message
{
public:
	radio_uplink_state(): bus_input_message(kind_t::radio_uplink_state) {}

	//! кука фрейма ожидающего отправку
	std::optional<uint64_t> cookie_in_wait;
	//! кука фрейма находящегося в процессе отправки
	std::optional<uint64_t> cookie_in_progress;
	//! кука фрейма, отправка которого успешно завершена
	std::optional<uint64_t> cookie_done;
	//! кука фрейма, отправка которого не получилась
	std::optional<uint64_t> cookie_failed;
};


//! Сообщение с фреймом, полученным от радио
/*! Само по себе радио шлет больше метаданных, рисуем только те, что интересны */
class radio_downlink_frame: public bus_input_message
{
public:
	radio_downlink_frame(): bus_input_message(kind_t::radio_frame_downlink) {}

	//! Правильная ли у этого фрейма контрольная сумма уровня радио
	bool checksum_valid = false;
	//! Номер сообщения
	uint64_t frame_cookie = 0;
	//! Номер фрейма (по мнению радио)
	uint16_t frame_no = 0;
	//! Собственно байты сообщения
	std::vector<uint8_t> data;
};


// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-


//! Абстрактное сообщение отправляемое сервером на шину
class bus_output_message
{
public:
	//! Тип сообщения
	enum class kind_t
	{
		sdu_uplink_event,		//!< событие, случившееся с отправляемым SDU
		sdu_downlink_arrived,	//!< SDU было принято стеком
		radio_uplink_frame,		//!< Отправка фрейма в радио-сервер
	};

protected:
	bus_output_message(kind_t kind_): kind(kind_) {}

public:
	virtual ~bus_output_message() = default;

	const kind_t kind;
};


std::string to_string(bus_output_message::kind_t kind);



//! Сообщение об удивительных событиях, происходящих с SDU в стеке и не только
class sdu_uplink_event: public bus_output_message
{
public:
	//! Собственно что случилось с фреймом
	enum class event_kind_t
	{
		sdu_accepted,			//!< Принят в стек
		sdu_rejected,			//!< Не принят в стек
		sdu_sent_to_radio,		//!< Передан на сервер радио
		sdu_radiated,			//!< Излучён в эфир
		sdu_radiation_failed,	//!< Илучение в эфир не удалось
		// Пока все, но вообще тут должны быть еще
		// sdu_delivered		//!< Пакет добрался до борта
		// sdu_abandoned		//!< Пакет не добрался до борта
	};

	sdu_uplink_event(): bus_output_message(kind_t::sdu_uplink_event) {} // @suppress("Class members should be properly initialized")

	//! идентификатор канала
	ccsds::uslp::gmapid_t gmapid;
	//! cookie фрагмента полезной нагрузки
	ccsds::uslp::payload_part_cookie_t part_cookie;
	//! Тип события
	event_kind_t event_kind;
	//! Комментарий к событию
	std::string comment;
};


//! Сообщение о том, что SDU пришло по радио и было принято стеком
class sdu_downlink: public bus_output_message
{
public:
	sdu_downlink(): bus_output_message(kind_t::sdu_downlink_arrived) {}

	//! идентификатор канала
	ccsds::uslp::gmapid_t gmapid;
	//! qos пакета
	ccsds::uslp::qos_t qos = ccsds::uslp::qos_t::EXPEDITED;
	//! флаги состояния пакета \sa ccsds::uslp
	uint64_t flags = 0;
	//! Собственно данные пакета
	std::vector<uint8_t> data;
};


class radio_uplink_frame: public bus_output_message
{
public:
	radio_uplink_frame(): bus_output_message(kind_t::radio_uplink_frame) {}

	uint64_t frame_cookie;
	std::vector<uint8_t> data;
};


#endif /* ITS_SERVER_USLP_SRC_BUS_MESSAGES_HPP_ */
