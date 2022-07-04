import typing
import os
os.environ['MAVLINK_DIALECT'] = "its"
os.environ['MAVLINK20'] = "its"
from pymavlink.dialects.v20 import its as its_mav
from pymavlink import mavutil
import time
import re
import math
import numpy as NumPy
import struct
import zmq
import json
import logging
import socket


_log = logging.getLogger(__name__)

from source.functions.wgs84 import wgs84_xyz_to_latlonh as wgs84_conv

def generate_log_file_name():
  return time.strftime("StrelA-MS_log_%d-%m-%Y_%H-%M-%S", time.localtime())


class Message():
    def __init__(self, message_id, source_id, msg_time, msg_data):
        self.message_id = message_id
        self.source_id = source_id
        self.data = msg_data
        self.time = msg_time
        self.creation_time = time.time()

    def get_message_id(self):
        return self.message_id

    def get_source_id(self):
        return self.source_id

    def get_time(self):
        return self.time

    def get_data_dict(self):
        return self.data

    def get_creation_time(self):
        return self.creation_time


class MAVDataSource():
    def __init__(self, connection_str="tcp://127.0.0.1:7778", log_path="./", notimestamps=True):
        self.notimestamps = notimestamps
        self.connection_str = connection_str
        self.log_path = log_path
        self.ground_coords = NumPy.array((0, 0, 0)).reshape((3, 1))
        self.target_coords = NumPy.array((0, 0, 0)).reshape((3, 1))

    def start(self):
        self.connection = mavutil.mavlink_connection(self.connection_str)
        self.log = open(self.log_path + generate_log_file_name + '.mav', 'wb')

    def read_data(self):
        msg = self.connection.recv_match()
        if (msg is None):
            raise RuntimeError("No Message")
        if not self.notimestamps:
            self.log.write(struct.pack("<Q", int(time.time() * 1.0e6) & -3))
        self.log.write(msg.get_msgbuf())

        if msg.get_type() == "BAD_DATA":
            raise TypeError("BAD_DATA message received")

        data = self.get_data([msg])
        if data is None:
            raise TypeError("Message type not supported")

        return data

    def get_data(self, msgs):
        msg_list = []
        for msg in msgs:
            if msg.get_type() == "BAD_DATA":
                msg_list.append(Message(message_id=msg.get_type(),
                                        source_id='0_0',
                                        msg_time=time.time(),
                                        msg_data={}))
                continue
            data = msg.to_dict()
            data.pop('mavpackettype', None)
            data.pop('time_steady', None)
            msg_time = data.pop("time_s", time.time()) + data.pop("time_us", 0)/1000000

            for item in list(data.items()):
                if isinstance(item[1], list):
                    data.pop(item[0])
                    for i in range(len(item[1])):
                        data.update([[item[0] + '[' + str(i) + ']', item[1][i]]])

            if msg.get_type() == "GPS_UBX_NAV_SOL":
                gps = wgs84_conv(msg.ecefX / 100, msg.ecefY / 100, msg.ecefZ / 100)
                self.target_coords = NumPy.array((msg.ecefX / 100, msg.ecefY / 100, msg.ecefZ / 100)).reshape((3, 1))
                data.update([['lat', gps[0]],
                             ['lon', gps[1]],
                             ['alt', gps[2]]])
                data['ecefX'] /= 100
                data['ecefY'] /= 100
                data['ecefZ'] /= 100
                msg_list.append(Message(message_id='TARGET_DISTANCE',
                                        source_id='0_0',
                                        msg_time=msg_time,
                                        msg_data={'distance':NumPy.linalg.norm((self.target_coords - self.ground_coords))}))
            elif msg.get_type() == 'PLD_DOSIM_DATA':
                gain =  1000 * 60 * 60
                delta = msg.delta_time
                if msg.delta_time != 0:
                    data.update([['dose_max', (msg.count_tick / 44) / msg.delta_time * gain],
                                 ['dose_min', (msg.count_tick / 52) / msg.delta_time * gain]])
                else:
                    data.update([['dose_max', 0],
                                 ['dose_min', 0]])
            elif msg.get_type() == 'AS_STATE':
                self.ground_coords = NumPy.array(msg.ecef).reshape((3, 1))
                msg_list.append(Message(message_id='TARGET_DISTANCE',
                                        source_id='0_0',
                                        msg_time=msg_time,
                                        msg_data={'distance':(NumPy.linalg.norm((self.target_coords - self.ground_coords)))}))

            header = msg.get_header()
            msg_list.append(Message(message_id=msg.get_type(),
                                    source_id=(str(header.srcSystem) + '_' + str(header.srcComponent)),
                                    msg_time=msg_time,
                                    msg_data=data))
        return msg_list

    def stop(self):
        self.connection.close()
        self.log.close()
        self.ground_coords = NumPy.array((0, 0, 0)).reshape((3, 1))
        self.target_coords = NumPy.array((0, 0, 0)).reshape((3, 1))


