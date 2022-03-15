#ifndef ITS_SERVER_USLP_SRC_BUS_MESSAGES_HPP_
#define ITS_SERVER_USLP_SRC_BUS_MESSAGES_HPP_


#include <string>
#include <cstdint>
#include <vector>

#include <ccsds/uslp/common/defs.hpp>
#include <ccsds/uslp/common/ids.hpp>



//! Абстрактное входящее сообщение c шины
class bus_input_message
{
public:
	//! Тип сообщения
	enum class kind_t
	{
		map_sdu		//!< сообщение для отправки
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
class bus_input_map_sdu: public bus_input_message
{
public:
	bus_input_map_sdu(): bus_input_message(kind_t::map_sdu) {}

	//! Идентификатор канала по которому сообщение должно быть отправлено
	ccsds::uslp::gmapid_t _gmapid;
	//! Желаемое качество отправки пакета
	ccsds::uslp::qos_t qos = ccsds::uslp::qos_t::EXPEDITED;
	//! Данные сообщения
	std::vector<uint8_t> _data;
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
		sdu_accepted,	//!< SDU было принято стеком
	};

protected:
	bus_output_message(kind_t kind_): kind(kind_) {}

public:
	virtual ~bus_output_message() = default;

	const kind_t kind;
};


std::string to_string(bus_output_message::kind_t kind);



//! Сообщение о том, что SDU было отправлено по радио
class bus_output_sdu_emitted: public bus_output_message
{
public:
	bus_output_sdu_emitted(): bus_output_message(kind_t::sdu_emitted) {}

	//! идентификатор канала
	ccsds::uslp::gmapid_t gmapid;
	//! cookie полезной нагрузки (и его доп параметры)
	ccsds::uslp::payload_cookie_ref cookie;
};


//! Сообщение о том, что SDU было принято стеком
class bus_output_sdu_accepted: public bus_output_message
{
public:
	bus_output_sdu_accepted(): bus_output_message(kind_t::sdu_accepted) {}

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
