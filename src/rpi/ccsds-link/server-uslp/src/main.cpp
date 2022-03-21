
#include <csignal>
#include <atomic>
#include <string>

#include <zmq.hpp>

#include "log.hpp"
#include "bus_io.hpp"


#define ITS_BSCP_ENDPOINT_KEY "ITS_GBUS_BSCP_ENDPOINT"
#define ITS_BPCS_ENDPOINT_KEY "ITS_GBUS_BPCS_ENDPOINT"


//! Настройки приложения
struct config
{
	//! Эндпоит broker publish, client subscribe
	std::string bpcs_endpoint;
	//! Эндпоинт broker subscribe, client publish
	std::string bscp_endpoint;
};



static std::atomic<bool> signal_catched;
static int signal_value = 0;

static void signal_handler(int signal)
{
	signal_value = signal;

	bool expected = false;
	bool desired = true;
	if (signal_catched.compare_exchange_strong(expected, desired))
	{
		return;
	}
	else
	{
		std::abort();	// Это не первый сигнал, который мы поймали
						// Почему-то мы не остановились по нему.
						// Значит будем просто грязно умирать
	}
}


static config get_config(int argc, char ** argv)
{
	config retval;

	if (const char * env_bscp = std::getenv(ITS_BSCP_ENDPOINT_KEY))
		retval.bscp_endpoint = env_bscp;
	else
		throw std::runtime_error("there is no BSCP endpoint in " ITS_BSCP_ENDPOINT_KEY " envvar");

	if (const char * env_bpcs = std::getenv(ITS_BPCS_ENDPOINT_KEY))
		retval.bpcs_endpoint = env_bpcs;
	else
		throw std::runtime_error("there is no BPCS endpoint in " ITS_BSCP_ENDPOINT_KEY " envvar");

	return retval;
}


static int real_main(int argc, char ** argv)
{
	config c;
	try
	{
		c = get_config(argc, argv);
	}
	catch (std::exception & e)
	{
		LOG_S(ERROR) << "unable to load config: " << e.what();
		return EXIT_FAILURE;
	}

	zmq::context_t ctx;
	bus_io io(ctx);
	io.connect_bpcs(c.bpcs_endpoint);
	io.connect_bscp(c.bscp_endpoint);

	signal_catched.store(false);
	std::signal(SIGTERM, signal_handler);
	std::signal(SIGINT, signal_handler);
	std::signal(SIGHUP, signal_handler);

	while(1)
	{
		if (signal_catched.load())
		{
			LOG_S(INFO) << "got signal " << signal_value << " stooping loop";
			break;
		}

		LOG_S(INFO+2) << "entering poll cycle";
		const auto poll_timeout = std::chrono::milliseconds(1000);
		const bool got_messages = io.poll_sub_socket(poll_timeout);
		if (!got_messages)
			continue;

		const auto message_ptr = io.recv_message();
		LOG_S(INFO+2) << "got bus message " << to_string(message_ptr->kind);
	}

	LOG_S(INFO) << "terminating gracefully";
	io.close();
	ctx.close();
	LOG_S(INFO) << "termination complete";

	return EXIT_SUCCESS;
}


int main(int argc, char ** argv)
{
	loguru::g_preamble_thread = false;
	//loguru::g_preamble_file = false;
	//loguru::g_preamble_verbose = false;
	loguru::g_preamble_time = false;
	loguru::g_preamble_date = false;
	loguru::g_preamble_uptime = false;
	loguru::g_colorlogtostderr = true;

	loguru::init(argc, argv);

	int rc = EXIT_SUCCESS;
	try
	{
		rc = real_main(argc, argv);
	}
	catch (std::exception & e)
	{
		LOG_S(ERROR) << "failure! " << e.what();
		// Бросаем дальше, чтобы увидеть стектрейс от логуру
		throw;
	}

	LOG_S(INFO) << "shutting down gracefully";
	return rc;
}