class MAVLogDataSource():
    def __init__(self, log_path, real_time=0, time_delay=0.01, notimestamps=True, packet_num_shift=0, packet_count=0):
        self.notimestamps = notimestamps
        self.log_path = log_path
        self.real_time = real_time
        self.time_delay = time_delay
        self.mav_data_sourse = MAVDataSource()
        self.packet_num_shift = packet_num_shift
        self.packet_count = packet_count

    def start(self):
        self.connection = mavutil.mavlogfile(self.log_path, notimestamps=self.notimestamps)
        self.time_shift = None
        self.time_start = None
        self.count = 0

    def read_data(self):
        msg = self.connection.recv_match()
        if (msg is None):
            raise RuntimeError("No Message")

        data = self.mav_data_sourse.get_data([msg])
        if data is None:
            raise TypeError("Message type not supported")

        self.count += 1
        if self.count < self.packet_num_shift:
            return []

        if self.packet_count > 0:
            if self.count > (self.packet_num_shift + self.packet_count):
                return []

        if self.real_time:
          if self.time_shift is None:
            self.time_shift = datetime.fromtimestamp(msg._timestamp)
            if self.time_start is None:
                self.time_start = time.time()
            while (time.time() - self.time_start) < (datetime.fromtimestamp(msg._timestamp) - self.time_shift):
                pass
        else:
            time.sleep(self.time_delay)
        return data

    def stop(self):
        self.connection.close()


class ZMQDataSource():

    def _extract_mdt_time(self, zmq_message: typing.List[bytes]) -> float:
        """ Извлекает из сообщения на ZMQ шине таймштамп если он там есть
            Если не удалось - возвращает time.time() """
        mdt = zmq_message[1]
        parsed = json.loads(mdt)  # type: typing.Dict

        timestamp = None
        if "time_s" in parsed and "time_us" in parsed:
            try:
                timestamp = int(parsed["time_s"]) + float(parsed["time_us"])/1000/1000
            except:
                pass

        return timestamp if timestamp is not None else time.time()

    def __init__(self, bus_bpcs="tcp://127.0.0.1:7778", topics=[], log_path="./", notimestamps=True):
        self.notimestamps = notimestamps
        self.bus_bpcs = bus_bpcs
        self.topics = topics
        self.log_path = log_path
        self.pkt_num = None
        self.pkt_count = 0
        self.mav_data_sourse = MAVDataSource()

    def start(self):
        self.zmq_ctx = zmq.Context()

        self.sub_socket = self.zmq_ctx.socket(zmq.SUB)
        self.sub_socket.connect(self.bus_bpcs)
        for topic in self.topics:
            self.sub_socket.setsockopt_string(zmq.SUBSCRIBE, topic)

        self.poller = zmq.Poller()
        self.poller.register(self.sub_socket, zmq.POLLIN)

        self.log = open(self.log_path + generate_log_file_name() + '.mav', 'wb')

        self.default_mav = its_mav.MAVLink(file=None)
        self.default_mav.robust_parsing = True
        self.mav_dict = {}

    def parse_mav_buffer(self, key, buf):
        mav = self.mav_dict.get(key, None)
        if mav is None:
            mav = its_mav.MAVLink(file=None)
            mav.robust_parsing = True
            self.mav_dict.update([(key, mav)])

        return mav.parse_buffer(buf)

    def read_data(self):
        events = dict(self.poller.poll(10))
        if self.sub_socket in events:
            zmq_msg = self.sub_socket.recv_multipart()
            data = []

            if zmq_msg[0] == b'radio.rssi_instant':
                return [Message(message_id='radio.rssi_instant',
                                source_id=('1_0'),
                                msg_time=self._extract_mdt_time(zmq_msg),
                                msg_data=json.loads(zmq_msg[1].decode("utf-8")))]
            elif zmq_msg[0] == b'radio.rssi_packet':
                return [Message(message_id='radio.rssi_packet',
                                source_id=('1_0'),
                                msg_time=self._extract_mdt_time(zmq_msg),
                                msg_data=json.loads(zmq_msg[1].decode("utf-8")))]
            elif zmq_msg[0] == b'radio.stats':
                return [Message(message_id='radio.stats',
                                source_id=('1_0'),
                                msg_time=self._extract_mdt_time(zmq_msg),
                                msg_data=json.loads(zmq_msg[1].decode("utf-8")))]

            elif (zmq_msg[0] == b'radio.downlink_frame') or (zmq_msg[0] == b'sdr.downlink_frame'):
                num = json.loads(zmq_msg[1].decode("utf-8")).get("frame_no", None)
                if (num is not None) and (self.pkt_num is not None):
                    if ((self.pkt_num + 1) < num):
                        self.pkt_count += num - (self.pkt_num + 1)
                self.pkt_num = num
                data.append(Message(message_id='LOST_MESSAGES',
                                    source_id=('0_0'),
                                    msg_time=time.time(),
                                    msg_data={'count': self.pkt_count,
                                              'num': self.pkt_num}))

            if len(zmq_msg) > 2:
                msg_buf = zmq_msg[2]

                if not self.notimestamps:
                    self.log.write(struct.pack("<Q", int(time.time() * 1.0e6) & -3))
                self.log.write(msg_buf)

                msg = self.parse_mav_buffer(zmq_msg[0].decode('utf-8'), msg_buf)
                _log.debug("got message: %s", list([str(x) for x in msg]))

                if msg is None:
                    raise RuntimeError("No Message")
                data.extend(self.mav_data_sourse.get_data(msg))

            return data
        else:
            raise RuntimeError("No Message")

    def stop(self):
        self.pkt_count = 0
        self.log.close()






