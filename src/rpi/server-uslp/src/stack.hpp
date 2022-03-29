#ifndef ITS_SERVER_USLP_SRC_STACK_HPP_
#define ITS_SERVER_USLP_SRC_STACK_HPP_


#include <ccsds/uslp/output_stack.hpp>
#include <ccsds/uslp/input_stack.hpp>


#define RADIO_FRAME_SIZE			(200)
#define SPACECRAFT_ID				(0x42)

#define UPLINK_VCHANNEL_ID			(0x00)
#define UPLINK_TELECOMMAND_MAPID	(0x00)
#define UPLINK_IP_MAPID				(0x01)

#define DOWNLINK_VCHANNEL_ID		(0x00)
#define DOWNLINK_TELEMETERY_MAPID	(0x00)
#define DOWNLINK_IP_MAPID			(0x01)


class ostack: public ccsds::uslp::output_stack
{
public:
	ostack();
};


class istack: public ccsds::uslp::input_stack
{
public:
	istack();
};


#endif /* ITS_SERVER_USLP_SRC_STACK_HPP_ */
