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


# A simple wrapper for motr commands

# Get the location of this script and look for the command in the
# same directory
dir=`dirname $0`

M0_SRC_DIR=$(dirname $(readlink -f $0))
M0_SRC_DIR="$M0_SRC_DIR/../../../"

. $M0_SRC_DIR/utils/functions # m0_default_xprt

XPRT=$(m0_default_xprt)

#. $dir/motr_config.sh

# Get local address
check_and_restart_lnet
LOCAL_NID=$(m0_local_nid_get)
LOCAL_EP=$LOCAL_NID:12345:33:100
HA_EP=$LOCAL_NID:12345:34:1
PROF_OPT='<0x7000000000000001:0>'
PROC_FID='<0x7200000000000000:0>'
INDEX_DIR="/tmp"
cmd_args="$LOCAL_EP $HA_EP $CONFD_EP '$PROF_OPT' '$PROC_FID' $INDEX_DIR"

echo -n "Enter ID of object to operate on: "
read KEY
cmd_args="$cmd_args $KEY"

if [ "$1" = "m0cat" ]; then
	echo -n "Enter the blocksize of the object: "
	read BLOCKSIZE

	echo -n "Enter the number of blocks to read: "
	read BLOCKCOUNT

	cmd_args="$cmd_args $BLOCKSIZE $BLOCKCOUNT"

	echo -n "Enter the name of the file to output to: "
	read OUTPUT

	cmd_args="$cmd_args $OUTPUT"
fi;

if [ "$1" = "m0cp" ] || [ "$1" = "m0cp_mt" ]; then
	echo -n "Enter the name of the file to copy from: "
	read SRC_FILE

	echo -n "Enter the blocksize of the object: "
	read BLOCKSIZE

	echo -n "Enter the number of blocks to copy: "
	read BLOCKCOUNT

	cmd_args="$cmd_args $SRC_FILE $BLOCKSIZE $BLOCKCOUNT"
fi;

# Assembly command
cmd_exec="$dir/$1"
cmd="$cmd_exec $cmd_args"

# Run it
echo "# $cmd" >/dev/tty

eval $cmd || {
	echo "Failed to run command $1"
}

# Local variables:
# sh-basic-offset: 8
# sh-indentation: 8
# tab-width: 8
# End:
