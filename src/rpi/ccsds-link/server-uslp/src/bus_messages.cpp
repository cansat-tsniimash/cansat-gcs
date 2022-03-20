#include "bus_messages.hpp"



std::string to_string(bus_input_message::kind_t kind)
{
	switch (kind)
	{
	case bus_input_message::kind_t::sdu_uplink:
		return "sdu_uplink";

	case bus_input_message::kind_t::radio_tx_state:
		return "radio_tx_state";

	case bus_input_message::kind_t::frame_downlink:
		return "frame_downlink";

	default:
		return "<unknown:" + std::to_string(static_cast<int>(kind));
	}
}


std::string to_string(bus_output_message::kind_t kind)
{
	switch (kind)
	{
	case bus_output_message::kind_t::sdu_emitted:
		return "sdu_emitted";

	case bus_output_message::kind_t::sdu_downlink:
		return "sdu_downlink";

	default:
		return "<unknown:" + std::to_string(static_cast<int>(kind));
	}
}
