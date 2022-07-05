import glob
import os
import argparse
import sys
import csv
import collections
import sys
from datetime import datetime
from wgs84 import wgs84_xyz_to_latlonh

if "MAVLINK20" not in os.environ:
    os.environ["MAVLINK20"] = "true"
if "MAVLINK_DIALECT" not in os.environ:
    os.environ["MAVLINK_DIALECT"] = "its"

from pymavlink import mavutil
from pymavlink.mavutil import mavlink


class MsgProcessor:

    def _expand(self,retval, the_list, base_name=""):
        """ Разворачивает список и каждый его элемент записывает в словарь retval
            с ключем "%s[%d]" % (base_name, i), где i - номер элемента """
        if isinstance(the_list, collections.abc.Iterable) and not isinstance(the_list, str):
            for i, value in enumerate(the_list):
                key = "%s[%d]" % (base_name, i)
                self._expand(retval, value, base_name=key)
        else:
            retval[base_name] = the_list

    def expand_arrays(self, msg_dict):
        """ Метод разворачивает элементы сообщения, которые массивы в плоские списки. """
        retval = {}
        for key, value in msg_dict.items():
            self._expand(retval=retval, the_list=value, base_name=key)

        return retval

    def __init__(self, base_path, notimestamps=False):
        # Инициализация этих откладывается до тех пор, пока мы не не определимся с полями.
        # А определимся мы в первом сообщении, как только развернем в нем все массивы
        # TODO: Можно определиться сразу по полю masg_class.array_lengths
        self.base_path = base_path
        self.stream = None
        self.writer = None
        self.message_count = 0
        self.notimestamps = notimestamps
        self.time_steady_range = (None, None) # type: typing.Tuple[typing.Optiona[int], typing.Optiona[int]]
        self.time_steady_launch = None
        self.time_launch = None
        # для фикса бага с переполнением time_steady БКУ
        self.fix_bcu_electrical_ts = False
        self.bcu_electrical_time_steady_offsets = collections.defaultdict(lambda: 0)
        self.bcu_electrical_time_steady_prevs = collections.defaultdict(lambda: 0)

    def accept(self, msg):
        self.msg_id = msg.get_msgId()
        self.msg_class = mavlink.mavlink_map[self.msg_id]

        msg_dict = msg.to_dict()
        msg_dict = self.expand_arrays(msg_dict)

        # Добавляем поля из загловка, которые нас интересуют
        hdr = msg.get_header()
        msg_dict.update({
            "seq": hdr.seq,
            "srcSystem": hdr.srcSystem,
            "srcComponent": hdr.srcComponent,
        })

        # Добавим поле с красивым таймштампом в читаемом виде
        # Если в сообщении есть time_s и time_us
        if "time_s" in msg_dict and "time_us" in msg_dict:
            ts = msg_dict["time_s"] + msg_dict["time_us"] / (1000 * 1000)
            try:
                dt = datetime.fromtimestamp(ts)
                ts_text = dt.strftime("%Y-%m-%dT%H:%M:%S")
                msg_dict.update({
                    "time_gregorian": ts_text,
                })
            except OverflowError:
                msg_dict.update({
                    "time_gregorian": "<invalid>"
                })

            if self.time_launch is not None:
                time_from_launch = msg_dict["time_s"] - self.time_launch
                mins_from_launch = (time_from_launch + msg_dict["time_us"] / (1000 * 1000)) / 60
                msg_dict["time_s_from_launch"] = time_from_launch
                msg_dict["mins_from_launch"] = mins_from_launch

        # Добавляем таймштамп из mavlog файла
        if not self.notimestamps:
            ts = msg._timestamp
            dt = datetime.fromtimestamp(ts)
            ts_text = dt.strftime("%Y-%m-%dT%H:%M:%S")
            msg_dict.update({
                "log_timestamp": ts,
                "log_timestamp_gregorian": ts_text
            })

        # Если это GPS_UBX_NAV_SOL
        if "GPS_UBX_NAV_SOL" == self.msg_class.name:
            # Пересчитываем координаты
            lat, lon, h = wgs84_xyz_to_latlonh(msg.ecefX / 100, msg.ecefY / 100, msg.ecefZ / 100)
            msg_dict.update({
                "lat": lat,
                "lon": lon,
                "h": h,
            })

        # time_steady в БКУ переполняется после 0xFFFFFFFF
        # Исправим это
        # К сожалению это все будет очень плохо выглядеть в случае, если были
        # ребуты. Но у нас их не было в этот раз, поэтому покатит
        if self.fix_bcu_electrical_ts and "ELECTRICAL_STATE" == self.msg_class.name:
            if hdr.srcSystem == 10: # 10 == БКУ
                component = hdr.srcComponent
                prev = self.bcu_electrical_time_steady_prevs[component]
                current = msg_dict["time_steady"]
                corrected = current + self.bcu_electrical_time_steady_offsets[component]
                if corrected < prev:
                    # в исходниках бку было (uint32_t)now / 1000
                    # То есть переполнение случается со смещением на 3 порядка
                    correction = int(0xFFFFFFFF / 1000)
                    corrected += correction
                    self.bcu_electrical_time_steady_offsets[component] += correction

                self.bcu_electrical_time_steady_prevs[component] = corrected
                msg_dict["time_steady"] = corrected

        if "time_steady" in msg_dict:
            left = self.time_steady_range[0]
            right = self.time_steady_range[1]
            that = msg_dict["time_steady"]
            if left is not None and that < left:
                return

            if right is not None and that > right:
                return

            if self.time_steady_launch is not None:
                time_steady = msg_dict["time_steady"]
                time_steady_from_launch = time_steady - self.time_steady_launch
                mins_steady_from_launch = time_steady_from_launch / 1000 / 60
                msg_dict["time_steady_from_launch"] = time_steady_from_launch
                msg_dict["mins_steady_from_launch"] = mins_steady_from_launch

        if "mavpackettype" in msg_dict:
            del msg_dict["mavpackettype"]  # Совершенно излишне в нашем случае

        if not self.writer:
            msg_hdr = msg.get_header()
            fstem = "%s-%s-%s" % (self.msg_class.name, msg_hdr.srcSystem, msg_hdr.srcComponent)
            fpath = os.path.join(self.base_path, fstem + ".csv")

            # Сортиранем время в начало таблички
            fieldnames = list(msg_dict.keys())

            def pull_front(column):
                if column in fieldnames:
                    fieldnames.remove(column)
                    fieldnames.insert(0, column)

            # pull_front("mins_steady_from_launch")
            # pull_front("time_steady_from_launch")
            # pull_front("time_steady")
            # pull_front("time_gregorian")
            # pull_front("time_us")
            # pull_front("time_s")

            self.stream = open(fpath, mode="w", newline='')
            self.writer = csv.DictWriter(self.stream, fieldnames=fieldnames)
            self.writer.writeheader()

        print(msg)  # Напечатаем сообщение для наглядности
        self.writer.writerow(msg_dict)
        self.message_count += 1


