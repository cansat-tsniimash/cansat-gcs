#!/usr/bin/python3

import socket
import typing
import sys
import enum
import logging
import time
import zmq
import json
import select
import struct

from dataclasses import dataclass, asdict

from senders_common import SenderCore


_log = logging.getLogger(__name__)



@dataclass
class RadioStats:

    pkt_received: int = 0
    crc_errors: int = 0
    hdr_errors: int = 0
    error_rc64k_calib: bool = False
    error_rc13m_calib: bool = False
    error_pll_calib: bool = False
    error_adc_calib: bool = False
    error_img_calib: bool = False
    error_xosc_start: bool = False
    error_pll_lock: bool = False
    error_pa_ramp: bool = False
    srv_rx_done: int = 0
    srv_rx_frames: int = 0
    srv_tx_frames: int = 0
    current_pa_power: int = 22
    requested_pa_power: int = None


class RadioServerImitator:

    ZMQ_POLL_TIMEOUT = 0.001
    """ Максимальное время, которое мы будем реально спать в ожидании сообщений на шине """
    FRAME_RECV_TIME = 0.200
    """ Время, имитации приёма фрейма. Поступающие фреймы будут разгребваться не чаще """
    FRAME_TRANSMIT_TIME = 0.400
    """ Время отправки uplink фреймов """

    LISTEN_PERIOD = 5
    """ Время ожидания пакета в эфире """

    INSTANT_RSSI_PERIOD = 0.100
    """ Период отправки radio.rssi_instant """
    STATS_PERIOD = 1.000
    """ Период отправки radio.stats """
    UPLINK_STATE_PERIOD = 1.000
    """ Период отправки radio.uplink_state сообщений """


    def _now(self):
        now = time.time()
        seconds = int(now)
        useconds = int((now - seconds) * 1000_000)
        return seconds, useconds

    def _get_instant_rssi(self):
        return 0

    def _get_frame_rssi(self):
        """ Возвращает кортеж в последовательности
            rssi_pkt, snr_pkt, rssi_signal """
        return 0, 11, 0

    def __init__(self, pub_socket, sub_socket, uplink_socket: socket.socket):
        self.stats = RadioStats()
        self.downlink_cookie = 1 # ноль запрещен
        self.downlink_frame_no = 0 # Начнем с нуля
        self.uplink_frame_no = 0

        self.uplink_in_wait = None # type: typing.Optional[int]
        self.uplink_in_progress = None # type: typing.Optional[int]
        self.uplink_done = None # type: typing.Optional[int]
        self.uplink_failed = None # type: typing.Optional[int]
        self.uplink_buffer = bytes()

        self.uplink_socket = uplink_socket
        self.sub_socket = sub_socket
        self.pub_socket = pub_socket

        self.sent_instant_rssi_timepoint = 0.0
        self.sent_stats_timepoint = 0.0
        self.sent_uplink_state_timepoint = 0.0

        self.block_irssi = False
        self.block_stats = False

        # Подписываемся на аплинк фреймы
        sub_socket.setsockopt(zmq.SUBSCRIBE, b"radio.uplink_frame")
        # Подписываемся на запросы мощи
        sub_socket.setsockopt(zmq.SUBSCRIBE, b"radio.pa_power_request")

    def do_instant_rssi_iteration(self):
        # Уже пора отправлять?
        now = time.time()
        if now > self.sent_instant_rssi_timepoint + self.INSTANT_RSSI_PERIOD:
            # пора
            if not self.block_irssi:
                self.send_rssi_instant()
            self.sent_instant_rssi_timepoint = now

    def do_stats_iteration(self):
        # Уже пора отправлять?
        now = time.time()
        if now > self.sent_stats_timepoint + self.STATS_PERIOD:
            # пора
            if not self.block_stats:
                self.send_stats()
            self.sent_stats_timepoint = now

    def do_uplink_state_iteration(self, force:bool=False):
        now = time.time()
        if force or now > self.sent_uplink_state_timepoint + self.UPLINK_STATE_PERIOD:
            self.send_uplink_state()
            self.sent_uplink_state_timepoint = now

    def active_sleep(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            # Разгребаем что там нам прислали
            ms_untill_deadline = int(deadline - time.time() * 1000)
            zmq_timeout_ms = int(self.ZMQ_POLL_TIMEOUT * 1000)
            # Будем спать не дольше чем можем и не больше миллисекунды
            # Чтобы успевать отправлять rssi
            zmq_timeout_ms = max(zmq_timeout_ms, ms_untill_deadline)
            if self.sub_socket.poll(zmq_timeout_ms, zmq.POLLIN):
                self.process_input_message()

            # Собственно отправляем RSSI и статистику и прочую шушеру
            self.do_instant_rssi_iteration()
            self.do_stats_iteration()
            self.do_uplink_state_iteration()

    def do_recv_iteration(self):
        got_frame = False
        # Сделали вид что мы старательно что-то принимали
        self.active_sleep(self.FRAME_RECV_TIME)
        # Теперь побыстрому сходим и проверим есть ли что-то реально на аплинк сокете
        select_timeout = 0
        ready_to_read, _, _ = select.select([self.uplink_socket], [], [], select_timeout)
        if self.uplink_socket in ready_to_read:
            # О, чет пришло, скидываем на шину
            got_frame = True
            data = self.uplink_socket.recv(0xFFFF)
            frame_no, = struct.unpack("<H", data[:2])
            payload = data[2:]
            _log.info("got frame %s from uplink", frame_no)
            self.send_downlink_frame(payload=payload, checksum_valid=True, frame_no=frame_no)

        # На этом все
        return got_frame

    def do_transmit_iteration(self):
        # Нам вообще есть чего отправлять?
        if not self.uplink_buffer:
            # нет, нечего, пока
            return False

        # окей, значит есть
        # Говорим клиенту о том, что мы начинаем отправку
        data = bytes(self.uplink_buffer)
        self.uplink_buffer = bytes()
        data = struct.pack("<H", self.uplink_frame_no) + data
        self.uplink_frame_no = (self.uplink_frame_no + 1) % 0xFFFF
        self.uplink_in_progress = self.uplink_in_wait
        self.uplink_in_wait = None
        self.send_uplink_state()

        # Делаем вид что работаем
        self.active_sleep(self.FRAME_TRANSMIT_TIME)

        # Реально отправляем данные
        self.uplink_socket.send(data)
        self.uplink_done = self.uplink_in_progress
        self.uplink_in_progress = None
        self.send_uplink_state()

        # На этом всё
        return True

    def loop(self):
        # Принимаем данные (если есть чо)
        deadline = time.time() + self.LISTEN_PERIOD
        while time.time() < deadline:
            got_frame = self.do_recv_iteration()
            if got_frame:
                deadline = time.time() + self.LISTEN_PERIOD

        # Отправляем данные (если есть чо)
        self.do_transmit_iteration()


    def send_downlink_frame(
        self,
        payload: bytes, checksum_valid:bool=True, frame_no:int=None,
    ):
        if frame_no is None:
            frame_no = self.downlink_frame_no

        self.downlink_frame_no = (frame_no + 1) % 0xFFFF
        cookie = self.downlink_cookie
        self.downlink_cookie = (self.downlink_cookie + 1) or 1 # ноль запрещен

        seconds, useconds = self._now()
        rssi_pkt, snr_pkt, rssi_signal = self._get_frame_rssi()

        metadata = {
            "time_s": seconds,
            "time_us": useconds,
            "checksum_valid": checksum_valid,
            "cookie": cookie,
            "frame_no": frame_no,
            "rssi_pkt": rssi_pkt,
            "snr_pkt": snr_pkt, 
            "rssi_signal": rssi_signal,
        }

        message = [
            b"radio.downlink_frame",
            json.dumps(metadata).encode("utf-8"),
            payload
        ]

        self.pub_socket.send_multipart(message)
        _log.info("sent downlink_frame %s" , message)

        # Тепеь добрасываем rssi пакет отдельно
        metadata = {
            "time_s": seconds,
            "time_us": useconds,
            "checksum_valid": checksum_valid,
            "cookie": cookie,
            "frame_no": frame_no,
            "rssi_pkt": rssi_pkt,
            "snr_pkt": snr_pkt,
            "rssi_signal": rssi_signal,
        }

        message = [
            b"radio.rssi_packet",
            json.dumps(metadata).encode("utf-8")
        ]

        self.pub_socket.send_multipart(message)
        _log.debug("sent frame rssi data %s" % message)

        # Обновляем статиситку и прочее
        self.stats.srv_rx_frames += 1
        self.stats.pkt_received += 1

    def send_rssi_instant(self, rssi:int=0):
        seconds, useconds = self._now()
        metadata = {
            "time_s": seconds,
            "time_us": useconds,
            "rssi": rssi
        }

        message = [
            b"radio.rssi_instant",
            json.dumps(metadata).encode("utf-8"),
        ]

        self.pub_socket.send_multipart(message)
        _log.debug("sent rssi instant %s" % message)

    def send_stats(self):
        metadata = asdict(self.stats)
        seconds, useconds = self._now()
        metadata.update({
            "time_s": seconds,
            "time_us": useconds,
        })

        message = [
            b"radio.stats",
            json.dumps(metadata).encode("utf-8"),
        ]

        self.pub_socket.send_multipart(message)
        _log.debug("sent stats %s", message)

    def send_uplink_state(self):
        seconds, useconds = self._now()
        metadata = {
            "time_s": seconds,
            "time_us": useconds,
            "cookie_in_wait": self.uplink_in_wait,
            "cookie_in_progress": self.uplink_in_progress,
            "cookie_sent": self.uplink_done,
            "cookie_dropped": self.uplink_failed
        }

        message = [
            b"radio.uplink_state",
            json.dumps(metadata).encode("utf-8")
        ]

        self.pub_socket.send_multipart(message)
        _log.debug("sent uplink state" % message)

    def process_input_message(self):
        message = self.sub_socket.recv_multipart()
        topic = message[0].decode("utf-8")

        if topic == "radio.pa_power_request":
            self.process_power_request(message[1:])
        elif topic == "radio.uplink_frame":
            self.process_uplink_frame(message[1:])
        else:
            _log.error("got an unknown topic %s", topic)

    def process_power_request(self, message_parts: typing.List[bytes]):
        meta = message[0].decode("utf-8")
        meta = json.loads(meta)

        pa_power = meta["pa_power"]
        _log.info("got pa power request to %s", pa_power)
        self.stats.requested_pa_power = pa_power

    def process_uplink_frame(self, message: typing.List[bytes]):
        meta, payload = message[0].decode("utf-8"), message[1]
        meta = json.loads(meta)

        cookie = meta["cookie"]
        _log.info("got uplink frame request %s", message)
        self.uplink_in_wait = cookie
        self.uplink_buffer = payload
        self.send_uplink_state()


def parse_host_port(value: str) -> typing.Tuple[str, int]:
    host, port = value.split(":")
    port = int(port)
    return host, port


def main(argv):
    core = SenderCore("PA power request sender")
    core.setup_log(logging.INFO)
    core.arg_parser.add_argument(
        "--uplink-bind", nargs='?', type=str, required=False, dest='uplink_bind',
        help='bind uplink udp socket to'
    )
    core.arg_parser.add_argument(
        "--uplink-connect", nargs='?', type=str, required=True, dest='uplink_connect',
        help='connect uplink udp socket to'
    )
    core.arg_parser.add_argument(
        "--block-irssi", action='store_true', dest='block_irssi',
        help='disable instant rssi messages'
    )
    core.arg_parser.add_argument(
        "--block-stats", action='store_true', dest='block_stats',
        help='block radio stats messages'
    )
    core.parse_args(argv)
    core.connect_sockets()

    sub_socket = core.sub_socket
    pub_socket = core.pub_socket
    uplink_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    bind_to = parse_host_port(core.args.uplink_bind)
    connect_to = parse_host_port(core.args.uplink_connect)
    _log.info("binding uplink socket to %s", bind_to)
    _log.info("connecting uplink socket to %s", connect_to)

    uplink_socket.bind(bind_to)
    uplink_socket.connect(connect_to)

    radio = RadioServerImitator(
        sub_socket=sub_socket, pub_socket=pub_socket,
        uplink_socket=uplink_socket
    )
    radio.block_irssi = core.args.block_irssi
    radio.block_stats = core.args.block_stats

    while True:
        try:
            radio.loop()
        except KeyboardInterrupt:
            _log.info("got ctrl+c, breaking")
            break

    uplink_socket.close()
    core.close()
    return 0


if __name__ == "__main__":
    # argv = [
    #     "--bus-bscp=tcp://localhost:7777",
    #     "--bus-bpcs=tcp://localhost:7778",
    #     "--uplink-bind=0.0.0.0:2222",
    #     "--uplink-connect=localhost:2223"
    # ]
    argv = sys.argv[1:]
    exit(main(argv))
