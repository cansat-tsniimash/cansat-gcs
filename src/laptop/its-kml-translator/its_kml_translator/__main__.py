import sys
import typing
from contextlib import closing
from http.server import HTTPServer
from http.server import BaseHTTPRequestHandler
from threading import Thread
import logging

import zmq
from pymavlink.dialects.v20.its import MAVLink, MAVLink_message, MAVLink_gps_ubx_nav_sol_message

from wgs84 import wgs84_xyz_to_latlonh
from dataset import Dataset

_log = logging.getLogger(__name__)


def run_server(dataset: Dataset, endpoint):
    class RequestHandler(BaseHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            self.dataset = dataset
            super(RequestHandler, self).__init__(*args, **kwargs)

        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-type", "application/vnd.google-earth.kml+xml")
            self.end_headers()
            data = dataset.get_string()
            self.wfile.write(data.encode("utf-8"))

    server = HTTPServer(endpoint, RequestHandler)
    _log.info("starting http server")
    server.serve_forever()


def run_zmq(dataset: Dataset, endpoint: str):
    ctx = zmq.Context()
    socket = ctx.socket(zmq.SUB)
    socket.connect(endpoint)
    _log.info("connecting zmq to %s", endpoint)
    socket.setsockopt(zmq.SUBSCRIBE, b"radio.downlink_frame")

    mav = MAVLink(None)
    mav.robust_parsing = True

    poller = zmq.Poller()
    poller.register(socket)
    _log.info("starting zmq loop")

    while True:
        events = dict(poller.poll(1000))
        if socket not in events.keys():
            continue

        topic, metadata, mav_bytes = socket.recv_multipart()
        packets = mav.parse_buffer(mav_bytes)

        process_packets(dataset, packets)


def process_packets(dataset: Dataset, packets: typing.List[MAVLink_message]):
    for packet in packets:
        if isinstance(packet, MAVLink_gps_ubx_nav_sol_message):
            x, y, z = packet.ecefX, packet.ecefY, packet.ecefZ
            lat, lon, alt = wgs84_xyz_to_latlonh(x/100, y/100, z/100)
            dataset.add_point(lon, lat, alt)
            _log.info("gps nav sol packet %s, %s, %s", lon, lat, alt)
        else:
            _log.debug("not a gps nav sol packet")


def main(argv):
    logging.basicConfig(stream=sys.stderr, level=logging.INFO)

    zmq_endpoint = "tcp://192.168.43.252:7778"
    http_endpoint = ("0.0.0.0", 8080)

    ds = Dataset()
    http_server_thread = Thread(target=run_server, args=(ds, http_endpoint), daemon=False)
    zmq_thread = Thread(target=run_zmq, args=(ds, zmq_endpoint), daemon=False)

    zmq_thread.start()
    http_server_thread.start()

    zmq_thread.join()
    http_server_thread.join()


argv = sys.argv[1:]
main(argv)
