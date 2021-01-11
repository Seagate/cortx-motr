#!/usr/bin/env python3

import subprocess
import os
import sys
import getopt
import json
import time
import netifaces
from socket import gethostname
from typing import Dict, List, NamedTuple, Set
import re
from datetime import datetime
from collections import OrderedDict
lnet_info = {}

sys.path.append(os.path.join(os.path.dirname(__file__), "..", ".."))
from cortx.utils.schema.payload import Json
from cortx.utils.conf_store import ConfStore

conf_store = ConfStore()
motr_url = 'json:///home/743120/mini_provisioner/motr_prov_conf.json'

def set_repo():
    repo_arr = [
                  {
                   "file":"/etc/yum.repos.d/cortx_commons.repo",
                   "desc":"[cortx_commons]",
                   "name":"cortx_commons",
                   "gpgcheck":"0",
                   "enabled":"1",
                   "baseurl":"http://cortx-storage.colo.seagate.com/releases/cortx/third-party-deps/centos/centos-7.8.2003/\n"
                  },
                  {
                   "file":"/etc/yum.repos.d/cortx_platform_base.repo",
                   "desc":"[cortx_platform_base]",
                   "name":"cortx_platform_base",
                   "gpgcheck":"0",
                   "enabled":"1",
                   "baseurl":"http://ssc-satellite1.colo.seagate.com/pulp/repos/EOS/Library/custom/CentOS-7/CentOS-7-OS/"
                  },
                  {
                   "file":"/etc/yum.repos.d/cortx_platform_extras.repo",
                   "desc":"[cortx_platform_extras]",
                   "name":"cortx_platform_extras",
                   "gpgcheck":"0",
                   "enabled":"1",
                   "baseurl":"http://ssc-satellite1.colo.seagate.com/pulp/repos/EOS/Library/custom/CentOS-7/CentOS-7-Extras/"
                  },
                  {
                   "file":"/etc/yum.repos.d/3rd_party_epel.repo",
                   "desc":"[epel]",
                   "name":"epel",
                   "gpgcheck":"0",
                   "enabled":"1",
                   "baseurl":"http://ssc-satellite1.colo.seagate.com/pulp/repos/EOS/Library/custom/EPEL-7/EPEL-7/"
                  }
                ]
    for repo_dict in repo_arr:
        for k, v in repo_dict.items():
            if (k == "file"):
                f1=open(v, "a+")
                continue
            if (k == "desc"):
                f1.write("{}\n".format(v))
                continue
            f1.write("{}=".format(k))
            f1.write("{}\n".format(v))
        f1.close()

def install_pkgs(pkgs):
    for pkg in pkgs:
        cmd = "yum install -y {}".format(pkg)
        print("Executing cmd = {}".format(cmd))
        execute_command(cmd, 120)

def install_lnet():
    lnet_pkgs = ["kmod-lustre-client", "lustre-client"]
    install_pkgs(lnet_pkgs)


def start_services(services):
    for service in services:
        cmd = "service {} start".format(service)
        print("Executing cmd = {}".format(cmd))
        execute_command(cmd, 180)
        time.sleep(10)
        cmd = "service {} status".format(service)
        print("Executing cmd = {}".format(cmd))
        execute_command(cmd, 180)
        time.sleep(10)

def configure_lnet_manual():
    ifaces = netifaces.interfaces()
    print("Available network ifaces={}".format(ifaces))
    for iface in ifaces:
      x = re.search("^eth[1-9]+[0-9]*", iface)
      if (x is not None):
            break
    fp = open("/etc/modprobe.d/lnet.conf", "w")
    fp.write("options lnet networks=tcp({})  config_on_load=1  lnet_peer_discovery_disabled=1\n".format(iface))
    fp.close()
    start_services(["lnet"])

    #Update conf store
    load_config('motr_prov_conf', motr_url)
    lnet_conf = conf_store.get('motr_prov_conf', 'motr>lnet', default_val=None)
    print("Atul on 108 lnet_conf={}".format(lnet_conf))
    lnet_conf["iface"] = iface
    lnet_conf["iface_type"] = "udp"
    lnet_conf["config_on_load"] = 0
    lnet_conf["lnet_peer_discovery_disabled"] = 0
    print("Atul on 113 lnet_conf={}".format(lnet_conf))
    conf_store.set('motr_prov_conf', 'motr>lnet', lnet_conf)
    conf_store.save('motr_prov_conf')
    print("Reading it back.................")
    load_config('motr_prov_conf_new', motr_url)
    motr_new = conf_store.get('motr_prov_conf', 'motr', default_val=None)
    print(motr_new)

