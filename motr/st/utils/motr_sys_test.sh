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


# Script for starting or stopping Motr system tests

. `dirname $0`/motr_st_inc.sh
# enable core dumps
ulimit -c unlimited

# Debugger to use
debugger=

# Print out usage
usage()
{
	cat <<.
Usage:

$ sudo motr_sys_test [start|stop|list|run] [local|remote] [-d debugger] \
[-i Index-service] [-t tests] [-k] [-u]

Where:

start: starts only motr system tests.
stop : stops motr system tests.
list : Lists all the available motr system tests.
run  : Starts Motr services, executes motr system tests and then\
stops motr services.

-d: Invoke a debugger if set, only gdb is supported currently

-i: Select Index service:
    CASS : Cassandra
    KVS  : Motr KVS

-k: run Motr system tests in kernel mode

-u: run Motr system tests in user space mode

-t TESTS: Only run selected tests

-r: run tests in a suite in random order
.
}
OPTIONS_STRING="d:i:kurt:"

# Get options
cmd=$1
conf=$2
shift 2

umod=1
random_mode=0
while getopts "$OPTIONS_STRING" OPTION; do
	case "$OPTION" in
		d)
			debugger="$OPTARG"
			;;
		i)
			index="$OPTARG"
			;;
		k)
			umod=0
			echo "Running Motr ST in Kernel mode"
			;;
		u)
			umod=1
			echo "Running Motr ST in User mode"
			;;
		t)
			tests="$OPTARG"
			;;
		r)
			random_mode=1
			;;
		*)
			usage
			exit 1
			;;
	esac
done

case "$conf" in
	local)
		. `dirname $0`/motr_local_conf.sh
		;;
	remote)
		. `dirname $0`/motr_remote_conf.sh
		;;
	*)
		usage
		exit
esac

case "$cmd" in
	start)
		sandbox_init

		if [ $umod -eq 1 ]; then
			motr_st_start_u $index $debugger
		else
			motr_st_start_k $index
		fi

		rc=$?
		sandbox_fini $rc
		report_and_exit motr_sys_test $rc
		;;
	run)
		sandbox_init
		( exec `dirname $0`/motr_services.sh start )

		if [ $umod -eq 1 ]; then
			motr_st_start_u $index $debugger
		else
			motr_st_start_k $index
		fi

		rc=$?
		( exec `dirname $0`/motr_services.sh stop )
		sandbox_fini $rc
		report_and_exit motr_sys_test $rc
		;;
	stop)
		if [ $umod -eq 1 ]; then
			motr_st_stop_u
		else
			motr_st_stop_k
		fi
		;;
	list)
		sandbox_init
		motr_st_list_tests
		sandbox_fini 0
		;;
	*)
		usage
		exit
esac

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
