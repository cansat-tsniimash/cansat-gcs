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
	"${BUILD_DIR%/}/server-uslp/server-uslp" Space \
	"2>&1 | tee uslp.log" Space \
	Enter

tmux split-window -v
tmux send-keys \
	"${THIS_DIR%/}/zmq/venv/bin/python" Space \
	"./zmq/broker.py" Space \
	"2>&1 | tee broker.log" Space \
	Enter

	#"--no-log" \
	# Enter

tmux split-window -h
tmux send-keys \
	"${THIS_DIR%/}/zmq/venv/bin/python" Space \
	"${THIS_DIR%/}/zmq/imitator_radio.py" Space \
	"--uplink-bind=0.0.0.0:2222" Space \
	"--uplink-connect=localhost:2222" Space \
	"--block-irssi" Space \
	"--block-stats" Space \
	"--rx-time 0.25" Space \
	"--tx-time 0.02" Space \
	"2>&1 | tee radio.log" Space \
	Enter

tmux split-window -h -t 0
tmux send-keys \
	"${THIS_DIR%/}/zmq/venv/bin/python" Space \
	"${THIS_DIR%/}/zmq/its_logfile_mav_to_uplink_sdu.py" Space \
	"--mapid 0" Space \
	"-i ${THIS_DIR%/}/zmq/its-broker-log-combined.zmq-log" Space \
	"--drop-untill 1633357251" Space \
	"2>&1 | tee sender.log" Space \
	# Enter

tmux a -t uslp
