#!/bin/bash

set -x

rm -rf /var/perfline/hw.$(hostname -s) || true
mkdir /var/perfline/hw.$(hostname -s)
rm -rf /var/perfline/network.$(hostname -s) || true
mkdir /var/perfline/network.$(hostname -s)

cp /etc/modprobe.d/lnet.conf /var/perfline/network.$(hostname -s)/lnet.conf
cp /etc/multipath.conf /var/perfline/network.$(hostname -s)/multipath.conf

CPU_INFO=$(lscpu)
NETWORK_INTERFACES_INFO=$(ip a)
HOSTNAME=$(hostname -s)
MEMORY_SIZE=$(grep MemTotal /proc/meminfo)
LNET_INFO=$(lctl list_nids)
MULTIPATH_INFO=$(multipath -ll)
GIT_INFO=$(rpm -qa | grep 'cortx-motr\|cortx-hare\|cortx-s3')

aaa=`python3 - "$CPU_INFO" "$NETWORK_INTERFACES_INFO" "$HOSTNAME" "$MEMORY_SIZE" "$LNET_INFO" "$MULTIPATH_INFO" "$GIT_INFO"<<EOF
import yaml
import sys


HW_PATH = f'/var/perfline/hw.{sys.argv[3]}/cpu_info'
MEMORY_PATH = f'/var/perfline/hw.{sys.argv[3]}/memory_info'
GIT_INFO_PATH = f'/var/perfline/hw.{sys.argv[3]}/git_info'

LNET_PATH = f'/var/perfline/network.{sys.argv[3]}/lnet_info'
MULTIPATH_PATH = f'/var/perfline/network.{sys.argv[3]}/multipath_info'
NETWORK_INFO_PATH = f'/var/perfline/network.{sys.argv[3]}/network_interfaces_info'
result_info_dict = {}

cpu_info = sys.argv[1].split('\n')
cpu_info_dict = {info.split(':')[0] : info.split(':')[1].strip() for info in cpu_info}
del cpu_info_dict['Flags']
result_info_dict['CPU'] = cpu_info_dict

with open(HW_PATH, 'w+') as file:
    yaml.dump(cpu_info_dict, file, default_flow_style=False)

with open(NETWORK_INFO_PATH, 'w+') as file:
    file.writelines(sys.argv[2])

with open(MEMORY_PATH, 'w+') as file:
    file.writelines(sys.argv[4])

with open(LNET_PATH, 'w+') as file:
    file.writelines(sys.argv[5])

with open(MULTIPATH_PATH, 'w+') as file:
    file.writelines(sys.argv[6])

with open(GIT_INFO_PATH, 'w+') as file:
    file.writelines(sys.argv[7])

EOF`