#include "stack.hpp"


ostack::ostack()
{
	using namespace ccsds;
	using namespace ccsds::uslp;

	mchannel_rr_muxer * phys = create_pchannel<mchannel_rr_muxer>("lora");
	phys->frame_size(RADIO_FRAME_SIZE);
	phys->error_control_len(error_control_len_t::ZERO);

	auto master = create_mchannel<vchannel_rr_muxer>(mcid_t(SPACECRAFT_ID));
	master->id_is_destination(true);

	auto virt = create_vchannel<map_rr_muxer>(gvcid_t(master->channel_id, UPLINK_VCHANNEL_ID));
	virt->frame_seq_no_len(2);

	auto * command_channel = create_map<map_access_emitter>(gmapid_t(virt->channel_id, UPLINK_TELECOMMAND_MAPID));
	auto * ip_channel = create_map<map_packet_emitter>(gmapid_t(virt->channel_id, UPLINK_IP_MAPID));

	phys->finalize();
}


istack::istack()
{
	using namespace ccsds;
	using namespace ccsds::uslp;

	mchannel_demuxer * phys;
	phys = create_pchannel<mchannel_demuxer>("lora");
	phys->insert_zone_size(0);
	phys->error_control_len(error_control_len_t::ZERO);

	vchannel_demuxer * master;
	master = create_mchannel<vchannel_demuxer>(mcid_t(SPACECRAFT_ID));

	map_demuxer * virt;
	virt = create_vchannel<map_demuxer>(gvcid_t(master->channel_id, DOWNLINK_VCHANNEL_ID));

	auto * telemetry_channel = create_map<map_packet_acceptor>(gmapid_t(virt->channel_id, DOWNLINK_TELEMETERY_MAPID));
	auto * ip_channel = create_map<map_packet_acceptor>(gmapid_t(virt->channel_id, DOWNLINK_IP_MAPID));

	phys->finalize();
}
