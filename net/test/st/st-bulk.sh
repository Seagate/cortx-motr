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


CWD=$(cd "$( dirname "$0")" && pwd)

source $CWD/st-config.sh
TEST_TYPE="bulk"
MSG_NR=1048576
MSG_SIZE=1m
CONCURRENCY_CLIENT=8
CONCURRENCY_SERVER=16
BD_BUF_NR_CLIENT=16
BD_BUF_NR_SERVER=32
BD_BUF_SIZE=4k
BD_BUF_NR_MAX=8

source $CWD/run-1x1.sh
