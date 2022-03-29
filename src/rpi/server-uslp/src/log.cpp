#include "log.hpp"


#include <sstream>
#include <array>

#include <boost/log/utility/setup/common_attributes.hpp>


namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;


std::ostream & operator << (std::ostream & stream, severity_level level_)
{
	const char * strings[] = {
			"error", "warning", "info", "debug", "trace"
	};

	int level = static_cast<int>(level_);
	if (level >= 0 && level <= static_cast<int>(severity_level::invalid_last))
		stream << strings[level];
	else
		stream << "<invalid:" << std::to_string(level) << ">";

	return stream;
}


std::istream & operator >> (std::istream & stream, severity_level & level)
{
	level = severity_level::invalid_last;

	std::string raw_value;
	stream >> raw_value;

	const std::array<severity_level, 5> variants = {
			severity_level::error,
			severity_level::warning,
			severity_level::info,
			severity_level::debug,
			severity_level::trace
	};

	for (const auto variant: variants)
	{
		if (to_string(variant) == raw_value)
		{
			level = variant;
			break;
		}
	}

	return stream;
}


std::string to_string(severity_level level)
{
	std::stringstream stream;
	stream << level;
	return stream.str();
}


void setup_log()
{
	logging::add_common_attributes();
}


source_t build_source(std::string channel_name)
{
	source_t retval;
	retval.channel(std::move(channel_name));
	return retval;
}

