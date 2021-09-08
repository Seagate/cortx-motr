#!/bin/bash

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "Are you using CentOS 7.8 or newer? If you are sure you are enter 'yes'. If you are not sure enter 'no' and check using 'cat /etc/redhat-release' if you are using CentOS 7.8 or newer you can rerun this script and enter 'yes' and it will work."
        read correct_version
else
        echo "Please use CentOS 7.8 or newer and then this installation will work"
        exit 1
fi

if [[ $correct_version == "no" ]] ; then
        echo "Please use CentOS 7.8 or newer and then this installation will work"
        echo "here is the link to the download site: https://www.centos.org/download/"
        exit 1
fi

yum install git -y
yum install epel-release -y
yum install ansible -y

git clone --recursive https://github.com/Seagate/cortx-motr.git

cd cortx-motr
scripts/install-build-deps
ip a
echo "Please enter your network interface name here (using the output of 'ip a' shown above'):"
read iface_name

sed -i "s/options lnet networks=tcp(eth0) config_on_load=1/options lnet networks=tcp("${iface_name}") config_on_load=1/" /etc/modprobe.d/lnet.conf

modprobe lnet
lctl list_nids

scripts/m0 make

