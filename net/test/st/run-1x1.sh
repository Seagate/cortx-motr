#!/usr/bin/env bash
#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#

set -eu

# set -x
# export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '

# Get the default net transport
XPRT=$(m0_default_xprt)

# The same values as in client-server UT
if [ "$XPRT" = "lnet" ]; then
	ADDR_PREFIX="$NID:12345:42:"
	BASE_PORTNUM=298
else
	ADDR_PREFIX="$NID@"
	BASE_PORTNUM=3000
fi

ADDR_CONSOLE4CLIENTS="$ADDR_PREFIX$BASE_PORTNUM"
ADDR_CONSOLE4SERVERS="$ADDR_PREFIX$((BASE_PORTNUM+1))"
ADDR_CMD_CLIENT="$ADDR_PREFIX$((BASE_PORTNUM+2))"
ADDR_DATA_CLIENT="$ADDR_PREFIX$((BASE_PORTNUM+3))"
ADDR_CMD_SERVER="$ADDR_PREFIX$((BASE_PORTNUM+4))"
ADDR_DATA_SERVER="$ADDR_PREFIX$((BASE_PORTNUM+5))"
DIR_COUNTER=0
DIRS_TO_DELETE=""

if [ "$(id -u)" -ne 0 ]; then
    echo "Must be run as root"
    exit 1
fi

node_start_addr()
{
	local role=$1
	local addr="$2"
	local addr_console="$3"
	local pid_role="$4"
	local dir

	if [ "$role" == "$KERNEL_ROLE" ]; then
		insmod "$MOD_M0NETTESTD" addr=$addr addr_console=$addr_console
	else
		dir="net-test$DIR_COUNTER"
		DIRS_TO_DELETE="$DIRS_TO_DELETE $dir"
		mkdir -p $dir
		pushd $dir > /dev/null
		DIR_COUNTER=$(expr $DIR_COUNTER + 1)
		"$CMD_M0NETTESTD" -a "$addr" -c "$addr_console" &
		popd > /dev/null
		eval PID_$4=$!
	fi
}

node_start()
{
	local role=$1
	local addr
	local addr_console

	if [ "$role" == "client" ]; then
		node_start_addr $role "$ADDR_CMD_CLIENT" \
			"$ADDR_CONSOLE4CLIENTS" CLIENT
	elif [ "$role" == "server" ]; then
		node_start_addr $role "$ADDR_CMD_SERVER" \
			"$ADDR_CONSOLE4SERVERS" SERVER
	fi
}

node_stop()
{
	local role=$1

	if [ "$role" == "$KERNEL_ROLE" ]; then
		rmmod m0nettestd
	fi
}

eval_kill_pid() {
	eval pid=\${$1-xxx}
	if [ "$pid" != "xxx" -a -f /proc/$pid/exe ]; then
		KILL_PID+="$pid "
	fi
}

unload_all() {
	KILL_PID=
	node_stop "client"
	node_stop "server"
	eval_kill_pid "PID_SERVER"
	eval_kill_pid "PID_CLIENT"
	eval_kill_pid "PID_CONSOLE"
	for pid in $KILL_PID; do
		kill $pid > /dev/null 2>&1 || true
		wait $pid > /dev/null 2>&1 || true
	done
	sleep $NET_CLEANUP_TIMEOUT
	rm -rf $DIRS_TO_DELETE
}
trap unload_all EXIT

# allow only 'fatal' and higher trace messages to be printed on console by
# default, to prevent cluttering of UT output with "fake" error messages,
# generated while testing various error paths using fault injection;
# this can be overridden with '-e' CLI option.
export M0_TRACE_LEVEL='error+'
# export M0_TRACE_LEVEL=debug+
export M0_TRACE_IMMEDIATE_MASK=all

node_start "client"
node_start "server"
sleep $NODE_INIT_DELAY

BULK_PARAMETERS=

if [ "$TEST_TYPE" == "bulk" ]; then
	BULK_PARAMETERS="-B $BD_BUF_NR_SERVER \
			 -b $BD_BUF_NR_CLIENT \
			 -f $BD_BUF_SIZE \
			 -g $BD_BUF_NR_MAX"
fi

"$CMD_M0NETTEST" -A "$ADDR_CONSOLE4SERVERS" \
		 -a "$ADDR_CONSOLE4CLIENTS" \
		 -C "$ADDR_CMD_SERVER" \
		 -c "$ADDR_CMD_CLIENT" \
		 -D "$ADDR_DATA_SERVER" \
		 -d "$ADDR_DATA_CLIENT" \
		 -t "$TEST_TYPE" \
		 -n "$MSG_NR" \
		 -T "$TEST_RUN_TIME" \
		 -s "$MSG_SIZE" \
		 -E "$CONCURRENCY_SERVER" \
		 -e "$CONCURRENCY_CLIENT" \
		 $VERBOSE \
		 $PARSABLE \
		 $BULK_PARAMETERS &
PID_CONSOLE=$!
wait $PID_CONSOLE

# The same time for fini
sleep $NODE_INIT_DELAY
