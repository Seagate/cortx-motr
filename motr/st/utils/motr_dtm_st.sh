#!/bin/bash
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


# System test framework.

#set -x

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.motr-dtm-st}

emsg="---"
M0_SRC_DIR=`readlink -f $0`
M0_SRC_DIR=${M0_SRC_DIR%/*/*/*/*}

. $M0_SRC_DIR/utils/functions # sandbox_init, report_and_exit
# following scripts are mandatory for start the motr service (motr_start/stop)
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_common_inc.sh
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/m0t1fs_server_inc.sh
. $M0_SRC_DIR/motr/st/utils/motr_local_conf.sh

verbose=0

#NODE_UUID=`uuidgen`

interrupt() { echo "Interrupted by user" >&2; stop 2; }
error() { echo "$@" >&2; stop 1; }


function usage()
{
	echo "$0"
	echo "Options:"
	echo "    '-v|--verbose'         output additional info into console"
	echo "    '-h|--help'            show this help"
}

start_motr()
{
	local rc=0
	local multiple_pools=0
	local stride=1024
	local N=$NR_DATA
	local K=$NR_PARITY
	local S=$NR_SPARE
	local P=$POOL_WIDTH
	local FI_OPTS="m0_ha_msg_accept:in-dtm-st:always"

	echo "Motr service starting..."
	if [ $verbose == 1 ]; then
		motr_service start $multiple_pools $stride $N $K $S $P $FI_OPTS
	else
		motr_service start $multiple_pools $stride $N $K $S $P $FI_OPTS >/dev/null 2>&1
	fi
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Failed to start Motr Service."
	fi
	return $rc
}

stop_motr()
{
	local rc=0
	echo "Motr service stopping..."
	if [ $verbose == 1 ]; then
		motr_service stop
	else
		motr_service stop >/dev/null 2>&1
	fi
	rc=$?
	if [ $rc -ne "0" ]
	then
		echo "Failed to stop Motr Service."
	fi
	return $rc
}

motr_dtm_st_pre()
{
	sandbox_init
	start_motr || error "Failed to start Motr service"
}

motr_dtm_st_post()
{
	local rc=${1:-$?}

	trap - SIGINT SIGTERM
	stop_motr || rc=$?

#	if [ $rc -eq 0 ]; then
	#	sandbox_fini
#	fi
	report_and_exit $0 $rc
}

main()
{
	motr_dtm_st_pre

	motr_dtm_st_post
}

arg_list=("$@")

[ `id -u` -eq 0 ] || die 'Must be run by superuser'

OPTS=`getopt -o vhni --long verbose,help -n 'parse-options' -- "$@"`
if [ $? != 0 ] ; then echo "Failed parsing options." >&2 ; exit 1 ; fi
eval set -- "$OPTS"
while true; do
	case "$1" in
	-v | --verbose ) verbose=1; shift ;;
	-h | --help )    usage ; exit 0; shift ;;
	-- ) shift; break ;;
	* ) break ;;
	esac
done

for arg in "${arg_list[@]}"; do
	if [ "${arg:0:2}" != "--" -a "${arg:0:1}" != "-" ]; then
		tests_list+=(${arg})
	fi
done

if [ ${#tests_list[@]} -eq 0 ]; then
	declare -a tests_list=(motr-start-stop
			      )
fi

trap interrupt SIGINT SIGTERM
main

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
