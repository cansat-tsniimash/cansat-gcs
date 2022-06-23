import struct
import enum
import typing


class EppProtocolId(enum.IntEnum):
    """ Идентификаторы протоколов используемых в EPP
        Тут не все, только интересные нам """

    IDLE = 0x00
    """ Encapsulation Idle Packet (the encapsulation data field, if present, contains no protocol data but only idle data) """
    IPE = 0x02
    """ Internet Protocol Extension (IPE) """
    EXTENDED = 0x06
    """ Extended Protocol ID for Encapsulation Packet Protocol """
    PRIVATE = 0x07
    """ Mission-specific, privately defined data """

class EppHeader:
    PVN = 0x07
    """ packet version number для этих пакетов
        занимают три бита в старшем разряде первого байта """

    MINIMUM_SIZE = 1
    """ Вот такие маленькие заголовки бывают """
    MAXIMUM_SIZE = 8
    """ вот такие боьшие заголовки бывают """

    @classmethod
    def probe_header_size(cls, target: typing.Union[bytes, int]):
        if isinstance(target, bytes):
            if not target:
                return 0

            target = bytes[0]

        pvn_in_buffer = (target >> 5) & 0x07
        if pvn_in_buffer != cls.PVN:
            # не похоже на наш пакет
            return 0

        # Если подошло, то смотрим длину
        len_of_len = (target >> 0) & 0x03;
        cases = { 0x00: 1, 0x01: 2, 0x02: 4, 0x03: 8}
        return cases[len_of_len]

    def __init__(self):
        self.protocol_id = 0
        self.packet_size = 0
        self.user_defined_field = None
        self.protocol_id_extension = None
        self.ccsds_field = None

    def size(self):
        # Если ccsds_field есть - заголовок сразу огромный
        if self.ccsds_field is not None:
            return 8

        # Дальше смотрим по длине пакета
        # Если не влезает в 3 байта, то тоже огромный заголовок
        if self.packet_size > 0xFFFFFF:
            return 8

        # 8 проверили, смотрим варианты на 4
        if self.protocol_id == EppProtocolId.EXTENDED:
            return 4
        if self.protocol_id_extension is not None:
            return 4
        if self.user_defined_field is not None:
            return 4
        if self.packet_size > 0xFF:
            return 4

        # Теперь варианты на 2 байта
        # Только если длина вообще есть
        if self.packet_size > 0:
            return 2

        # А если нет двойки, то будет единица
        return 1

    def real_packet_size(self) -> int:
        return self.packet_size + 1

    def set_real_packet_size(self, value:int):
        if 0 == value:
            raise ValueError("EPP real packet size can not be zero")

        max_size = 0xFF_FF_FF_FF + 1
        if value > max_size:
            raise ValueError(
                "EPP real packet size can not be bigger than %s (%s specified)"
                % (max_size, value)
            )

        self.packet_size = value - 1

    def accomodate_to_payload_size(self, payload_size):
        header_size = self.size()
        self.packet_size = payload_size + header_size - 1

        while True:
            self.packet_size = payload_size + header_size - 1
            header_size2 = self.size()
            if header_size == header_size2:
                break

            header_size = header_size2

        return self.real_packet_size()

    def write(self) -> bytes:
        retval = bytearray()

        header_size = self.size()
        if 1 == header_size:
            len_of_len = 0x00
        elif 2 == header_size:
            len_of_len = 0x01
            # Пишем только длину
            retval.append(self.packet_size)
        elif 4 == header_size:
            len_of_len = 0x02
            # Пишем расширенные поля
            retval.append(self._make_second_byte())
            # А потом длину пакета в BE
            retval += struct.pack(">H", self.packet_size)
        elif 8 == header_size:
            len_of_len = 0x03
            # расширенные поля
            retval.append(self._make_second_byte())
            # CCSDS поле
            retval += struct.packet(">H", self.ccsds_field or 0)
            # и вот теперь длину пакета
            retval += struct.packet(">L", self.packet_size)
        else:
            raise RuntimeError("Invalid EPP header size in write()")

        byte1 = 0
        byte1 |= (self.PVN & 0x07) << 5
        byte1 |= (self.protocol_id & 0x07) << 2
        byte1 |= (len_of_len & 0x03) << 0
        retval.insert(0, byte1)
        return bytes(retval)

    def read(self, data: bytes):
        header_size = EppHeader.probe_header_size(data)
        if not header_size:
            raise ValueError("Supplied data is not a valid EEP header as it seems")

        if header_size > len(bytes):
            raise ValueError(
                "EPP header size %s is greater than size of specified buffer %s"
                % (header_size, len(data))
            )

        byte1 = data[0]
        self.protocol_id = (byte1 >> 2) & 0x07

        self.user_defined_field = None
        self.protocol_id_extension = None
        self.ccsds_field = None
        if 1 == header_size:
            self.packet_size = 0
        elif 2 == header_size:
            self.packet_size = data[1]
        elif 4 == header_size:
            self._load_second_byte(data[1])
            self.packet_size = struct.unpack(">H", data[2:4])
        elif 8 == header_size:
            self._load_second_byte(data[1])
            self.ccsds_field = struct.unpack(">H", data[2:4])
            self.packet_size = struct.unpack(">L", data[5:7])
        else:
            raise RuntimeError("Invalid EPP header size in read()")

    def _make_second_byte(self) -> int:
        pid_ext = self.protocol_id_extension or 0x00
        usr_fld = self.user_defined_field or 0x00
        byte2 = 0
        byte2 |= (pid_ext & 0x0F) << 0
        byte2 |= (usr_fld & 0x0F) << 4
        return byte2

    def _load_second_byte(self, byte2: int):
        self.protocol_id_extension = (byte2 >> 0) & 0x0F
        self.user_defined_field = (byte2 >> 4) & 0x0F


class QOS(enum.Enum):
    EXPEDITED = "expedited"
    SEQUENCE_CONTROLLED = "sequence_controlled"
