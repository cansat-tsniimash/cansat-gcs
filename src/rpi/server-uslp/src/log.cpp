#include "log.hpp"


#include <cstdlib>
#include <iostream>
#include <sstream>
#include <array>

#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/core/core.hpp>


namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;


std::ostream & operator << (std::ostream & stream, severity_level level_)
{
	const char * strings[] = {
			"trace", "debug", "info", "warning", "error",
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
			severity_level::trace,
			severity_level::debug,
			severity_level::info,
			severity_level::warning,
			severity_level::error,
	};

	for (const auto variant: variants)
	{
		auto string_variant = to_string(variant);
		boost::algorithm::to_lower(string_variant);

		if (string_variant == raw_value)
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


severity_level severity_level_from_string(const std::string & level_str)
{
	std::stringstream stream(level_str);
	severity_level level;
	stream >> level;
	return level;
}


class formatter_t
{
public:
	bool enable_shell_color = false;


	int code_for_level(severity_level level_) const
	{
		if (!enable_shell_color)
			return -1;

		static const int codes[] = {
			90, // gray //"trace"
			38, // light gray // "debug",
			97, // white // "info",
			33, // yellow // "warning"
			31, // red // "error",
		};

		const int level = static_cast<int>(level_);
		if (level < 0 || level >= sizeof(codes)/sizeof(*codes))
			return -1;

		return codes[level];
	}


	void operator()(logging::record_view const& rec, logging::formatting_ostream& strm) const
	{
		strm << "[" << logging::extract<std::string>("Channel", rec) << "] ";

		const auto level = logging::extract<severity_level>("Severity", rec);
		const int color_code = code_for_level(level.get());

		if (color_code >= 0)
			strm << "\033[" << color_code << "m";

		strm << "<" << level << "> ";

		if (color_code >= 0)
			strm << "\033[0m";

		strm << logging::extract<std::string>("Message", rec);
	}
};


void setup_log()
{
	auto core = logging::core::get();
	core->add_global_attribute("Timestamp", attrs::local_clock());

	typedef sinks::synchronous_sink<sinks::text_ostream_backend> text_sink;
	boost::shared_ptr<text_sink> sink = boost::make_shared<text_sink>();

	boost::shared_ptr<std::ostream> cout_ptr(&std::cout, [](std::ostream*){});
	sink->locked_backend()->add_stream(cout_ptr);


	formatter_t formatter;
	if (isatty(1)) // 0 stdint, 1 stdout, 2 stderr/stdlog
		formatter.enable_shell_color = true;

	sink->set_formatter(std::move(formatter));
	core->add_sink(sink); // @suppress("Invalid arguments")


	severity_level level = severity_level::info;
	const char * env_level = std::getenv("ITS_LOG_LEVEL");
	if (nullptr != env_level)
	{
		severity_level candidate = severity_level_from_string(env_level);
		if (candidate != severity_level::invalid_last)
			level = candidate;
	}

	auto filterer = [level](const boost::log::attribute_value_set & attrs)->bool {
		const auto record_level = logging::extract<severity_level>("Severity", attrs);
		return record_level >= level;
	};

	core->set_filter(std::move(filterer));
}


source_t build_source(std::string channel_name)
{
	source_t retval;
	retval.channel(std::move(channel_name));
	return retval;
}