def configure_lnet_from_conf_store():
    '''
       Get iface and /etc/modprobe.d/lnet.conf params from
       conf store. Configure lnet. Start lnet service
    '''
    load_config('motr_prov_conf', motr_url)
    lnet_conf = conf_store.get('motr_prov_conf', 'motr>lnet', default_val=None)
    fp = open("/etc/modprobe.d/lnet.conf", "w")
    fp.write("options lnet networks=tcp({})  config_on_load=1  lnet_peer_discovery_disabled=1\n".format(lnet_conf["iface"], lnet_conf["config_on_load"], lnet_conf["lnet_peer_discovery_disabled"]))
    fp.close()
    time.sleep(10)
    cmd = "service lnet start"
    ret = execute_command(cmd, 180)
    time.sleep(10)
    cmd = "service lnet status"
    ret = execute_command(cmd, 180)
    return ret

def execute_command(cmd, timeout_secs = 100):
    ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          shell=True)
    stdout, stderr = ps.communicate(timeout=timeout_secs);
    stdout = str(stdout, 'utf-8')
    if (ps.returncode != 0):
        print("Failed cmd = {}\nret = {}\nout = %s\n".format(cmd, ps.returncode, stdout))

def execute_command_1(cmd):
    ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          shell=True)
    stdout, stderr = ps.communicate();
    stdout = str(stdout, 'utf-8')
    #print("Atul stdout={}".format(stdout))
    return (ps.returncode, stdout)

def lnet_conf_check():
  cmd = "rpm -qa | grep lustre"
  (ret, stdout)= execute_command(cmd)
  print("Atul ret={}\n\nstdout={}".format(ret, type(stdout)))

def load_config(index, backend_url):
    """Instantiate and Load Config into constore"""
    conf_store.load(index, backend_url)
    return conf_store

def create_lvm(node_name, metadata_dev, is_physical):
    cmd = f"swapoff -a"
    execute_command(cmd)

    cmd = f"parted {metadata_dev} mklabel gpt"
    execute_command(cmd)

    end = 10
    if is_physical:
        end = 1000
    cmd = f"parted {metadata_dev} mkpart primary ext4 0% {end}GB"
    execute_command(cmd)

    start = 11
    if is_physical:
        start = 1001
    cmd = f"parted {metadata_dev} mkpart primary ext2 {start}GB 100%"
    execute_command(cmd)

    cmd = f"parted {metadata_dev} toggle 2 lvm"
    execute_command(cmd)

    cmd = f"pvcreate {metadata_dev}2"
    execute_command(cmd)

    cmd = f"vgcreate  vg_metadata_{node_name} {metadata_dev}2"
    execute_command(cmd)

    cmd = f"vgchange --addtag {node_name} vg_metadata_{node_name}"
    execute_command(cmd)

    cmd = f"vgscan --cache"
    execute_command(cmd)

    cmd = f"lvcreate -n lv_main_swap vg_metadata_{node_name} -l 51%VG"
    execute_command(cmd)

    cmd = f"lvcreate -n lv_raw_metadata vg_metadata_{node_name} -l 100%FREE"
    execute_command(cmd)

    cmd = f"mkswap -f /dev/vg_metadata_{node_name}/lv_main_swap"
    execute_command(cmd)

    cmd = f"test -e /dev/vg_metadata_{node_name}/lv_main_swap"
    execute_command(cmd)

    cmd = f"swapon /dev/vg_metadata_{node_name}/lv_main_swap"
    execute_command(cmd)

    cmd = (
            f"echo \"/dev/vg_metadata_{node_name}/lv_main_swap    swap    "
            f"swap    defaults        0 0\" >> /etc/fstab"
        )
    execute_command(cmd)

    cmd = f"mkfs.ext4 {metadata_dev}1 -L cortx_metadata"
    execute_command(cmd)

    #cmd = f"blockdev --flushbufs /dev/disk/by-id/dm-name-mpath* || true"
    #execute_command(cmd)

    time.sleep(10)

    cmd = f"timeout -k 10 30 partprobe || true"
    execute_command(cmd)

def config_lvm():
    #TODO: get below config from KV store
    node_name = "srvnode-2"
    metadata_device = "/dev/sde"
    is_physical = False
    create_lvm(node_name, metadata_device, is_physical)

if __name__ == "__main__":
    #load_config('motr_prov_conf', 'json:///home/743120/mini_provisioner/motr_prov_conf.json')
    #result_data = conf_store.get_data('motr_prov_conf')
    #conf_store.set('motr_prov_conf', 'motr>iface', "eth2")
    #result_data=conf_store.get('motr_prov_conf', 'motr>iface', default_val=None)
    #print(result_data)
    #if ("eth2" in result_data):
    #    print("Found")
    #else:
    #    print("not found")
    #lnet_conf_check()
    #set_repo()
    #install_lnet()
    #configure_lnet_from_conf_store()
    #install_motr()
    configure_lnet_manual()
