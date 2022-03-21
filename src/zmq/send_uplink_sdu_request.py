#!/usr/bin/env python3

import sys
import os
import json
import enum
import argparse
import logging
import time

import zmq


_log = logging.getLogger(__name__)


class QOS(enum.Enum):
    EXPEDITED = "expedited"
    SEQUENCE_CONTROLLED = "sequence_controlled"


def main(args):
    logging.basicConfig(level=logging.INFO, format='%(asctime)-15s %(levelname)s %(message)s')

    ctx = zmq.Context()
    # sub_socket = ctx.socket(zmq.SUB)
    # sub_ep = os.environ["ITS_GBUS_BPCS_ENDPOINT"]
    # _log.info("connecting sub to %s" % sub_ep)
    # sub_socket.connect(sub_ep)

    pub_ep = os.environ["ITS_GBUS_BSCP_ENDPOINT"]
    _log.info("connecting pub to %s" % pub_ep)
    pub_socket = ctx.socket(zmq.PUB)
    pub_socket.connect(pub_ep)


    parser = argparse.ArgumentParser("uplink sdu request sender", add_help=True)
    parser.add_argument(
        "--scid", nargs='?', type=int, help="gmap sc id", default=0x42, dest="sc_id"
    )
    parser.add_argument(
        "--vcid", nargs='?', type=int, help="gmap vc id", default=0x00, dest="vc_id"
    )
    parser.add_argument(
        '--mapid', nargs='?', type=int, help="gmap map id", required=True, dest="map_id"
    )
    parser.add_argument(
        "--qos", nargs='?', type=QOS, choices=[x.value for x in QOS], help="qos of sdu", default=QOS.EXPEDITED, dest="qos"
    )
    parser.add_argument(
        "--cookie", nargs='?', type=int, default=0, help="sdu cookie", dest="cookie")

    args = parser.parse_args(argv)
    _log.info("using channel sc_id=0x%X, vc_id=0x%X, map_id=0x%X", args.sc_id, args.vc_id, args.map_id)
    _log.info("using qos=%s", args.qos)
    _log.info("using cookie=%d", args.cookie)


    metadata = {
        "sc_id": args.sc_id, "vchannel_id": args.vc_id, "map_id": args.map_id,
        "cookie": args.cookie, "qos": args.qos.value
    }

    data = bytes([i % 0xFF for i in range(0, 200)])
    _log.info("using payload %s", data)


    pub_socket.send_multipart([
        b"uslp.uplink_sdu_request",
        json.dumps(metadata).encode("utf-8"),
        data
    ])

    time.sleep(0.1)

    return 0


if __name__ == "__main__":
    argv = sys.argv[1:]
    exit(main(argv))
