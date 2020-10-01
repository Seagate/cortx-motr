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

set -eu

# Note: if you want to use systemtap with Motr kernel module (m0tr.ko)
# which is not installed in the system, you need to symlink m0tr.ko and
# run depmod. Example:
# # sudo ln -s /work/motr/motr/m0tr.ko \
#              /lib/modules/`uname -r`/kernel/drivers/m0tr.ko
# # sudo depmod
# It's useful when you want to debug a module which is in the source tree.

M0_SRC_DIR="$(realpath $0)"
M0_SRC_DIR="${M0_SRC_DIR%/*/*/*}"
. $M0_SRC_DIR/utils/functions  # die

if [ ${0##*/} = $(basename $(realpath $0)) ]; then
	die "${0##*/}: Don't execute this script, use a dedicated symlink."
fi

STP_SCRIPT=$0.stp
[ -r "$STP_SCRIPT" ] || die "$STP_SCRIPT: No such file"

LIBMOTR="$M0_SRC_DIR/motr/.libs/libmotr.so"
M0D="$M0_SRC_DIR/motr/.libs/lt-m0d"
UT="$M0_SRC_DIR/ut/.libs/lt-m0ut"
M0TAPSET="$M0_SRC_DIR/scripts/systemtap/tapset"

set -x
stap -vv -d "$LIBMOTR" -d "$M0D" -d "$UT" --ldd                         \
	-DMAXTRACE=10 -DSTP_NO_OVERLOAD                                 \
	-DMAXSKIPPED=1000000 -DMAXERRORS=1000 -DMAXSTRINGLEN=2048       \
	-t -DTRYLOCKDELAY=5000                                          \
	-I "$M0TAPSET" "$STP_SCRIPT" "$LIBMOTR" "$M0D"
