#ifndef ITS_SERVER_USLP_SRC_STACK_HPP_
#define ITS_SERVER_USLP_SRC_STACK_HPP_


#include <ccsds/uslp/output_stack.hpp>
#include <ccsds/uslp/input_stack.hpp>

#include <ccsds/uslp/common/defs.hpp>

#include <ccsds/uslp/physical/mchannel_rr_muxer.hpp>
#include <ccsds/uslp/master/vchannel_rr_muxer.hpp>
#include <ccsds/uslp/virtual/map_rr_muxer.hpp>
#include <ccsds/uslp/map/map_packet_emitter.hpp>
#include <ccsds/uslp/map/map_access_emitter.hpp>

#include <ccsds/uslp/physical/mchannel_demuxer.hpp>
#include <ccsds/uslp/master/vchannel_demuxer.hpp>
#include <ccsds/uslp/virtual/map_demuxer.hpp>
#include <ccsds/uslp/map/map_packet_acceptor.hpp>
#include <ccsds/uslp/map/map_access_acceptor.hpp>


#define RADIO_FRAME_SIZE (200)


class ostack
{
public:
	ostack()
	{
		using namespace ccsds;
		using namespace ccsds::uslp;

		mchannel_rr_muxer * phys = _stack.create_pchannel<mchannel_rr_muxer>("lora");
		phys->frame_size(RADIO_FRAME_SIZE);
		phys->error_control_len(error_control_len_t::ZERO);

		auto master = _stack.create_mchannel<vchannel_rr_muxer>(mcid_t(0x42));
		master->id_is_destination(true);

		auto virt = _stack.create_vchannel<map_rr_muxer>(gvcid_t(master->channel_id, 0x00));
		virt->frame_seq_no_len(2);

		_ip_channel = _stack.create_map<map_packet_emitter>(gmapid_t(virt->channel_id, 0x00));
		_command_channel = _stack.create_map<map_access_emitter>(gmapid_t(virt->channel_id, 0x01));

		phys->finalize();
	}


	void send_command(ccsds::uslp::payload_cookie_t cookie, const uint8_t * data, size_t data_size)
	{
		using namespace ccsds;
		using namespace ccsds::uslp;

		_command_channel->add_sdu(cookie, data, data_size, qos_t::SEQUENCE_CONTROLLED);
	}

private:
	ccsds::uslp::map_packet_emitter * _ip_channel;
	ccsds::uslp::map_access_emitter * _command_channel;
	ccsds::uslp::output_stack _stack;

};


class istack
{
public:
	istack()
	{
		using namespace ccsds;
		using namespace ccsds::uslp;

		mchannel_demuxer * phys;
		phys = _stack.create_pchannel<mchannel_demuxer>("lora");
		phys->insert_zone_size(0);
		phys->error_control_len(error_control_len_t::ZERO);

		vchannel_demuxer * master;
		master = _stack.create_mchannel<vchannel_demuxer>(mcid_t(0x42));

		map_demuxer * virt;
		virt = _stack.create_vchannel<map_demuxer>(gvcid_t(master->channel_id, 0x00));

		_ip_channel = _stack.create_map<map_packet_acceptor>(gmapid_t(virt->channel_id, 0x00));
		_telemetry_channel = _stack.create_map<map_packet_acceptor>(gmapid_t(virt->channel_id, 0x01));

		phys->finalize();
	}

private:
	ccsds::uslp::map_packet_acceptor * _ip_channel;
	ccsds::uslp::map_packet_acceptor * _telemetry_channel;
	ccsds::uslp::input_stack _stack;
};




#endif /* ITS_SERVER_USLP_SRC_STACK_HPP_ */