def BME280_parse(data: bytes):
    unpacked = struct.unpack("<BhHIIH", data[:15])
    flag = unpacked[0]
    bme280_temp = unpacked[1] / 10
    num = unpacked[2]
    bme280_pres = unpacked[3]
    time = unpacked[4]
    print("BME280")
    return [Message(message_id='BME280',
                    source_id=('monolit'),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'temp':bme280_temp, 'pres':bme280_pres})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)

def doZe_parse(data: bytes):
        unpacked = struct.unpack("<BHIIIIH", data[:21])
        flag = unpacked[0]
        num = unpacked[1]
        time = unpacked[2]
        time_now = unpacked[3]
        tick_min = unpacked[4]
        tick_sum = unpacked[5]
        print("DOSE")
        return [Message(message_id='doZe',
                    source_id=('monolit'),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'tick_min':tick_min, 'tick_now':time_now})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)

def GPS_parse(data: bytes):
       unpacked = struct.unpack("<BBhHffIIIH", data[:28])
       flag = unpacked[0]
       gps_fix = unpacked[1]
       gps_altitude = unpacked[2] / 10
       num = unpacked[3]
       gps_latitude = unpacked[4]
       gps_longtitude = unpacked[5]
       time = unpacked[6]
       gps_time_s = unpacked[7]
       gps_time_us = unpacked[8]
       print("GPS")
       return [Message(message_id='GPS',
                    source_id=('monolit'),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'alt':gps_altitude, 'lat' :gps_latitude, 'lon' :gps_longtitude, 'fix' :gps_fix})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)

def DS_parse(data: bytes):
        unpacked = struct.unpack("<BBHHIH", data[:12])
        flag = unpacked[0]
        state_apparate = unpacked[1]
        num = unpacked[2]     
        DS18temp = unpacked[3]
        print("DS18")
        return [Message(message_id='DS18',
                    source_id=('monolit'),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'DS18temp' :DS18temp})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)

def Orient_parse(data: bytes):
        unpacked = struct.unpack("<BhhhhhhhhhHIH", data[:27])
        flag = unpacked[0]
        acc_x = unpacked[1] / 1000
        acc_y = unpacked[2] / 1000
        acc_z = unpacked[3] / 1000
        gyro_x = unpacked[4] / 100
        gyro_y = unpacked[5] / 100
        gyro_z = unpacked[6] / 100
        mag_x = unpacked[7] / 1000
        mag_y = unpacked[8] / 1000
        mag_z = unpacked[9] / 1000
        num = unpacked[10]
        time = unpacked[11]
        print("Orient")
        return [Message(message_id='Orient',
                    source_id=('monolit'),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'acc_x' :acc_x, 'acc_y' :acc_y, 'acc_z' :acc_z, 'gyro_x' :gyro_x, 'gyro_y' :gyro_y, 'gyro_z' :gyro_z, 'mag_x' :mag_x, 'mag_y' :mag_y, 'mag_z' :mag_z})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)













class MonolitRadioSentenceSource():
    def __init__(self, bus_bpcs="tcp://127.0.0.1:7778"):
        self.bus_bpcs = bus_bpcs
        print(bus_bpcs)

    def start(self):
        self.zmq_ctx = zmq.Context()

        self.sub_socket = self.zmq_ctx.socket(zmq.SUB)
        self.sub_socket.connect(self.bus_bpcs)
        self.sub_socket.setsockopt_string(zmq.SUBSCRIBE, "")

        self.poller = zmq.Poller()
        self.poller.register(self.sub_socket, zmq.POLLIN)
        print("start")

    def read_data(self):
        events = dict(self.poller.poll(10))
        print(events)
        if self.sub_socket in events:
            zmq_msg = self.sub_socket.recv_multipart()#получаем сообщение
            raw_bytes = zmq_msg[0]
            print(raw_bytes)
            data = []

            if raw_bytes[0] == 117:
                return BME280_parse(raw_bytes)
            elif raw_bytes[0] == 66:
                return doZe_parse(raw_bytes)
            elif raw_bytes[0] == 71:
                return GPS_parse(raw_bytes)
            elif raw_bytes[0] == 99:
                return DS_parse(raw_bytes)
            elif raw_bytes[0] == 228:
                return Orient_parse(raw_bytes)

            return data
        else:
            raise RuntimeError("No Message")

    def stop(self):
        pass
        print("stop")





