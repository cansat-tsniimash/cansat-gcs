#include <iostream>
#include <unistd.h>

#include "tun_device.hpp"
#include "zmq_server.hpp"

#define LOGURU_WITH_STREAMS 1
#include <loguru.hpp>


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

	zmq::context_t ctx;
	zmq_server server(&ctx);
	tun_device dev;

	server.open();
	usleep(100*1000); // Для того чтобы сервер успел соединится

	dev.open("tun100");
	dev.set_ip("10.10.10.1", 24);
	dev.set_mtu(200);
	dev.set_up(true);

	zmq::pollitem_t item;


	while(1)
	{
		int fd = dev.fd();
		uint8_t buffer[300];
		int readed = ::read(fd, buffer, sizeof(buffer));
		std::cout << "read " << readed << std::endl;
	}

	return 0;
}
