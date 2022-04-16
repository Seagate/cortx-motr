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


ERR_SUCCESS=0
ERR_NOT_IMPLEMENTED=1
ERR_SUDO_REQUIRED=2
ERR_INVALID_ARGS=3

ERR_LNET=10
ERR_LNET_CONF_FILE_NOT_FOUND=$((ERR_LNET+1))
ERR_LNET_CONF_FILE_IS_EMPTY=$((ERR_LNET+2))
ERR_LNET_COMP_NOT_INSTALLED=$((ERR_LNET+3))
ERR_LNET_BAD_CONF_DATA=$((ERR_LNET+4))
ERR_LENT_DEV_FILE_NOT_FOUND=$((ERR_LNET+5))
ERR_LNET_INVALID_IP=$((ERR_LNET+6))
ERR_LNET_IP_ADDR_PING_FAILED=$((ERR_LNET+7))
ERR_LNET_NID_FOR_IP_ADDR_NOT_FOUND=$((ERR_LNET+8))

ERR_SRVC=50
ERR_SRVC_ETC_SYSCONFIG_MOTR_NOT_FOUND=$((ERR_SRVC+1))
ERR_SRVC_UUID_FILE_CREATE_FAILED=$((ERR_SRVC+2))
ERR_SRVC_UUID_FILE_DELETE_FAILED=$((ERR_SRVC+3))
ERR_SRVC_MOTR_STATUS_FAILED=$((ERR_SRVC+4))

ERR_CFG=60
ERR_CFG_FILE_NOTFOUND=$((ERR_CFG+1))
ERR_CFG_KEY_NOTFOUND=$((ERR_CFG+2))
ERR_CFG_KEY_OPNOTALLOWED=$((ERR_CFG+3))
ERR_CFG_SALT_FAILED=$((ERR_CFG+4))
