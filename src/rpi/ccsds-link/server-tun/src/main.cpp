#include <iostream>
#include <array>
#include <unistd.h>

#include "tun_device.hpp"
#include "zmq_server.hpp"

#include "log.hpp"

#define TUN_BUFFER_SIZE 1500


static void loop(zmq_server & server, tun_device & tun)
{
	const auto tun_fd = tun.fd();
	const auto & sub_socket = server.bpcs_socket();

	std::array<zmq::pollitem_t, 2> poll_items{{
			{ nullptr, tun_fd , ZMQ_POLLIN, 0 },
			{ const_cast<void*>(sub_socket.handle()), 0, ZMQ_POLLIN, 0 }
	}};

	LOG_S(INFO+2) << "entering poll cycle";
	int rc = zmq::poll(poll_items, std::chrono::milliseconds(500));
	if (0 == rc)
		return;

	if (poll_items[0].revents & ZMQ_POLLIN)
	{
		// Что-то пришло с туннеля
		LOG_S(INFO+1) << "got event from tun device";

		std::vector<uint8_t> tun_buffer(1500);
		const size_t readed = tun.read_packet(tun_buffer.data(), tun_buffer.size());
		if (readed > tun_buffer.size())
			LOG_S(ERROR) << "tun device provided larger packet that we can handle";

		tun_buffer.resize(readed);
		uint16_t flags = 0;
		uint16_t proto = 0;
		/*
		if (readed >= 4)
		{
			// Выгребаем информацию о пакете
			std::memcpy(&flags, tun_buffer.data(), sizeof(flags));
			std::memcpy(&proto, tun_buffer.data()+2, sizeof(proto));
			tun_buffer.erase(tun_buffer.begin(), tun_buffer.begin()+4);
		}
		else
			LOG_S(ERROR) << "tun device provided less than 4 bytes, there is no PI";
		*/
		LOG_S(INFO+1) << "there is a tun packet of size "
				<< tun_buffer.size() << ", "
				// << "proto " << proto << ","
				// << "flags " << flags
		;
		uplink_packet message;
		message.data = std::move(tun_buffer);
		message.proto = proto;
		message.flags = flags;
		server.send_uplink_packet(message);
	}

	if (poll_items[1].revents & ZMQ_POLLIN)
	{
		// Что-то пришло с шины
		LOG_S(INFO+1) << "got event from bus";

		downlink_packet message;
		server.recv_downlink_packet(message);
		if (message.bad)
		{
			LOG_S(INFO+1) << "message is bad";
		}
		else
		{
			tun.write_packet(message.data.data(), message.data.size());
		}
	}
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

	zmq::context_t ctx;
	zmq_server server(&ctx);
	tun_device tun;

	try
	{
		server.set_uplink_channel(0x42, 0, 1);
		server.set_downlink_channel(0x42, 0, 1);
		server.open();
		usleep(100*1000); // Для того чтобы сервер успел соединится
	}
	catch (std::exception & e)
	{
		LOG_S(ERROR) << "unable to start ZMQ server: " << e.what();
		return EXIT_FAILURE;
	}

	try
	{
		tun.open("tun100");
		tun.set_ip("10.10.10.1", 24);
		tun.set_mtu(200);
		tun.set_up(true);
	}
	catch (std::exception & e)
	{
		LOG_S(ERROR) << "unable to start tun device: " << e.what();
		return EXIT_FAILURE;
	}

	while(1)
	{
		try
		{
			loop(server, tun);
		}
		catch (std::exception & e)
		{
			LOG_S(ERROR) << "loop failed with error " << e.what();
		}
	}

	return EXIT_SUCCESS;
}
