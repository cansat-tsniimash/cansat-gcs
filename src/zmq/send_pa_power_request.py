import logging
import os
import time
import io

from senders_common import SenderCore

_log = logging.getLogger(__name__)


def main(argv):
	core = SenderCore("PA power request sender")
	core.setup_log()

	core.arg_parser.add_argument(
		'--bus-endpoint', nargs='?', type=str, help='zmq bus bscp endpoint',
		required=False, dest='bus_endpoint'
	)
	core.arg_parser.add_argument(
		'power', type=int, nargs='?', help="pa power value in dbm. Allowed values: 10, 14, 17, 20, 22"
	)

	args = core.parse_args(argv)
	core.connect_sockets()

	power = args.power

	if power not in [10, 14, 17, 20, 22]:
		_log.error("invalid pa power value: \"%s\"" % power)
		stream = io.StringIO()
		core.arg_parser.print_help(stream)
		_log.error(stream.getvalue())
		return 1

	_log.info("sending request for power '%s'" % power)
	core.pub_message("radio.pa_power_request", {"pa_power": power})

	_log.info("done")
	return 0


import sys
main(sys.argv[1:])
exit()
