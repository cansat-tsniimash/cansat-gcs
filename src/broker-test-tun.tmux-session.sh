#!/bin/sh

export ITS_GBUS_BPCS_ENDPOINT=tcp://localhost:7778
export ITS_GBUS_BSCP_ENDPOINT=tcp://localhost:7777

tmux new-session -d -n server-uslp -s uslp
#tmux set-option remain-on-exit on
tmux send-keys \
	"./rpi/ccsds-link-build/debug/server-uslp/server-uslp" Space -v1 \
	Enter

tmux split-window -v
tmux send-keys \
	"python3" Space "./zmq/broker.py" Space "--no-log" \
	Enter

tmux split-window -h \
	"./zmq/imitator_radio.py" \
	"--uplink-bind=10.30.10.10:2222" \
	"--uplink-connect=10.30.10.10:2222" \
	"--block-irssi" \
	"--block-stats"

tmux split-window -h -t 0
tmux send-keys \
	"cd ./zmq" \
	Enter

tmux a -t uslp
