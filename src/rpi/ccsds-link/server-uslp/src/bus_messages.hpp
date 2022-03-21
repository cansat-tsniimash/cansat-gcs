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
		sdu_uplink,			//!< клиенты хотят что-то отправить
		frame_downlink,		//!< радио прислало новый фрейм
		radio_tx_state,		//!< Состояние отправного буфера радио
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
class bus_input_sdu_uplink: public bus_input_message
{
public:
	bus_input_sdu_uplink(): bus_input_message(kind_t::sdu_uplink) {}

	//! Идентификатор канала по которому сообщение должно быть отправлено
	ccsds::uslp::gmapid_t gmapid;
	//! Желаемое качество отправки пакета
	ccsds::uslp::qos_t qos = ccsds::uslp::qos_t::EXPEDITED;
	//! Кука сообщения
	ccsds::uslp::payload_cookie_t cookie;
	//! Данные сообщения
	std::vector<uint8_t> _data;
};


//! Сообщение о состоянии отправного буфера радио
class bus_input_radio_uplink_state: public bus_input_message
{
public:
	bus_input_radio_uplink_state(): bus_input_message(kind_t::radio_tx_state) {}

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
class bus_input_radio_downlink_frame: public bus_input_message
{
public:
	bus_input_radio_downlink_frame(): bus_input_message(kind_t::frame_downlink) {}

	//! Правильная ли у этого фрейма контрольная сумма уровня радио
	bool checksum_valid = false;
	//! Номер сообщения
	uint64_t cookie = 0;
	//! Номер фрейма (по мнению радио)
	uint64_t frame_no = 0;
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
		sdu_emitted,	//!< SDU было "выпущено" стеком в радио-сервер
		sdu_downlink,	//!< SDU было принято стеком
	};

protected:
	bus_output_message(kind_t kind_): kind(kind_) {}

public:
	virtual ~bus_output_message() = default;

	const kind_t kind;
};


std::string to_string(bus_output_message::kind_t kind);



//! Сообщение о том, что SDU было отправлено по радио
class bus_output_sdu_event: public bus_output_message
{
public:
	bus_output_sdu_event(): bus_output_message(kind_t::sdu_emitted) {}

	//! идентификатор канала
	ccsds::uslp::gmapid_t gmapid;
	//! cookie полезной нагрузки (и его доп параметры)
	ccsds::uslp::payload_cookie_ref cookie;
};


//! Сообщение о том, что SDU было принято стеком
class bus_output_sdu_downlink: public bus_output_message
{
public:
	bus_output_sdu_downlink(): bus_output_message(kind_t::sdu_downlink) {}

	//! идентификатор канала
	ccsds::uslp::gmapid_t gmapid;
	//! qos пакета
	ccsds::uslp::qos_t qos = ccsds::uslp::qos_t::EXPEDITED;
	//! флаги состояния пакета \sa ccsds::uslp
	uint64_t flags = 0;
	//! Собственно данные пакета
	std::vector<uint8_t> data;
};


#endif /* ITS_SERVER_USLP_SRC_BUS_MESSAGES_HPP_ */
