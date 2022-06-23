#include <iostream>
#include <array>
#include <tuple>

#include <unistd.h>

#include <boost/program_options.hpp>

#include "tun_device.hpp"
#include "zmq_server.hpp"

#include "log.hpp"

static auto _slg = build_source("main");


#define TUN_BUFFER_SIZE 1500


static std::string split_cidr_addr(const std::string & input)
{
	// никуда не годится, но простенько

	const auto slash_pos = input.find('/');
	if (slash_pos == std::string::npos)
		throw std::invalid_argument("bad CIDR address: \"" + input + "\". there is no slash");

	const auto retval = input.substr(0, slash_pos);
	if (0 == retval.size())
		throw std::invalid_argument("bad CIDR address: \"" + input + "\". addr size is zero");

	return retval;
}


static int split_cidr_mask(const std::string & input)
{
	// тоже никуда не годится, но тоже простенько

	const auto slash_pos = input.find('/');
	if (slash_pos == std::string::npos)
		throw std::invalid_argument("bad CIDR address: \"" + input + "\". there is no slash");

	const auto mask_part = input.substr(slash_pos + 1);
	if (0 == mask_part.size())
		throw std::invalid_argument("bad CIDR address: \"" + input + "\". zero sized port part");

	return std::stoi(mask_part);
}


static std::tuple<int, int, int> split_channel_id(const std::string & input)
{
	const auto first_dot_pos = input.find('.');
	const auto last_dot_pos = input.find_last_of('.');

	if (std::string::npos == first_dot_pos || std::string::npos == last_dot_pos)
		throw std::invalid_argument("bad channel id. No dots found");

	if (first_dot_pos == last_dot_pos)
		throw std::invalid_argument("bad channel id. only one dot found");

	const auto sc_part = input.substr(0, first_dot_pos);
	const auto vc_part = input.substr(first_dot_pos+1, last_dot_pos-first_dot_pos);
	const auto map_part = input.substr(last_dot_pos+1);

	return std::make_tuple( // @suppress("Invalid arguments")
			std::stoi(sc_part), std::stoi(vc_part), std::stoi(map_part)
	);
}


static void loop(zmq_server & server, tun_device & tun)
{
	const auto tun_fd = tun.fd();
	const auto & sub_socket = server.bpcs_socket();

	std::array<zmq::pollitem_t, 2> poll_items{{
			{ nullptr, tun_fd , ZMQ_POLLIN, 0 },
			{ const_cast<void*>(sub_socket.handle()), 0, ZMQ_POLLIN, 0 }
	}};

	LOG(trace) << "entering poll cycle";
	int rc = zmq::poll(poll_items, std::chrono::milliseconds(500));
	if (0 == rc)
		return;

	if (poll_items[0].revents & ZMQ_POLLIN)
	{
		// Что-то пришло с туннеля
		LOG(debug) << "got event from tun device";

		std::vector<uint8_t> tun_buffer(1500);
		const size_t readed = tun.read_packet(tun_buffer.data(), tun_buffer.size());
		if (readed > tun_buffer.size())
			LOG(error) << "tun device provided larger packet that we can handle";

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
		LOG(debug) << "there is a tun packet of size "
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
		LOG(debug) << "got event from bus";

		downlink_packet message;
		server.recv_downlink_packet(message);
		if (message.bad)
		{
			LOG(debug) << "message is bad";
		}
		else
		{
			tun.write_packet(message.data.data(), message.data.size());
		}
	}
}


int main(int argc, char ** argv)
{
	setup_log();

	// Эти опции мы разберем из argv
	std::string tun_name = "tun100";
	std::string uplink_channel = "66.0.1";
	std::string downlink_channel = "66.0.1";
	std::string tun_addr = "10.0.0.1/24";
	int tun_mtu = 200;
	// Эти допарсим сами
	std::string tun_ip;
	int tun_mask;
	int uplink_sc, uplink_vc, uplink_map;
	int downlink_sc, downlink_vc, downlink_map;

	try
	{
		namespace po = boost::program_options;
		po::options_description descr;
		descr.add_options()
				("tun", po::value(&tun_name)->default_value(tun_name))
				("up-channel", po::value(&uplink_channel)->default_value(uplink_channel))
				("down-channel", po::value(&downlink_channel)->default_value(downlink_channel))
				("addr", po::value(&tun_addr)->default_value(tun_addr))
				("mtu", po::value(&tun_mtu)->default_value(tun_mtu))
				("help", po::value<bool>()->implicit_value(true))
		;

		po::command_line_parser parser(argc, argv);
		parser.allow_unregistered();
		parser.options(descr);
		auto parsed_options = parser.run();
		po::variables_map vm;
		po::store(parsed_options, vm, true);
		po::notify(vm);

		if (vm.find("help") != vm.end())
		{
			std::cout << descr;
			return EXIT_FAILURE;
		}

		tun_ip = split_cidr_addr(tun_addr);
		tun_mask = split_cidr_mask(tun_addr);

		try
		{
			std::tie(uplink_sc, uplink_vc, uplink_map) = split_channel_id(uplink_channel);
		}
		catch (std::exception & e)
		{
			throw std::runtime_error("bad uplink channel \"" + std::string(e.what()) + "\"");
		}

		try
		{
			std::tie(downlink_sc, downlink_vc, downlink_map) = split_channel_id(downlink_channel);
		}
		catch (std::exception & e)
		{
			throw std::runtime_error("bad downlink channel \"" + std::string(e.what()) + "\"");
		}
	}
	catch (std::exception & e)
	{
		LOG(error) << "unable to parse options: " << e.what();
		return EXIT_FAILURE;
	}

	zmq::context_t ctx;
	zmq_server server(&ctx);
	tun_device tun;

	try
	{
		server.set_uplink_channel(uplink_sc, uplink_vc, uplink_map);
		server.set_downlink_channel(downlink_sc, downlink_vc, downlink_map);
		server.open();
		usleep(100*1000); // Для того чтобы сервер успел соединится
	}
	catch (std::exception & e)
	{
		LOG(error) << "unable to start ZMQ server: " << e.what();
		return EXIT_FAILURE;
	}

	try
	{
		tun.open(tun_name);
		tun.set_ip(tun_ip, tun_mask);
		tun.set_mtu(tun_mtu);
		tun.set_up(true);
	}
	catch (std::exception & e)
	{
		LOG(error) << "unable to start tun device: " << e.what();
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
			LOG(error) << "loop failed with error " << e.what();
		}
	}

	return EXIT_SUCCESS;
}