def main(argv):
    parser = argparse.ArgumentParser("tm parser to csvs", add_help=True)
    parser.add_argument("--input", "-i", nargs="?", dest="input", required=True)
    parser.add_argument("--output-dir", "-o", nargs="?", dest="output_dir", help="If output is None, input filename is name for output directory", default=None)
    parser.add_argument("--notimestamps", dest="notimestamps", default=False, action='store_true')
    parser.add_argument("--time-steady-start", nargs="?", type=int, dest="time_steady_start", default=None)
    parser.add_argument("--time-steady-stop", nargs="?", type=int, dest="time_steady_stop", default=None)
    parser.add_argument("--time-steady-launch", nargs="?", type=int, dest="time_steady_launch", default=None)
    parser.add_argument("--time-launch", nargs="?", type=int, dest="time_launch", default=None)
    parser.add_argument("--fix-bcu-electricals", action="store_true", dest="fix_bcu_electricals")
    args = parser.parse_args(argv)

    base_path_ = args.output_dir
    files = glob.iglob(args.input)
    notimestamps = args.notimestamps
    time_steady_range = (args.time_steady_start, args.time_steady_stop)
    time_steady_launch = args.time_steady_launch
    time_launch = args.time_launch
    fix_bcu_electricals = args.fix_bcu_electricals

    for f in files:
        base_path = base_path_
        if not base_path:
            base_path = os.path.splitext(f)[0] + "_mav_csv"

        if not os.path.isdir(base_path):
            os.makedirs(base_path, exist_ok=True)

        con = mavutil.mavlogfile(f, notimestamps=notimestamps)
        processors = {}
        bad_data_bytes = 0

        while True:
            msg = con.recv_msg()
            if not msg:
                break

            msg_id = msg.get_msgId()
            if msg_id < 0:
                bad_data_bytes += 1
                continue

            msg_hdr = msg.get_header()
            msg_key = "%s-%s-%s" % (msg_id, msg_hdr.srcSystem, msg_hdr.srcComponent)
            if msg_key not in processors:
                processor = MsgProcessor(base_path, notimestamps=notimestamps)
                processor.time_steady_range = time_steady_range
                processor.time_steady_launch = time_steady_launch
                processor.fix_bcu_electrical_ts = fix_bcu_electricals
                processor.time_launch = time_launch
                processors.update({msg_key: processor})

            processor = processors[msg_key]
            processor.accept(msg)

        print("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-")
        print("Stats in total for file %s:" % f, file=sys.stderr)
        for msg_key, processor in processors.items():
            print(
                "%s-%s: %d messages" % (processor.msg_class.name, msg_key, processor.message_count),
                file=sys.stderr
            )
        print("bad data %s bytes in total" % bad_data_bytes, file=sys.stderr)


if __name__ == "__main__":
    exit(main(sys.argv[1:]))