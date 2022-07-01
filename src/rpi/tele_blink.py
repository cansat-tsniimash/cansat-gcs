import gpiod
import zmq
import time
import os


MAVLINK_DATA_TOPIC = b"radio.downlink_frame"
LED_CHIP = "/dev/gpiochip0"
LED_LINE = 21


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
    led_lines = chip.get_lines([LED_LINE])
    led_lines.request(consumer="its_tele_blink", type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])

    # while True:
    #     led_lines.set_values([1])
    #     time.sleep(0.5)
    #     led_lines.set_values([0])
    #     time.sleep(1)

    while True:
        events = dict(poller.poll(int(0.5*1000)))
        if sub_socket in events:
            msgs = sub_socket.recv_multipart()
            print("got msgs %s", msgs)
            led_lines.set_values([1])
        else:
            ed_lines.set_values([0])

    del ctx
    return 0


if __name__ == "__main__":
    exit(main())
