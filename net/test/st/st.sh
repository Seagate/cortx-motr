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

# Next lines are useful for ST scripts debugging
# set -eux
# export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '

SANDBOX_DIR=${SANDBOX_DIR:-/var/motr/sandbox.net-st}

CWD=$(dirname $(readlink -f $0))
M0_SRC_DIR=${CWD%/*/*/*}

. $M0_SRC_DIR/utils/functions # die, opcode, sandbox_init, report_and_exit
. $M0_SRC_DIR/m0t1fs/linux_kernel/st/common.sh
. $CWD/st-config.sh

# Get the default net transport
XPRT=$(m0_default_xprt)

role_space()
{
	local role=$1
	[ "$role" == "$KERNEL_ROLE" ] && echo -n "kernel space"
	[ "$role" != "$KERNEL_ROLE" ] && echo -n "user space"
}

unload_all() {
	if [ "$XPRT" = "lnet" ]; then
		modunload
	fi
}
trap unload_all EXIT

if [ "$XPRT" = "lnet" ]; then
	modprobe_lnet
	lctl network up > /dev/null
	modload || exit $?
fi


sandbox_init
export TEST_RUN_TIME=5
echo "transfer machines endpoint prefix is $NID"


if [ "$XPRT" = "lnet" ]; then
	ROLE=(none client server)
else
	ROLE=(none)
fi

for KERNEL_ROLE in ${#ROLE[@]}; do
	export KERNEL_ROLE
	echo -n "------ test client in $(role_space client), "
	echo "test server in $(role_space server)"
	echo "--- ping test (test message size is 4KiB)"
	$CWD/st-ping.sh
	echo "--- bulk test (test message size is 1MiB)"
	$CWD/st-bulk.sh
done
sandbox_fini
report_and_exit net 0
