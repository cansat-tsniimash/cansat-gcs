import sys
import os
import logging
import argparse
import datetime
import time

import zmq


from its_logfile import LogfileWriter

# Периодичность вызова fflush для лога (с)
LOGFILE_FLUSH_PERIOD = 1


ITS_GBUS_BPCS_ENDPOINT = os.environ.get("ITS_GBUS_BPCS_ENDPOINT", "tcp://0.0.0.0:7778")
ITS_GBUS_BSCP_ENDPOINT = os.environ.get("ITS_GBUS_BSCP_ENDPOINT", "tcp://0.0.0.0:7777")

ITS_GBUS_BPCS_ENDPOINT = ITS_GBUS_BPCS_ENDPOINT.replace("localhost", "0.0.0.0")
ITS_GBUS_BSCP_ENDPOINT = ITS_GBUS_BSCP_ENDPOINT.replace("localhost", "0.0.0.0")


_log = logging.getLogger(__name__)


def generate_logfile_name():
    now = datetime.datetime.utcnow().replace(microsecond=0)
    isostring = now.isoformat()  # строка вида 2021-04-27T23:17:31
    isostring = isostring.replace("-", "")  # Строка вида 20210427T23:17:31
    isostring = isostring.replace(":", "")  # Строка вида 20210427T231731, то что надо
    return "its-broker-log-" + isostring + ".zmq-log"


def main(argv):
    parser = argparse.ArgumentParser("ITS zmq bus broker!", add_help=True)
    parser.add_argument("--no-log", action='store_true', dest='no_log', help='do not create a logfile')
    args = parser.parse_args(argv)

    # TODO: Сделать красивую остановку по ctrl+c
    ctx = zmq.Context()

    if not args.no_log:
        logfile_name = generate_logfile_name()
        _log.info("using logfile \"%s\"", logfile_name)
        logfile_writer = LogfileWriter(logfile_name)
    else:
        _log.warning("LOG FILE IS DISABLED BY CONFIG")
        logfile_writer = None

    try:
        bus_sub = ITS_GBUS_BSCP_ENDPOINT
        bus_pub = ITS_GBUS_BPCS_ENDPOINT

        sub_socket, pub_socket = ctx.socket(zmq.SUB), ctx.socket(zmq.PUB)

        _log.info("binding sub to %s", bus_sub)
        _log.info("binding pub to %s", bus_pub)
        sub_socket.bind(bus_sub)
        pub_socket.bind(bus_pub)

        # Подписываемся на все на sub сокете
        sub_socket.setsockopt_string(zmq.SUBSCRIBE, "")

        poller = zmq.Poller()
        poller.register(sub_socket, zmq.POLLIN)
        last_log_flush_timepoint = time.time()
        while True:
            events = dict(poller.poll(1000))
            if sub_socket in events:
                msgs = sub_socket.recv_multipart()
                
                _log.info("got messages: %r", msgs)
                pub_socket.send_multipart(msgs)
                if logfile_writer:
                    logfile_writer.write(msgs)

                now = time.time()
                if logfile_writer and now - last_log_flush_timepoint >= LOGFILE_FLUSH_PERIOD:
                    logfile_writer.flush()

    except KeyboardInterrupt:
        _log.info("Shutting Down...")
        pass

    finally:
        del ctx
        if logfile_writer:
            logfile_writer.close()

    return 0


if __name__ == "__main__":
    logging.basicConfig(
        # format='%(asctime)-15s %(message)s',
        format='%(message)s', # для systemd лога таймштампы только мешают
        level=logging.INFO
    )

    argv = sys.argv[1:]
    exit(main(argv))
