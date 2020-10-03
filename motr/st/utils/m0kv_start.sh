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

# Script to start motr CmdLine tool (m0kv).

usage()
{
	cat <<.
Usage:

$ sudo m0kv_start.sh [local|remote] [-v] ["(m0kv index commands)"]
.
}

conf=$1
shift 1
verbose=0
if [ $1 == "-v" ] ; then
	verbose=1
	shift
fi
all=$*

function m0kv_cmd_start()
{
	# Assembly command
	local exec="`dirname $0`/../../m0kv/m0kv"
	if [ ! -f $exec ];then
		echo "Can't find m0kv"
		return 1
	fi

	local args="-l $MOTR_LOCAL_EP -h $MOTR_HA_EP \
		    -p '$MOTR_PROF_OPT' -f '$MOTR_PROC_FID'"
	local cmdline="$exec $args $all"
	if [ $verbose == 1 ]; then
		echo "Running m0kv command line tool..."
		echo "$cmdline"
	fi
	eval $cmdline || {
		err=$?
		echo "Motr CmdLine utility returned $err!"
		return $err
	}

	return $?
}

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

m0kv_cmd_start
exit $?

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
