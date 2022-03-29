
#include <csignal>
#include <atomic>
#include <string>

#include <zmq.hpp>

#include "log.hpp"
#include "bus_io.hpp"
#include "dispatcher.hpp"
#include "stack.hpp"


static auto _slg = build_source("main");


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
		LOG(error) << "unable to load config: " << e.what();
		return EXIT_FAILURE;
	}

	zmq::context_t ctx;
	bus_io io(ctx);
	io.connect_bpcs(c.bpcs_endpoint);
	io.connect_bscp(c.bscp_endpoint);

	ostack ost;
	istack ist;

	dispatcher d(ist, ost, io);

	signal_catched.store(false);
	std::signal(SIGTERM, signal_handler);
	std::signal(SIGINT, signal_handler);
	std::signal(SIGHUP, signal_handler);

	while(1)
	{
		if (signal_catched.load())
		{
			LOG(info) << "got signal " << signal_value << " stooping loop";
			break;
		}

		d.poll();
	}

	LOG(info) << "terminating gracefully";
	io.close();
	ctx.close();
	LOG(info) << "termination complete";

	return EXIT_SUCCESS;
}


int main(int argc, char ** argv)
{
	int rc = EXIT_SUCCESS;
	try
	{
		rc = real_main(argc, argv);
	}
	catch (std::exception & e)
	{
		LOG(error) << "failure! " << e.what();
		// Бросаем дальше, чтобы увидеть стектрейс от логуру
		throw;
	}

	LOG(info) << "shutting down gracefully";
	return rc;
}

