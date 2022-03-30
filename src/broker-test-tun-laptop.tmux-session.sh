#!/usr/bin/env bash

if [[ -z "$1" ]] ; then
	echo "Нужно указать каталог сборки для запуска оттуда всех бинарников"
	exit 1
fi

export ITS_GBUS_BPCS_ENDPOINT=tcp://localhost:7778
export ITS_GBUS_BSCP_ENDPOINT=tcp://localhost:7777
export ITS_LOG_LEVEL info

THIS_DIR=`dirname "$0"`
BUILD_DIR="$1"

tmux new-session -d -n server-uslp -s uslp
#tmux set-option remain-on-exit on
tmux send-keys \
	"${BUILD_DIR}/server-uslp/server-uslp" Space \
	Enter

tmux split-window -v
tmux send-keys \
	"python3" Space "./zmq/broker.py" Space "--no-log" \
	Enter

tmux split-window -h
tmux send-keys \
	"${THIS_DIR}/zmq/imitator_radio.py" Space \
	"--uplink-bind=10.30.10.10:2222" Space \
	"--uplink-connect=10.30.10.1:2222" Space \
	"--block-irssi" Space \
	"--block-stats" Space \
	Enter

tmux split-window -h -t 0
tmux send-keys \
	sudo Space -E Space "${BUILD_DIR%/}/server-tun/server-tun" Space \
	"--tun tun200 --addr 10.0.0.10/24 --mtu 200" Space \
	Enter

tmux a -t uslp
