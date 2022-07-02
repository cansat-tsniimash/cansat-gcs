import gpiod
import zmq
import time
import os

from pymavlink.dialects.v20.its import MAVLink, MAVLink_bad_data, MAVLink_gps_ubx_nav_sol_message


MAVLINK_DATA_TOPIC = b"radio.downlink_frame"
LED_CHIP = "/dev/gpiochip0"
TM_LED_LINE = 21
FIX_LED_LINE = 20


def main():
    ctx = zmq.Context()
    sub_socket = ctx.socket(zmq.SUB)

    sub_ep = os.environ["ITS_GBUS_BPCS_ENDPOINT"]
    print("connecting to %s" % sub_ep)

    sub_socket.connect(sub_ep)
    sub_socket.setsockopt(zmq.SUBSCRIBE, MAVLINK_DATA_TOPIC)
    poller = zmq.Poller()
    poller.register(sub_socket, zmq.POLLIN)

    chip = gpiod.Chip(LED_CHIP)
    tm_led_lines = chip.get_lines([TM_LED_LINE, FIX_LED_LINE])
    tm_led_lines.request(consumer="its_tele_blink", type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0, 0])

    mav = MAVLink(file=None)
    mav.robust_parsing = True

    while True:
        tm_led_value = 0
        fix_led_value = 0

        events = dict(poller.poll(int(0.5*1000)))
        if sub_socket in events:
            msgs = sub_socket.recv_multipart()
            tm_led_value = 1
            print("got msgs %s", msgs)

            mavlink_payload = msgs[2]
            packets = mav.parse_buffer(mavlink_payload)
            for p in packets or []:
                if isinstance(p, MAVLink_gps_ubx_nav_sol_message):
                    fix = p.gpsFix
                    if fix != 0:
                        fix_led_value = 1

        led_lines.set_values([tm_led_value, fix_led_value])

    del ctx
    return 0


if __name__ == "__main__":
    exit(main())