class AtlasRadioSentenceSource():
    def __init__(self, bus_bpcs="tcp://127.0.0.1:7778"):
        self.bus_bpcs = bus_bpcs

    def start(self):
        self.zmq_ctx = zmq.Context()

        self.sub_socket = self.zmq_ctx.socket(zmq.SUB)
        self.sub_socket.connect(self.bus_bpcs)
        self.sub_socket.setsockopt_string(zmq.SUBSCRIBE, "")

        self.poller = zmq.Poller()
        self.poller.register(self.sub_socket, zmq.POLLIN)

    def read_data(self):
        events = dict(self.poller.poll(10))
        if self.sub_socket in events:
            zmq_msg = self.sub_socket.recv_multipart()#получаем сообщение
            raw_bytes = zmq_msg[0]

            if raw_bytes[0] == 0xff:
                return MA_type_1_parse(raw_bytes)
            elif raw_bytes[0] == 0xfe:
                return MA_type_2_parse(raw_bytes)
            elif raw_bytes[0] == 0xfa:
                return DA_type_1_parse(raw_bytes)
            elif raw_bytes[0] == 0xfb:
                return DA_type_2_parse(raw_bytes)


            return []
        else:
            raise RuntimeError("No Message")

    def stop(self):
        pass
        print("stop")






    def MA_type_1_parse(data: bytes)
    unpacked = struct.unpack("<BHI2fh2fhHBH", data[:32])
    flag = unpacked[0]
    num = unpacked[1]
    time = unpacked[2]
    bme280_pres = unpacked[3]
    bme280_temp = unpacked[4] / 100
    ds18b20_temp = unpacked[5]
    latitude = unpacked[6]
    longitude = unpacked[7]
    height = unpacked[8]
    print("BME280")
    return [Message(message_id='BME280',
                    source_id=('atlas_ma_type_1'),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'temp':bme280_temp, 'pres':bme280_pres, 'ds_temp':ds18b20_temp, 'lat':latitude, 'long':longitude, 'hei':height})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)


    def MA_type_2_parse(data: bytes)
    unpacked = struct.unpack("<BHI9hfBh", data[:32])
    flag = unpacked[0]
    time = unpacked[2]
    num = unpacked[1]
    acc_mg_1 = unpacked[3]
    acc_mg_2 = unpacked[4]
    acc_mg_3 = unpacked[5]
    gyro_mdps_1 = unpacked[6]
    gyro_mdps_2 = unpacked[7]
    gyro_mdps_3 = unpacked[8]
    lism_1 = unpacked[9]
    lism_2 = unpacked[10]
    lism_3 = unpacked[11]
    phort_res = unpacked[12]
    print("BME280")
    return [Message(message_id='ACC',
                    source_id=('atlas_ma_type_2'),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'acc_1':acc_mg_1, 'acc_2':acc_mg_2, 'acc_3': acc_mg_3, 'gyro_1':gyro_mdps_1, 'gyro_2':gyro_mdps_2, 'gyro_3': gyro_mdps_3, 'lism_1':lism_1, 'lism_2':lism_2, 'lism_3':lism_3})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)

    def DA_type_1_parse(data: bytes)
    unpacked = struct.unpack("<BBHIIhH6hH", data[:15])
    flag = unpacked[0]
    bme280_temp = unpacked[5] / 100
    num = unpacked[2]
    bme280_pres = unpacked[4]
    time = unpacked[3]
    print("BME280")
    return [Message(message_id='1',
                    source_id=('atlas_da'+ str(unpacked[1])),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'temp':bme280_temp, 'pres':bme280_pres})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)

    def DA_type_2_parse(data: bytes)
    unpacked = struct.unpack("<BBHI3fBBH", data[:15])
    flag = unpacked[0]
    num = unpacked[2]
    time = unpacked[3]
    latitude = unpacked[4]
    longitude = unpacked[5]
    height = unpacked[6]
    print("GPS")
    return [Message(message_id='2',
                    source_id=('atlas_da'+ str(unpacked[1])),
                    msg_time=time,#Vремя с контроллера (в нижней части графика) pavel_gps
                    msg_data={'num':num, 'temp':bme280_temp, 'pres':bme280_pres})]#fилд айди (данные из пакетова, кот0-ые пойдут на график)1