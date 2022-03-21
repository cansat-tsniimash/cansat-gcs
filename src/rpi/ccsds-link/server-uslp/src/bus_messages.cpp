#include "bus_messages.hpp"


std::string to_string(bus_input_message::kind_t kind)
{
	switch (kind)
	{
	case bus_input_message::kind_t::sdu_uplink_request:
		return "sdu_uplink_request";

	case bus_input_message::kind_t::radio_tx_state:
		return "radio_tx_state";

	case bus_input_message::kind_t::radio_frame_downlink:
		return "radio_frame_downlink";

	default:
		return "<unknown:" + std::to_string(static_cast<int>(kind));
	}
}


std::string to_string(bus_output_message::kind_t kind)
{
	switch (kind)
	{
	case bus_output_message::kind_t::sdu_uplink_event:
		return "sdu_uplink_event";

	case bus_output_message::kind_t::sdu_downlink_arrived:
		return "sdu_downlink_arrived";

	default:
		return "<unknown:" + std::to_string(static_cast<int>(kind));
	}
}
