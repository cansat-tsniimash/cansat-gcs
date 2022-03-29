#ifndef ITS_SERVER_USLP_SRC_LOG_HPP_
#define ITS_SERVER_USLP_SRC_LOG_HPP_


#include <ostream>
#include <istream>

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/expressions/keyword_fwd.hpp>
#include <boost/log/expressions/keyword.hpp>


enum class severity_level
{
	error = 0,
	warning,
	info,
	debug,
	trace,

	invalid_last
};

std::ostream & operator << (std::ostream & stream, severity_level level);
std::istream & operator >> (std::istream & stream, severity_level & level);
std::string to_string(severity_level level);


BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);

typedef boost::log::sources::severity_channel_logger<severity_level> source_t;


#define LOG(level) BOOST_LOG_SEV(_slg, severity_level::level)


void setup_log();

source_t build_source(std::string channel_name);


#endif /* ITS_SERVER_USLP_SRC_LOG_HPP_ */
