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


#set -x

motr_st_util_dir=$( cd "$(dirname "$0")" ; pwd -P )

. $motr_st_util_dir/motr_sns_common.sh

N=3
K=2
S=2
P=14
stride=4

main()
{
	motr_sns_repreb $N $K $S $P $stride

	return $rc
}

echo "SNS Repair/Rebalance Test ... "
trap unprepare EXIT
main
report_and_exit sns-repair-rebalance $?
