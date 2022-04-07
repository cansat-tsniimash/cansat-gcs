#!/usr/bin/env python3

import sys
import enum
import logging
import time
import typing
import struct

from ccsds.epp import EppHeader, EppProtocolId, QOS

from senders_common import SenderCore


_log = logging.getLogger(__name__)


def main(args):
    core = SenderCore("uplink sdu request sender")
    core.setup_log()
    
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
    core.arg_parser.add_argument(
        "--cookie", nargs='?', type=int, default=0, help="sdu cookie", dest="cookie")

    args = core.parse_args(argv)
    _log.info("using channel sc_id=0x%X, vc_id=0x%X, map_id=0x%X", args.sc_id, args.vc_id, args.map_id)
    _log.info("using qos=%s", args.qos)
    _log.info("using cookie=%d", args.cookie)

    core.connect_sockets()
 
    metadata = {
        "sc_id": args.sc_id, "vchannel_id": args.vc_id, "map_id": args.map_id,
        "cookie": args.cookie, "qos": args.qos.value
    }

    data = bytes([i % 0xFF for i in range(0, 500)])

    # Приклеиваем EPP заголовок
    header = EppHeader()
    header.protocol_id = EppProtocolId.PRIVATE
    header.accomodate_to_payload_size(len(data))
    data = header.write() + data

    _log.info("using payload %s", data)

    topic = "uslp.uplink_sdu_request.%d-%d-%d" % (args.sc_id, args.vc_id, args.map_id)
    core.pub_message(topic, metadata, data)
    time.sleep(0.2)
    core.close()
    return 0


if __name__ == "__main__":
    argv = sys.argv[1:]
    exit(main(argv))
