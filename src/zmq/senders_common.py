#!/user/bin/python3

import os
import logging
import argparse
import zmq
import json
import time


BUS_BSCP_ENDPOINT = "ITS_GBUS_BSCP_ENDPOINT"
BUS_BPCS_ENDPOINT = "ITS_GBUS_BPCS_ENDPOINT"


_log = logging.getLogger(__name__)


class SenderCore:

	@staticmethod
	def setup_log():
		logging.basicConfig(level=logging.INFO, format='%(asctime)-15s %(levelname)s %(message)s')

	def __init__(self, app_name: str):
		self.args = None
		self.arg_parser = argparse.ArgumentParser(app_name, add_help=True)
		self.arg_parser.add_argument(
			'--bus-bscp', nargs='?', type=str, help='zmq bus bscp endpoint',
			required=False, dest='bus_bscp'
		)
		self.arg_parser.add_argument(
			'--bus-bpcs', nargs='?', type=str, help='zmq bus bpcs endpoint',
			required=False, dest='bus_bpcs'
		)

		self.zmq_ctx = zmq.Context()
		self.pub_socket = self.zmq_ctx.socket(zmq.PUB)
		self.sub_socket = self.zmq_ctx.socket(zmq.SUB)

	def parse_args(self, argv):
		args = self.arg_parser.parse_args(argv)

		if not args.bus_bscp:
			# Попробуем из енва
			args.bus_bscp = os.environ.get(BUS_BSCP_ENDPOINT, None)
			if not args.bus_bscp:
				raise ValueError(
					"Unable to load config. There is no bus bscp endpoint in config and in env"
				)

		if not args.bus_bpcs:
			# Попробуем из енва
			args.bus_bpcs = os.environ.get(BUS_BPCS_ENDPOINT, None)
			if not args.bus_bpcs:
				raise ValueError(
					"Unable to load config. There is no bus bpcs endpoint in config and in env"
				)

		self.args = args
		return self.args

	def connect_sockets(self):
		assert self.args

		_log.info("connecting to bpcs %s, bscp %s", self.args.bus_bpcs, self.args.bus_bscp)
		self.pub_socket.connect(self.args.bus_bscp)
		self.sub_socket.connect(self.args.bus_bpcs)

		time.sleep(0.1) # Чтобы сокеты успели соединиться

	def pub_message(self, topic: str, metadata: dict, payload: bytes = None):
		message = [
			topic.encode('utf-8'),
			json.dumps(metadata).encode('utf-8')
		]

		if payload is not None:
			message.append(payload)

		self.pub_socket.send_multipart(message)

	def close(self):
		self.pub_socket.close()
		self.sub_socket.close()
		del self.zmq_ctx
