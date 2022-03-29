#!/usr/bin/env bash


if [[ -z "$1" ]] ; then
	echo "Нужно указать каталог сборки для запуска оттуда всех бинарников"
	exit 1
fi

export ITS_GBUS_BPCS_ENDPOINT=tcp://localhost:7778
export ITS_GBUS_BSCP_ENDPOINT=tcp://localhost:7777

THIS_DIR=`dirname "$0"`
BUILD_DIR="$1"

tmux new-session -d -n server-uslp -s uslp
#tmux set-option remain-on-exit on
tmux send-keys \
	"${BUILD_DIR}/server-uslp/server-uslp" Space -v1 \
	Enter

tmux split-window -v
tmux send-keys \
	"python3" Space "./zmq/broker.py" Space "--no-log" \
	Enter

tmux split-window -h \
	"${THIS_DIR}/zmq/imitator_radio.py" \
	"--uplink-bind=10.30.10.10:2222" \
	"--uplink-connect=10.30.10.10:2222" \
	"--block-irssi" \
	"--block-stats"

tmux split-window -h -t 0
tmux send-keys \
	"cd ${THIS_DIR}/zmq" \
	Enter

tmux a -t uslp
