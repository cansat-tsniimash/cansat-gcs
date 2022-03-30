#ifndef ITS_SERVER_USLP_SRC_LOG_HPP_
#define ITS_SERVER_USLP_SRC_LOG_HPP_


#include <ostream>
#include <istream>

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/expressions/keyword_fwd.hpp>
#include <boost/log/expressions/keyword.hpp>


#define LOG(level) BOOST_LOG_SEV(_slg, severity_level::level)


enum class severity_level
{
	trace = 0,
	debug,
	info,
	warning,
	error,

	invalid_last
};


BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);


std::ostream & operator << (std::ostream & stream, severity_level level);
std::istream & operator >> (std::istream & stream, severity_level & level);
std::string to_string(severity_level level);
severity_level severity_level_from_string(const std::string & level_str);

typedef boost::log::sources::severity_channel_logger<severity_level> source_t;

source_t build_source(std::string channel_name);


void setup_log();


#endif /* ITS_SERVER_USLP_SRC_LOG_HPP_ */
