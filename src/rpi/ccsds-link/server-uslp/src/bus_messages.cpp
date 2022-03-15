#include "bus_messages.hpp"



std::string to_string(bus_input_message::kind_t kind)
{
	switch (kind)
	{
	case bus_input_message::kind_t::map_sdu:
		return "map_sdu";

	default:
		return "<unknown:" + std::to_string(static_cast<int>(kind));
	}
}


std::string to_string(bus_output_message::kind_t kind)
{
	switch (kind)
	{
	case bus_output_message::kind_t::sdu_emitted:
		return "map_sdu_emitted";

	case bus_output_message::kind_t::sdu_accepted:
		return "map_sdu_accepted";

	default:
		return "<unknown:" + std::to_string(static_cast<int>(kind));
	}
}
