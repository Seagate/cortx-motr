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


#This script helps to create a device configuration file viz. devices.conf.
#The file uses yaml format, as desired by the m0d program.
#The script uses the fdisk command and extracts only the unused devices
#present on the system (i.e without valid partition table).
#Below illustration describes a typical devices.conf entry,
#Device:
#       - id: 1
#	  filename: /dev/sda

i=0
echo "Device:" > ./devices.conf
sudo fdisk -l 2>&1 |  grep "doesn't contain a valid partition table"  | awk '{ print $2 }'| grep "/dev" | grep [a-z]$ | while read line
do
echo "       - id: $i" >> ./devices.conf
echo "         filename: $line" >> ./devices.conf
i=$(($i + 1))
done
