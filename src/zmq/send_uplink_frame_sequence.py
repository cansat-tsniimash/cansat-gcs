import sys
import os
import enum
import json

import zmq

from senders_common import SenderCore


FRAME_SIZE = 200


def send_payload(socket, cookie: int, payload: bytes):
    meta = {"cookie": cookie}
    parts = [
        b"radio.uplink_frame",
        json.dumps(meta).encode("utf-8"),
        payload
    ]

    socket.send_multipart(parts)


def main(argv):
    core = SenderCore("uplink frame test sender sequencer")
    core.setup_log()
    core.parse_args(argv)
    core.connect_sockets()

    sub_socket = core.sub_socket
    pub_socket = core.pub_socket

    sub_socket.setsockopt(zmq.SUBSCRIBE, b"radio.uplink_state")

    cookie = 0
    serial = 0
    while True:
        msgs = sub_socket.recv_multipart()

        meta = json.loads(msgs[1])
        in_wait = meta["cookie_in_wait"]
        in_progress = meta["cookie_in_progress"]
        sent = meta["cookie_sent"]
        dropped = meta["cookie_dropped"]

        print(
            f"in wait: %s, in in_progress: %s, sent: %s, dropped: %s"
            % (in_wait, in_progress, sent, dropped)
        )

        if in_wait is not None and int(in_wait) == cookie:
            continue

        cookie = (cookie + 1) & 0xFFFFFFFFFFFFFFFF

        packet = b''
        for counter in range(0, FRAME_SIZE):
            packet += bytes([serial])
            serial = (serial + 1) & 0xFF

        print("sending packet: %s" % cookie)
        send_payload(pub_socket, cookie, packet)

    core.close()
    return 0

if __name__ == "__main__":
    argv = sys.argv[1:]
    exit(main(argv))
