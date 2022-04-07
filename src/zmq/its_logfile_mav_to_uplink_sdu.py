import time
import enum
import logging
import collections
from tkinter import E

from pymavlink.dialects.v20.its import MAVLink, MAVLink_bad_data

from its_logfile import LogfileReader
from senders_common import SenderCore
from ccsds.epp import QOS, EppProtocolId, EppHeader


_log = logging.getLogger(__name__)


def main(argv):
    core = SenderCore("uplink sdu request sender")
    core.setup_log()

    core.arg_parser.add_argument(
        "-i,--input,--log-file", nargs="?", type=str, dest="log_file", required=True
    )
    core.arg_parser.add_argument(
        "--speed", nargs="?", type=float, dest="speed", default=1.0
    )
    core.arg_parser.add_argument(
        "--drop-untill", nargs="?", type=float, dest="drop_untill"
    )
    core.arg_parser.add_argument(
        "--scid", nargs='?', type=int, help="gmap sc id", default=0x42, dest="sc_id"
    )
    core.arg_parser.add_argument(
        "--vcid", nargs='?', type=int, help="gmap vc id", default=0x00, dest="vc_id"
    )
    core.arg_parser.add_argument(
        '--mapid', nargs='?', type=int, help="gmap map id", required=True, dest="map_id"
    )
    core.arg_parser.add_argument(
        "--qos", nargs='?', type=QOS, choices=[x.value for x in QOS], help="qos of sdu", default=QOS.EXPEDITED, dest="qos"
    )

    args = core.parse_args(argv)
    core.connect_sockets()

    filepath = args.log_file
    if args.speed > 0:
        speed_factor = 1/args.speed
    else:
        speed_factor = 0

    drop_untill = args.drop_untill

    mavlink = MAVLink(file=None)
    mavlink.robust_parsing = True
    sdu_cookie = 0

    sc_id = args.sc_id
    vc_id = args.vc_id
    map_id = args.map_id
    qos = args.qos

    socket = core.pub_socket
    time.sleep(0.1) # нужно чуточку поспать пока сокеты соединяются

    _log.info("playing file \"%s\"" % filepath)
    last_msg_time = None
    with LogfileReader(filepath) as reader:
        while True:
            time_and_msg = reader.read()
            if not time_and_msg:
                break

            msg_time, msg = time_and_msg
            if drop_untill is not None and msg_time < drop_untill:
                continue

            if last_msg_time is not None:
                # Спим, чтобы отправлять сообщения в том же темпе как и отправитель
                to_sleep = msg_time - last_msg_time
                to_sleep = to_sleep * speed_factor
                _log.debug("sleeping for %s seconds" % to_sleep)
                time.sleep(to_sleep)

            last_msg_time = msg_time
            # А теперь делаем финт ушами
            topic = msg[0]
            if topic != b"radio.downlink_frame":
                continue

            mavlink_payload = msg[2]
            messages = mavlink.parse_buffer(mavlink_payload)
            for m in messages or []:
                if isinstance(m, MAVLink_bad_data):
                    continue

                _log.info("sending %s %s" % (last_msg_time, m.get_type()))
                msgbuf = m.get_msgbuf()
                epp_header = EppHeader()
                epp_header.protocol_id = EppProtocolId.PRIVATE
                epp_header.accomodate_to_payload_size(len(msgbuf))
                payload = epp_header.write() + msgbuf

                metadata = {
                    "sc_id": sc_id, "vchannel_id": vc_id, "map_id": map_id,
                    "cookie": sdu_cookie, "qos": qos.value
                }
                sdu_cookie += 1

                sdu_topic = "uslp.uplink_sdu_request.%d.%d.%d" % (sc_id, vc_id, map_id)
                core.pub_message(sdu_topic, metadata, payload)

if __name__ == "__main__":
    import sys
    argv = [
        "-i", "its-broker-log-combined.zmq-log",
        "--mapid", "0",
        "--bus-bscp", "tcp://localhost:2021",
        "--bus-bpcs", "tcp://localhost:2020",
        "--speed", "0"
    ]
    argv = sys.argv[1:]
    exit(main(argv))
