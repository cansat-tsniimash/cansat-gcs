#ifndef ITS_SERVER_USLP_SRC_DISPATCHER_HPP_
#define ITS_SERVER_USLP_SRC_DISPATCHER_HPP_


#include "stack.hpp"
#include "bus_messages.hpp"


class dispatcher
{
public:
	dispatcher()
	{

	}

	void dispatch_bus_message(const bus_input_message & message)
	{
		switch (message.kind)
		{
		case bus_input_message::kind_t::sdu_uplink_request:
		}
	}

private:
	void _on_sdu_uplink_request(const bus_input_sdu_uplink_request & request)
	{
		const auto gmap_id = request.gmapid;
		auto * map = _ostack.stack().get_map_channel(gmap_id);
	}

	istack _istack;
	ostack _ostack;
};



#endif /* ITS_SERVER_USLP_SRC_DISPATCHER_HPP_ */
