#!/usr/bin/env python3
#
# Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
import sys
import errno
import os
import re
import subprocess
import logging
import glob
import time
import yaml
import psutil
from typing import List, Dict, Any
from cortx.utils.conf_store import Conf
from cortx.utils.cortx import Const

MOTR_SERVER_SCRIPT_PATH = "/usr/libexec/cortx-motr/motr-start"
MOTR_MKFS_SCRIPT_PATH = "/usr/libexec/cortx-motr/motr-mkfs"
MOTR_FSM_SCRIPT_PATH = "/usr/libexec/cortx-motr/motr-free-space-monitor"
MOTR_CONFIG_SCRIPT = "/opt/seagate/cortx/motr/libexec/motr_cfg.sh"
MOTR_MINI_PROV_LOGROTATE_SCRIPT = "/opt/seagate/cortx/motr/libexec/motr_mini_prov_logrotate.sh"
CROND_DIR = "/etc/cron.hourly"
LNET_CONF_FILE = "/etc/modprobe.d/lnet.conf"
LIBFAB_CONF_FILE = "/etc/libfab.conf"
SYS_CLASS_NET_DIR = "/sys/class/net/"
MOTR_SYS_CFG = "/etc/sysconfig/motr"
MOTR_WORKLOAD_DIR = "/opt/seagate/cortx/motr/workload"
FSTAB = "/etc/fstab"
LOGFILE = "/var/log/seagate/motr/mini_provisioner"
LOGDIR = "/var/log/seagate/motr"
LOGGER = "mini_provisioner"
IVT_DIR = "/var/log/seagate/motr/ivt"
MOTR_LOG_DIR = "/var/motr"
TIMEOUT_SECS = 120
MACHINE_ID_LEN = 32
MOTR_LOG_DIRS = [LOGDIR, MOTR_LOG_DIR]
BE_LOG_SZ = 4*1024*1024*1024 #4G
BE_SEG0_SZ = 128 * 1024 *1024 #128M
ALLIGN_SIZE = 4096
MACHINE_ID_FILE = "/etc/machine-id"
TEMP_FID_FILE = "/opt/seagate/cortx/motr/conf/service_fid.yaml"
CMD_RETRY_COUNT = 5
MEM_THRESHOLD = 4*1024*1024*1024
CVG_COUNT_KEY = "num_cvg"

class MotrError(Exception):
    """ Generic Exception with error code and output """

    def __init__(self, rc, message, *args):
        self._rc = rc
        self._desc = message % (args)

    def __str__(self):
        return f"error[{self._rc}]: {self._desc}"

def execute_command_without_log(cmd,  timeout_secs = TIMEOUT_SECS,
    verbose = False, retries = 1, stdin = None, logging=False):
    ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         shell=True)
    if stdin:
        ps.stdin.write(stdin.encode())
    stdout, stderr = ps.communicate(timeout=timeout_secs);
    stdout = str(stdout, 'utf-8')

    time.sleep(1)
    if ps.returncode != 0:
        raise MotrError(ps.returncode, f"\"{cmd}\" command execution failed")

#TODO: logger config(config_log) takes only self as argument so not configurable,
#      need to make logger configurable to change formater, etc and remove below
#      duplicate code,
def execute_command_console(self, command):
    logger = logging.getLogger("console")
    if not os.path.exists(LOGDIR):
        try:
            os.makedirs(LOGDIR, exist_ok=True)
            with open(f'{self.logfile}', 'w'): pass
        except:
            raise MotrError(errno.EINVAL, f"{self.logfile} creation failed\n")
    else:
        if not os.path.exists(self.logfile):
            try:
                with open(f'{self.logfile}', 'w'): pass
            except:
                raise MotrError(errno.EINVAL, f"{self.logfile} creation failed\n")
    logger.setLevel(logging.DEBUG)
    # create file handler which logs debug message in log file
    fh = logging.FileHandler(self.logfile)
    fh.setLevel(logging.DEBUG)
    # create console handler to log messages ERROR and above
    ch = logging.StreamHandler()
    ch.setLevel(logging.INFO)
    formatter = logging.Formatter('%(asctime)s - %(message)s')
    fh.setFormatter(formatter)
    ch.setFormatter(formatter)
    logger.addHandler(fh)
    logger.addHandler(ch)
    logger.info(f"executing command {command}")
    try:
        process = subprocess.Popen(command, stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   shell=True)
    except Exception as e:
        logger.error("ERROR {} when running {} with exception {}".format(sys.exc_info()[1],
                      command, e.message))
        return None
    while True:
        stdout = process.stdout.readline()
        if process.poll() is not None:
            break
        if stdout:
            logger.info(stdout.strip().decode())
    rc = process.poll()
    return rc


def execute_command(self, cmd, timeout_secs = TIMEOUT_SECS, verbose = False,
                    retries = 1, stdin = None, logging=True):
    # logging flag is set False when we execute any command
    # before logging is configured.
    # If logging is False, we use print instead of logger
    # verbose(True) and logging(False) can not be set simultaneously.

    for i in range(retries):
        if logging == True:
            self.logger.info(f"Retry: {i}. Executing cmd: '{cmd}'")

        ps = subprocess.Popen(cmd, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              shell=True)
        if stdin:
            ps.stdin.write(stdin.encode())
        stdout, stderr = ps.communicate(timeout=timeout_secs);
        stdout = str(stdout, 'utf-8')

        if logging == True:
            self.logger.info(f"ret={ps.returncode}\n")

        if (self._debug or verbose) and (logging == True):
            self.logger.debug(f"[CMD] {cmd}\n")
            self.logger.debug(f"[OUT]\n{stdout}\n")
            self.logger.debug(f"[RET] {ps.returncode}\n")
        if ps.returncode == 0:
            break
        time.sleep(1)
    if ps.returncode != 0:
        raise MotrError(ps.returncode, f"\"{cmd}\" command execution failed")
    return stdout, ps.returncode

# For normal command, we execute command for CMD_RETRY_COUNT(5 default) times and for each retry timeout is of TIMEOUT_SECS(120s default).
# For daemon(e.g. m0d services), retry_count is 1 and tmeout is 0 so that we just execute this daemon command only once without timeout.
def execute_command_verbose(self, cmd, timeout_secs = TIMEOUT_SECS, verbose = False, set_timeout=True, retry_count = CMD_RETRY_COUNT):
    self.logger.info(f"Executing cmd : '{cmd}' \n")
    # For commands without timeout
    if set_timeout == False:
        timeout_secs = None
        retry_count = 1
    cmd_retry_delay = 1
    for cmd_retry_count in range(retry_count):
        ps = subprocess.run(cmd, stdin=subprocess.PIPE, check=False,
                            stdout=subprocess.PIPE, timeout=timeout_secs,
                            stderr=subprocess.PIPE, shell=True)
        self.logger.info(f"ret={ps.returncode}")
        self.logger.debug(f"Executing {cmd_retry_count} time")
        stdout = ps.stdout.decode('utf-8')
        self.logger.debug(f"[OUT]{stdout}")
        self.logger.debug(f"[ERR]{ps.stderr.decode('utf-8')}")
        self.logger.debug(f"[RET] {ps.returncode}")
        if ps.returncode != 0:
            time.sleep(cmd_retry_delay)
            continue
        return stdout, ps.returncode
    return

def execute_command_without_exception(self, cmd, timeout_secs = TIMEOUT_SECS, retries = 1):
    for i in range(retries):
        self.logger.info(f"Retry: {i}. Executing cmd : '{cmd}'\n")
        ps = subprocess.run(list(cmd.split(' ')), check=False, timeout=timeout_secs)
        self.logger.info(f"ret={ps.returncode}\n")
        if ps.returncode == 0:
            break
        time.sleep(1)
    return ps.returncode

def check_type(var, vtype, msg):
    if not isinstance(var, vtype):
        raise MotrError(errno.EINVAL, f"Invalid {msg} type. Expected: {vtype}")
    if not bool(var):
        raise MotrError(errno.EINVAL, f"Empty {msg}.")

def configure_machine_id(self, phase):
    if Conf.machine_id:
        self.machine_id = Conf.machine_id
        if not os.path.exists(f"{MACHINE_ID_FILE}"):
            if phase == "start":
                with open(f"{MACHINE_ID_FILE}", "w") as fp:
                    fp.write(f"{self.machine_id}\n")
        else:
            op = execute_command(self, f"cat {MACHINE_ID_FILE}", logging=False)[0].strip("\n")
            if op != self.machine_id:
                raise MotrError(errno.EINVAL, "machine id does not match")
    else:
        raise MotrError(errno.ENOENT, "machine id not available in conf")

def get_server_node(self):
    """Get current node name using machine-id."""
    try:
        machine_id = self.machine_id;
        server_node = Conf.get(self._index, 'node')[machine_id]
    except:
        raise MotrError(errno.EINVAL, f"MACHINE_ID {machine_id} does not exist in ConfStore")

    check_type(server_node, dict, "server_node")
    return server_node

def calc_size(self, sz):
    ret = -1
    suffixes = ['K', 'Ki', 'Kib', 'M', 'Mi', 'Mib', 'G', 'Gi', 'Gib']
    sz_map = {
              "K": 1024, "M": 1024*1024, "G": 1024*1024*1024,
              "Ki": 1024, "Mi": 1024*1024, "Gi": 1024*1024*1024,
              "Kib": 1024, "Mib": 1024*1024, "Gib": 1024*1024*1024 }

    # Check if sz ends with proper suffixes. It matches only one suffix.
    temp = list(filter(sz.endswith, suffixes))
    if len(temp) > 0:
        suffix = temp[0]
        num_sz = re.sub(r'[^0-9]', '', sz) # Ex: If sz is 128MiB then num_sz=128
        map_val = sz_map[suffix] # Ex: If sz is 128MiB then map_val = 1024*1024*1024
        ret = int(num_sz) * int(map_val)
        return ret
    else:
        self.logger.error(f"Invalid format of mem limit: {sz}\n")
        self.logger.error("Please use valid format Ex: 1024, 1Ki, 1Mi, 1Gi etc..\n")
        return ret

def set_setup_size(self, service):
    ret = False
    sevices_limits = Conf.get(self._index, 'cortx>motr>limits')['services']

    # Default self.setup_size  is "small"
    self.setup_size = "small"

    # For services other then ioservice and confd, return True
    # It will set default setup size i.e. small
    if service not in ["ioservice", "ios", "io", "all", "confd"]:
        self.setup_size = "small"
        self.logger.info(f"service is {service}. So seting setup size to {self.setup_size}\n")
        return True

    #Provisioner passes io as parameter to motr_setup.
    #Ex: /opt/seagate/cortx/motr/bin/motr_setup config --config yaml:///etc/cortx/cluster.conf --services io
    #But in /etc/cortx/cluster.conf io is represented by ios. So first get the service names right
    if service in ["io", "ioservice"]:
         svc = "ios"
    else:
         svc = service
    for arr_elem in sevices_limits:
        # For ios, confd we check for setup size according to mem size
        if arr_elem['name'] == svc:
            min_mem = arr_elem['memory']['min']

            if min_mem.isnumeric():
                sz = int(min_mem)
            else:
                sz = calc_size(self, min_mem)

            self.logger.info(f"mem limit in config is {min_mem} i.e. {sz}\n")

            # Invalid min mem format
            if sz < 0:
                ret = False
                break
            # If mem limit in ios > 4G then it is large setup size
            elif sz > MEM_THRESHOLD:
                self.setup_size = "large"
                self.logger.info(f"setup_size set to {self.setup_size}\n")
                ret = True
                break
            else:
                self.setup_size = "small"
                self.logger.info(f"setup_size set to {self.setup_size}\n")
                ret = True
                break
    if ret == False:
        raise MotrError(errno.EINVAL, f"Setup size is not set properly for service {service}."
                                      f"Please update valid mem limits for {service}")
    else:
        self.logger.info(f"service={service} and setup_size={self.setup_size}\n")
    return ret

def get_value(self, key, key_type):
    """Get data."""
    try:
        val = Conf.get(self._index, key)
    except:
        raise MotrError(errno.EINVAL, "{key} does not exist in ConfStore")

    check_type(val, key_type, key)
    return val

def get_logical_node_class(self):
    """Get logical_node_class."""
    try:
        logical_node_class = self.cluster['logical_node_class']
    except:
        raise MotrError(errno.EINVAL, f"{logical_node_class} does not exist in ConfStore")
    check_type(logical_node_class, list, "logical_node_class")
    return logical_node_class

def restart_services(self, services):
    for service in services:
        self.logger.info(f"Restarting {service} service\n")
        cmd = f"systemctl stop {service}"
        execute_command(self, cmd)
        cmd = f"systemctl start {service}"
        execute_command(self, cmd)
        cmd = f"systemctl status {service}"
        execute_command(self, cmd)

def validate_file(file):
    if not os.path.exists(file):
        raise MotrError(errno.ENOENT, f"{file} does not exist")

# Check if file paths are valid
def validate_files(files):
    for file in files:
        if not os.path.exists(file):
            raise MotrError(errno.ENOENT, f"{file} does not exist")

# Create directories
def create_dirs(self, dirs):
    for entry in dirs:
        if not os.path.exists(entry):
            cmd = f"mkdir -p {entry}"
            execute_command(self, cmd, logging=False)

def is_hw_node(self):
    try:
        node_type = self.server_node['type']
    except:
        raise MotrError(errno.EINVAL, "node_type not found")

    check_type(node_type, str, "node type")
    if node_type == "HW":
        return True
    else:
        return False

def validate_motr_rpm(self):
    '''
        1. check m0tr.ko exists in current kernel modules
        2. check /etc/sysconfig/motr
    '''
    cmd = "uname -r"
    cmd_res = execute_command(self, cmd)
    op = cmd_res[0]
    kernel_ver = op.replace('\n', '')
    check_type(kernel_ver, str, "kernel version")

    kernel_module = f"/lib/modules/{kernel_ver}/kernel/fs/motr/m0tr.ko"
    self.logger.info(f"Checking for {kernel_module}\n")
    validate_file(kernel_module)

    self.logger.info(f"Checking for {MOTR_SYS_CFG}\n")
    validate_file(MOTR_SYS_CFG)

def update_config_file(self, fname, kv_list):
    lines = []
    # Get all lines of file in buffer
    with open(f"{MOTR_SYS_CFG}", "r") as fp:
        for line in fp:
            lines.append(line)
    num_lines = len(lines)
    self.logger.info(f"Before update, in file {fname}, num_lines={num_lines}\n")

    #Check for keys in file
    for (k, v) in kv_list:
        found = False
        for lno in range(num_lines):
            # If found, update inline.
            if lines[lno].startswith(f"{k}="):
                lines[lno] = f"{k}={v}\n"
                found = True
                break
        # If not found, append
        if not found:
            lines.append(f"{k}={v}\n")
            found = False

    num_lines = len(lines)
    self.logger.info(f"After update, in file {fname}, num_lines={num_lines}\n")

    # Write buffer to file
    with open(f"{MOTR_SYS_CFG}", "w+") as fp:
        for line in lines:
            fp.write(f"{line}")

def update_copy_motr_config_file(self):
    local_path = self.local_path
    log_path = self.log_path
    machine_id = self.machine_id
    validate_files([MOTR_SYS_CFG, local_path, log_path])
    MOTR_M0D_DATA_DIR = f"{local_path}/motr"
    if not os.path.exists(MOTR_M0D_DATA_DIR):
        create_dirs(self, [f"{MOTR_M0D_DATA_DIR}"])
    MOTR_LOCAL_SYSCONFIG_DIR = f"{MOTR_M0D_DATA_DIR}/sysconfig"
    if not os.path.exists(MOTR_LOCAL_SYSCONFIG_DIR):
        create_dirs(self, [f"{MOTR_LOCAL_SYSCONFIG_DIR}"])

    MOTR_M0D_CONF_DIR = f"{MOTR_LOCAL_SYSCONFIG_DIR}/{machine_id}"
    MOTR_M0D_CONF_XC = f"{MOTR_M0D_CONF_DIR}/confd.xc"
    MOTR_M0D_ADDB_STOB_DIR = f"{log_path}/motr/{machine_id}/addb"
    MOTR_M0D_TRACE_DIR = f"{log_path}/motr/{machine_id}/trace"
    # Skip MOTR_CONF_XC
    dirs = [MOTR_M0D_DATA_DIR, MOTR_M0D_ADDB_STOB_DIR, MOTR_M0D_TRACE_DIR, MOTR_M0D_CONF_DIR]
    create_dirs(self, dirs)

    # Update new config keys to config file /etc/sysconfig/motr
    config_kvs = [("MOTR_M0D_CONF_DIR", f"{MOTR_M0D_CONF_DIR}"),
                   ("MOTR_M0D_DATA_DIR", f"{MOTR_M0D_DATA_DIR}"),
                   ("MOTR_M0D_CONF_XC", f"{MOTR_M0D_CONF_XC}"),
                   ("MOTR_M0D_ADDB_STOB_DIR", f"{MOTR_M0D_ADDB_STOB_DIR}"),
                   ("MOTR_M0D_TRACE_DIR", f"{MOTR_M0D_TRACE_DIR}")]

    update_config_file(self, f"{MOTR_SYS_CFG}", config_kvs)

    # Copy config file to new path
    cmd = f"cp {MOTR_SYS_CFG} {MOTR_M0D_CONF_DIR}"
    execute_command(self, cmd)

# Get lists of metadata disks from Confstore of all cvgs
# Input: node_info
# Output: [['/dev/sdc'], ['/dev/sdf']]
#        where ['/dev/sdc'] is list of metadata disks of cvg[0]
#              ['/dev/sdf'] is list of metadata disks of cvg[1]
def get_md_disks_lists(self, node_info):
    md_disks_lists = []
    cvg_count = node_info[CVG_COUNT_KEY]
    cvg = node_info['cvg']
    for i in range(cvg_count):
        temp_cvg = cvg[i]
        if temp_cvg['devices']['metadata']:
            md_disks_lists.append(temp_cvg['devices']['metadata'])
    self.logger.info(f"md_disks lists on node = {md_disks_lists}\n")
    return md_disks_lists

# Get metada disks from list of lists of metadata disks of
# different cvgs of node
# Input: [['/dev/sdc'], ['/dev/sdf']]
#        where ['/dev/sdc'] is ist of metadata disks of cvg[0]
#              ['/dev/sdf'] is list of metadata disks of cvg[1]
# Output: ['/dev/sdc', '/dev/sdf']
def get_mdisks_from_list(self, md_lists):
    md_disks = []
    md_len_outer = len(md_lists)
    for i in range(md_len_outer):
        md_len_innner = len(md_lists[i])
        for j in range(md_len_innner):
            md_disks.append(md_lists[i][j])
    self.logger.info(f"md_disks on node = {md_disks}\n")
    return md_disks

# Update metadata disk entries to motr-hare confstore
def update_to_file(self, index, url, machine_id, md_disks):
    ncvgs = len(md_disks)
    for i in range(ncvgs):
        md = md_disks[i]
        len_md = len(md)
        for j in range(len_md):
            md_disk = md[j]
            self.logger.info(f"setting key server>{machine_id}>cvg[{i}]>m0d[{j}]>md_seg1"
                         f" with value {md_disk} in {url}")
            Conf.set(index, f"server>{machine_id}>cvg[{i}]>m0d[{j}]>md_seg1",f"{md_disk}")
            Conf.save(index)

# populate self.storage_nodes with machine_id for all storage_nodes
def get_data_nodes(self):
    machines: Dict[str,Any] = self.nodes
    storage_nodes: List[str] = []
    services = Conf.search(self._index, 'node', 'services', Const.SERVICE_MOTR_IO.value)
    for machine_id in machines.keys():
       result = [svc for svc in services if machine_id in svc]
       # skipped control , HA and server pod
       if result:
           storage_nodes.append(machine_id)
    return storage_nodes

def update_motr_hare_keys(self, nodes):
    # key = machine_id value = node_info
    for machine_id in self.storage_nodes:
        node_info = nodes.get(machine_id)
        md_disks_lists = get_md_disks_lists(self, node_info)
        update_to_file(self, self._index_motr_hare, self._url_motr_hare, machine_id, md_disks_lists)

# Write below content to /etc/cortx/motr/mini_prov_logrotate.conf file so that mini_mini_provisioner
# log file will be rotated hourly and retained recent max 4 files. Max size of log file is 10M.

# Content:
# /etc/cortx/log/motr/<machine-id>/mini_provisioner {
#    hourly
#    size 10M
#    rotate 4
#    delaycompress
#    copytruncate
# }
def add_entry_to_logrotate_conf_file(self):
    MOTR_M0D_DATA_DIR = f"{self.local_path}/motr"
    validate_files([MOTR_M0D_DATA_DIR])
    mini_prov_conf_file = f"{MOTR_M0D_DATA_DIR}/mini_prov_logrotate.conf"

    indent=' '*4
    lines=["{a} {b}\n".format(a=self.logfile, b='{'),
           f"{indent}hourly\n",
           f"{indent}size 10M\n",
           f"{indent}rotate 4\n",
           f"{indent}delaycompress\n",
           f"{indent}copytruncate\n",
           "{a}\n".format(a='}')]
    with open(f"{mini_prov_conf_file}", 'w+') as fp:
        for line in lines:
            fp.write(line)

def motr_config_k8(self):
    if not verify_libfabric(self):
        raise MotrError(errno.EINVAL, "libfabric is not up.")

    # To rotate mini_provisioner log file
    add_entry_to_logrotate_conf_file(self)

    if self.machine_id not in self.storage_nodes:
        # Modify motr config file
        update_copy_motr_config_file(self)
        return

    # If setup_size is large i.e.HW, read the (key,val)
    # from /opt/seagate/cortx/motr/conf/motr.conf and
    # update to /etc/sysconfig/motr
    if self.setup_size == "large":
        cmd = "{} {}".format(MOTR_CONFIG_SCRIPT, " -c")
        execute_command(self, cmd, verbose = True)

    update_motr_hare_keys(self, self.nodes)
    execute_command(self, MOTR_CONFIG_SCRIPT, verbose = True)

    # Update be_seg size only for storage node
    update_bseg_size(self)

    # Modify motr config file
    update_copy_motr_config_file(self)
    return

def motr_config(self):
    # Just to check if lnet is working properly
    try:
       transport_type = self.server_node['network']['data']['transport_type']
    except:
       raise MotrError(errno.EINVAL, "transport_type not found")

    check_type(transport_type, str, "transport_type")

    if transport_type == "lnet":
        if not verify_lnet(self):
            raise MotrError(errno.EINVAL, "lent is not up.")
    elif transport_type == "libfabric":
        if not verify_libfabric(self):
            raise MotrError(errno.EINVAL, "libfabric is not up.")

    is_hw = is_hw_node(self)
    if is_hw:
        self.logger.info(f"Executing {MOTR_CONFIG_SCRIPT}")
        execute_command(self, MOTR_CONFIG_SCRIPT, verbose = True)

def configure_net(self):
    """Wrapper function to detect lnet/libfabric transport."""
    try:
        transport_type = Conf.get(self._index, 'cortx>motr>transport_type')
    except:
        raise MotrError(errno.EINVAL, "transport_type not found")

    check_type(transport_type, str, "transport_type")

    if transport_type == "lnet":
        configure_lnet(self)
    elif transport_type == "libfab":
        configure_libfabric(self)
    else:
        raise MotrError(errno.EINVAL, "Unknown data transport type\n")

def configure_lnet(self):
    '''
       Get iface and /etc/modprobe.d/lnet.conf params from
       conf store. Configure lnet. Start lnet service
    '''
    try:
        iface = self.server_node['network']['data']['private_interfaces'][0]
    except:
        raise MotrError(errno.EINVAL, "private_interfaces[0] not found\n")

    self.logger.info(f"Validate private_interfaces[0]: {iface}\n")
    cmd = f"ip addr show {iface}"
    execute_command(self, cmd)

    try:
        iface_type = self.server_node['network']['data']['interface_type']
    except:
        raise MotrError(errno.EINVAL, "interface_type not found\n")

    lnet_config = (f"options lnet networks={iface_type}({iface}) "
                  f"config_on_load=1  lnet_peer_discovery_disabled=1\n")
    self.logger.info(f"lnet config: {lnet_config}")

    with open(LNET_CONF_FILE, "w") as fp:
        fp.write(lnet_config)

    execute_command(self, "systemctl enable lnet")
    restart_services(self, ["lnet"])
    time.sleep(2)
    # Ping to nid
    self.logger.info("Doing ping to nids\n")
    ret = lnet_self_ping(self)
    if not ret:
       raise MotrError(errno.EINVAL, "lent self ping failed\n")

def configure_libfabric(self):
    cmd = "fi_info"
    execute_command(self, cmd, verbose=True)

def verify_libfabric(self):
    cmd = "fi_info"
    execute_command(self, cmd)
    return True

def swap_on(self):
    cmd = "swapon -a"
    execute_command(self, cmd)

def swap_off(self):
    cmd = "swapoff -a"
    execute_command(self, cmd, retries=3)

def add_swap_fstab(self, dev_name):
    '''
        1. check swap entry found in /etc/fstab
        2. if found, do nothing
        3. if not found, add swap entry in /etc/fstab
    '''
    swap_entry = f"{dev_name}    swap    swap    defaults        0 0\n"
    swap_found = False
    swap_off(self)

    try:
        with open(FSTAB, "r") as fp:
            lines = fp.readlines()
            for line in lines:
                ret = line.find(dev_name)
                if ret == 0:
                    swap_found = True
                    self.logger.info(f"Swap entry found: {swap_entry}\n")
    except:
        swap_on(self)
        raise MotrError(errno.EINVAL, f"Cant read f{FSTAB}\n")

    try:
        if not swap_found:
            with open(FSTAB, "a") as fp:
                fp.write(swap_entry)
            self.logger.info(f"Swap entry added: {swap_entry}\n")
    except:
        raise MotrError(errno.EINVAL, f"Cant append f{FSTAB}\n")
    finally:
        swap_on(self)

def del_swap_fstab_by_vg_name(self, vg_name):
    swap_off(self)

    cmd = f"sed -i '/{vg_name}/d' {FSTAB}"
    execute_command(self, cmd)

    swap_on(self)

def create_swap(self, swap_dev):
    self.logger.info(f"Make swap of {swap_dev}\n")
    cmd = f"mkswap -f {swap_dev}"
    execute_command(self, cmd)

    self.logger.info(f"Test {swap_dev} swap device\n")
    cmd = f"test -e {swap_dev}"
    execute_command(self, cmd)

    self.logger.info(f"Adding {swap_dev} swap device to {FSTAB}\n")
    add_swap_fstab(self, swap_dev)


def create_lvm(self, index, metadata_dev):
    '''
        1. validate /etc/fstab
        2. validate metadata device file
        3. check requested volume group exist
        4. if exist, remove volume group and swap related with it.
           because if user request same volume group with different device.
        5. If not exist, create volume group and lvm
        6. create swap from lvm
    '''
    try:
        cmd = f"fdisk -l {metadata_dev}2"
        execute_command(self, cmd)
    except MotrError:
        pass
    else:
        metadata_dev = f"{metadata_dev}2"

    try:
        cmd = f"pvdisplay {metadata_dev}"
        out = execute_command(self, cmd)
    except MotrError:
        pass
    else:
        self.logger.warning(f"Volumes are already created on {metadata_dev}\n{out[0]}\n")
        return False

    index = index + 1
    node_name = self.server_node['name']
    vg_name = f"vg_{node_name}_md{index}"
    lv_swap_name = f"lv_main_swap{index}"
    lv_md_name = f"lv_raw_md{index}"
    swap_dev = f"/dev/{vg_name}/{lv_swap_name}"

    self.logger.info(f"metadata device: {metadata_dev}\n")

    self.logger.info(f"Checking for {FSTAB}\n")
    validate_file(FSTAB)

    self.logger.info(f"Checking for {metadata_dev}\n")
    validate_file(metadata_dev)

    cmd = f"fdisk -l {metadata_dev}"
    execute_command(self, cmd)

    try:
        cmd = f"vgs {vg_name}"
        execute_command(self, cmd)
    except MotrError:
        pass
    else:
        self.logger.info(f"Removing {vg_name} volume group\n")

        del_swap_fstab_by_vg_name(self, vg_name)

        cmd = f"vgchange -an {vg_name}"
        execute_command(self, cmd)

        cmd = f"vgremove {vg_name} -ff"
        execute_command(self, cmd)

    self.logger.info(f"Creating physical volume from {metadata_dev}\n")
    cmd = f"pvcreate {metadata_dev} --yes"
    execute_command(self, cmd)

    self.logger.info(f"Creating {vg_name} volume group from {metadata_dev}\n")
    cmd = f"vgcreate {vg_name} {metadata_dev}"
    execute_command(self, cmd)

    self.logger.info(f"Adding {node_name} tag to {vg_name} volume group\n")
    cmd = f"vgchange --addtag {node_name} {vg_name}"
    execute_command(self, cmd)

    self.logger.info("Scanning volume group\n")
    cmd = "vgscan --cache"
    execute_command(self, cmd)

    self.logger.info(f"Creating {lv_swap_name} lvm from {vg_name}\n")
    cmd = f"lvcreate -n {lv_swap_name} {vg_name} -l 51%VG --yes"
    execute_command(self, cmd)

    self.logger.info(f"Creating {lv_md_name} lvm from {vg_name}\n")
    cmd = f"lvcreate -n {lv_md_name} {vg_name} -l 100%FREE --yes"
    execute_command(self, cmd)

    swap_check_cmd = "free -m | grep Swap | awk '{print $2}'"
    free_swap_op = execute_command(self, swap_check_cmd)
    allocated_swap_size_before = int(float(free_swap_op[0].strip(' \n')))
    create_swap(self, swap_dev)
    allocated_swap_op = execute_command(self, swap_check_cmd)
    allocated_swap_size_after = int(float(allocated_swap_op[0].strip(' \n')))
    if allocated_swap_size_before >= allocated_swap_size_after:
        raise MotrError(errno.EINVAL, f"swap size before allocation"
                        f"({allocated_swap_size_before}M) must be less than "
                        f"swap size after allocation({allocated_swap_size_after}M)\n")
    else:
        self.logger.info(f"swap size before allocation ={allocated_swap_size_before}M\n")
        self.logger.info(f"swap_size after allocation ={allocated_swap_size_after}M\n")
    return True

def calc_lvm_min_size(self, lv_path, lvm_min_size):
    cmd = f"lsblk --noheadings --bytes {lv_path} | " "awk '{print $4}'"
    res = execute_command(self, cmd)
    lv_size = res[0].rstrip("\n")
    lv_size = int(lv_size)
    self.logger.info(f"{lv_path} size = {lv_size} \n")
    if lvm_min_size is None:
        lvm_min_size = lv_size
        return lvm_min_size
    lvm_min_size = min(lv_size, lvm_min_size)
    return lvm_min_size

def get_cvg_cnt_and_cvg(self):
    try:
        cvg_cnt = self.server_node[CVG_COUNT_KEY]
    except:
        raise MotrError(errno.EINVAL, "cvg_cnt not found\n")

    check_type(cvg_cnt, str, CVG_COUNT_KEY)

    try:
        cvg = self.server_node['cvg']
    except:
        raise MotrError(errno.EINVAL, "cvg not found\n")

    # Check if cvg type is list
    check_type(cvg, list, "cvg")

    # Check if cvg is non empty
    if not cvg:
        raise MotrError(errno.EINVAL, "cvg is empty\n")
    return cvg_cnt, cvg

def validate_storage_schema(storage):
    check_type(storage, list, "storage")
    for elem in storage:
        check_type(elem, dict, "storage element")
        for key, val in elem.items():
            if key=="name":
                val_type=str
                check_type(val, val_type, key)
            if key=="type":
                val_type=str
                check_type(val, val_type, key)
            if key=="metadata_devices":
                val_type=list
                check_type(val, val_type, key)
                sz = len(val)
                for i in range(sz):
                    check_type(val[i], str, f"metadata_devices[{i}]")
            if key=="data_devices":
                val_type=list
                check_type(val, val_type, key)
                sz = len(val)
                for i in range(sz):
                    check_type(val[i], str, f"data_devices[{i}]")

def align_val(val, size):
    return (int(val/size) * size)

def update_bseg_size(self):
    dev_count = 0
    lvm_min_size = None

    md_disks_list = get_md_disks_lists(self, self.node)
    md_disks = get_mdisks_from_list(self, md_disks_list)
    md_len = len(md_disks)
    for i in range(md_len):
        lvm_min_size = calc_lvm_min_size(self, md_disks[i], lvm_min_size)
    if lvm_min_size:
        align_val(lvm_min_size, ALLIGN_SIZE)
        self.logger.info(f"setting MOTR_M0D_IOS_BESEG_SIZE to {lvm_min_size}\n")
        cmd = f'sed -i "/MOTR_M0D_IOS_BESEG_SIZE/s/.*/MOTR_M0D_IOS_BESEG_SIZE={lvm_min_size}/" {MOTR_SYS_CFG}'
        execute_command(self, cmd)
    return

def config_lvm(self):
    dev_count = 0
    lvm_min_size = None
    lvm_min_size = None

    cvg_cnt, cvg = get_cvg_cnt_and_cvg(self)
    for i in range(int(cvg_cnt)):
        cvg_item = cvg[i]
        try:
            metadata_devices = cvg_item["metadata_devices"]
        except:
            raise MotrError(errno.EINVAL, "metadata devices not found\n")
        check_type(metadata_devices, list, "metadata_devices")
        self.logger.info(f"\nlvm metadata_devices: {metadata_devices}\n\n")

        for device in metadata_devices:
            ret = create_lvm(self, dev_count, device)
            if ret == False:
                continue
            dev_count += 1
            lv_md_name = f"lv_raw_md{dev_count}"
            cmd = f"lvs -o lv_path | grep {lv_md_name}"
            res = execute_command(self, cmd)
            lv_path = res[0].rstrip("\n")
            lvm_min_size = calc_lvm_min_size(self, lv_path, lvm_min_size)
    if lvm_min_size:
        self.logger.info(f"setting MOTR_M0D_IOS_BESEG_SIZE to {lvm_min_size}\n")
        cmd = f'sed -i "/MOTR_M0D_IOS_BESEG_SIZE/s/.*/MOTR_M0D_IOS_BESEG_SIZE={lvm_min_size}/" {MOTR_SYS_CFG}'
        execute_command(self, cmd)

def get_lnet_xface() -> str:
    """Get lnet interface."""
    lnet_xface = None
    try:
        with open(LNET_CONF_FILE, 'r') as f:
            # Obtain interface name
            for line in f.readlines():
                if len(line.strip()) <= 0: continue
                tokens = re.split(r'\W+', line)
                if len(tokens) > 4:
                    lnet_xface = tokens[4]
                    break
    except:
        raise MotrError(errno.EINVAL, f"Cant parse {LNET_CONF_FILE}")

    if lnet_xface == None:
        raise MotrError(errno.EINVAL,
                        f"Cant obtain iface details from {LNET_CONF_FILE}")
    if lnet_xface not in os.listdir(SYS_CLASS_NET_DIR):
        raise MotrError(errno.EINVAL,
                        f"Invalid iface {lnet_xface} in lnet.conf")
    return lnet_xface

def check_pkgs(self, pkgs):
    """Check rpm packages."""
    for pkg in pkgs:
        ret = 1
        cmd = f"rpm -q {pkg}"

        try:
            cmd_res = execute_command(self, cmd)
            ret = cmd_res[1]
        except MotrError:
            pass

        if ret == 0:
            self.logger.info(f"rpm found: {pkg}\n")
        else:
            raise MotrError(errno.ENOENT, f"Missing rpm: {pkg}")

def get_nids(self, nodes):
    """Get lnet nids of all available nodes in cluster."""
    nids = []
    myhostname = self.server_node["hostname"]

    for node in nodes:
        if (myhostname == node):
            cmd = "lctl list_nids"
        else:
            cmd = (f"ssh  {node}"
                    " lctl list_nids")

        op = execute_command(self, cmd)
        nids.append(op[0].rstrip("\n"))

    return nids

def get_nodes(self):
    nodes_info = Conf.get(self._index, 'server_node')
    nodes= []
    for value in nodes_info.values():
        nodes.append(value["hostname"])
    return nodes

def lnet_ping(self):
    """Lnet lctl ping on all available nodes in cluster."""
    nodes = get_nodes(self)
    # nodes is a list of hostnames
    nids = get_nids(self, nodes)
    self.logger.info("lnet pinging on all nodes in cluster\n")
    for nid in nids:
       cmd = f"lctl ping {nid}"
       self.logger.info(f"lctl ping on: {nid}\n")
       execute_command(self, cmd)

def test_lnet(self):
    '''
        1. check lustre rpm
        2. validate lnet interface which was configured in init
        3. ping on lnet interface
        4. lctl ping on all nodes in cluster. motr_setup post_install and prepare
           MUST be performed on all nodes before executing this step.
    '''
    self.logger.info("post_install and prepare phases MUST be performed "
                     "on all nodes before executing test phase\n")
    search_lnet_pkgs = ["kmod-lustre-client", "lustre-client"]
    check_pkgs(self, search_lnet_pkgs)

    lnet_xface = get_lnet_xface()
    self.logger.info(f"lnet interface found: {lnet_xface}\n")

    cmd = f"ip addr show {lnet_xface}"
    cmd_res = execute_command(self, cmd)
    ip_addr = cmd_res[0]

    try:
        ip_addr = ip_addr.split("inet ")[1].split("/")[0]
        self.logger.info(f"lnet interface ip: {ip_addr}\n")
    except:
        raise MotrError(errno.EINVAL, f"Cant parse {lnet_xface} ip addr")

    self.logger.info(f"ping on: {ip_addr}\n")
    cmd = f"ping -c 3 {ip_addr}"
    execute_command(self, cmd)

    lnet_ping(self)

def test_libfabric(self):
    search_libfabric_pkgs = ["libfabric"]
    check_pkgs(self, search_libfabric_pkgs)

def get_metadata_disks_count(self):
    dev_count = 0
    cvg_cnt, cvg = get_cvg_cnt_and_cvg(self)
    for i in range(int(cvg_cnt)):
        cvg_item = cvg[i]
        try:
            metadata_devices = cvg_item["metadata_devices"]
        except:
            raise MotrError(errno.EINVAL, "metadata devices not found\n")
        check_type(metadata_devices, list, "metadata_devices")
        self.logger.info(f"\nlvm metadata_devices: {metadata_devices}\n\n")

        for device in metadata_devices:
            dev_count += 1
    return dev_count

def lvm_exist(self):
    metadata_disks_count = get_metadata_disks_count(self)
    node_name = self.server_node['name']

    # Fetch lvm paths of existing lvm's e.g. /dev/vg_srvnode-1_md1/lv_raw_md1
    lv_list = execute_command(self, "lvdisplay | grep \"LV Path\" | awk \'{ print $3 }\'")[0].split('\n')
    lv_list = lv_list[0:len(lv_list)-1]

    # Check if motr lvms are already created.
    # If all are already created, return
    for i in range(1, metadata_disks_count+1):
        md_lv_path = f'/dev/vg_{node_name}_md{i}/lv_raw_md{i}'
        swap_lv_path = f'/dev/vg_{node_name}_md{i}/lv_main_swap{i}'

        if md_lv_path in lv_list:
            if swap_lv_path in lv_list:
                continue
            else:
                self.logger.warning(f"{swap_lv_path} does not exist. Need to create lvm\n")
                return False
        else:
            self.logger.warning(f"{md_lv_path} does not exist. Need to create lvm\n")
            return False
    return True

def cluster_up(self):
    cmd = '/usr/bin/hctl status'
    self.logger.info(f"Executing cmd : '{cmd}'\n")
    ret = execute_command_without_exception(self, cmd)
    if ret == 0:
        return True
    else:
        return False

def pkg_installed(self, pkg):
    cmd = f'/usr/bin/yum list installed {pkg}'
    ret = execute_command_without_exception(self, cmd)
    if ret == 0:
        self.logger.info(f"{pkg} is installed\n")
        return True
    else:
        self.logger.info(f"{pkg} is not installed\n")
        return False

def test_io(self):
    mix_workload_path = f"{MOTR_WORKLOAD_DIR}/mix_workload.yaml"
    m0worklaod_path = f"{MOTR_WORKLOAD_DIR}/m0workload"
    m0crate_path = f"{MOTR_WORKLOAD_DIR}/m0crate_workload_batch_1_file1.yaml"
    if (
        os.path.isfile(m0worklaod_path) and
        os.path.isfile(mix_workload_path) and
        os.path.isfile(m0crate_path)
       ):
        cmd = f"{m0worklaod_path} -t {mix_workload_path}"
        out = execute_command(self, cmd, timeout_secs=1000)
        self.logger.info(f"{out[0]}\n")
    else:
        self.logger.error("workload files are missing\n")

    # Configure motr mini provisioner logger.
    # File to log motr mini prov logs: /var/log/seagate/motr/mini_provisioner.
    # Currently we log to both console and /var/log/seagate/motr/mini_provisioner.
    # Firstly check if /var/log/seagate/motr exist. If not, create it.

def config_logger(self):
    logger = logging.getLogger(LOGGER)
    if not os.path.exists(LOGDIR):
        try:
            os.makedirs(LOGDIR, exist_ok=True)
            with open(f'{self.logfile}', 'w'): pass
        except:
            raise MotrError(errno.EINVAL, f"{self.logfile} creation failed\n")
    else:
        if not os.path.exists(self.logfile):
            try:
                with open(f'{self.logfile}', 'w'): pass
            except:
                raise MotrError(errno.EINVAL, f"{self.logfile} creation failed\n")
    logger.setLevel(logging.DEBUG)
    # create file handler which logs debug message in log file
    fh = logging.FileHandler(self.logfile)
    fh.setLevel(logging.DEBUG)
    # create console handler to log messages ERROR and above
    ch = logging.StreamHandler()
    ch.setLevel(logging.ERROR)
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    fh.setFormatter(formatter)
    ch.setFormatter(formatter)
    logger.addHandler(fh)
    logger.addHandler(ch)
    return logger

def remove_dirs(self, log_dir, patterns):
    if not os.path.exists(os.path.dirname(log_dir)):
        self.logger.warning(f"{log_dir} does not exist")
        return

    if len(patterns) == 0:
        self.logger.info(f"Removing {log_dir}")
        execute_command(self, f"rm -rf {log_dir}")
        return

    for pattern in patterns:
        removed_dirs = []

        # Search directories for files/dirs with pattern in their names and remove it.
        # e.g. removes addb* dirs from /var/motr
        # search_pat=/var/motr/addb*

        search_pat = "{}/{}*".format(log_dir, pattern)
        for dname in glob.glob(search_pat, recursive=True):
            removed_dirs.append(dname)
            execute_command(self, f"rm -rf {dname}")
        if len(removed_dirs) > 0:
            self.logger.info(f"Removed below directories of pattern {pattern} from {log_dir}.\n{removed_dirs}")

def remove_logs(self, patterns):
    for log_dir in MOTR_LOG_DIRS:
        if os.path.exists(log_dir):
            remove_dirs(self, log_dir, patterns)
        else:
            self.logger.warning(f"{log_dir} does not exist")
    if os.path.exists(IVT_DIR):
        self.logger.info(f"Removing {IVT_DIR}")
        execute_command(self, f"rm -rf {IVT_DIR}")

def check_services(self, services):
    for service in services:
        self.logger.info(f"Checking status of {service} service\n")
        cmd = f"systemctl status {service}"
        execute_command(self, cmd)
        ret = execute_command_without_exception(self, cmd)
        if ret != 0:
            return False
    return True

def verify_lnet(self):
    self.logger.info("Doing ping to nids.\n")
    ret = lnet_self_ping(self)
    if not ret:
        # Check if lnet is up. If lnet is not up, restart lnet and try ping nid.
        # Else, ping nid after some delay since lnet is already up.
        if not check_services(self, ["lnet"]):
            self.logger.info("lnet is not up. Restaring lnet.\n")
            restart_services(self, ["lnet"])
            self.logger.info("Doing ping to nids after 5 seconds.\n")
        else:
            self.logger.warning("lnet is up. Doing ping to nids after 5 seconds.\n")
        execute_command_without_exception(self, "sleep 5")
        ret = lnet_self_ping(self)
    return ret

def lnet_self_ping(self):
    nids = []

    op = execute_command(self, "lctl list_nids")
    nids.append(op[0].strip("\n"))
    self.logger.info(f"nids= {nids}\n")
    for nid in nids:
       cmd = f"lctl ping {nid}"
       self.logger.info(f"lctl ping on: {nid}\n")
       ret = execute_command_without_exception(self, cmd)
       if ret != 0:
            return False
    return True

def update_motr_hare_keys_for_all_nodes(self):
    hostname = self.server_node["hostname"]
    nodes_info = Conf.get(self._index, 'server_node')
    retry_count = 60
    retry_delay = 2
    for value in nodes_info.values():
        host = value["hostname"]
        cvg_count = value[CVG_COUNT_KEY]
        name = value["name"]
        self.logger.info(f"update_motr_hare_keys for {host}\n")
        for i in range(int(cvg_count)):
            lv_path = None
            lv_md_name = f"lv_raw_md{i + 1}"

            if (hostname == value["hostname"]):
                cmd = ("lvs -o lv_path")
                res = execute_command_verbose(self, cmd)
                r = re.compile(f".*{lv_md_name}")
                try:
                    lvm_find = list(filter(r.match,res[0].split()))
                    lv_path = lvm_find[0].strip()
                except Exception as e:
                    self.logger.info(f"exception pass {e}\n")
            else:
                cmd = (f"ssh  {host}"
                       f" \"lvs -o lv_path\"")
                for retry in range(1, retry_count):
                    self.logger.info(f"Getting LVM data for {host}, attempt: {retry}\n")
                    res = execute_command_verbose(self, cmd)
                    r = re.compile(f".*{lv_md_name}")
                    try:
                        lvm_find = list(filter(r.match,res[0].split()))
                        lv_path = lvm_find[0].strip()
                    except Exception as e:
                        self.logger.info(f"exception pass {e}\n")
                    if lv_path:
                        self.logger.info(f"found lvm {lv_path} after {retry} count")
                        break
                    else:
                        time.sleep(retry_delay)
            if not lv_path:
                raise MotrError(res[1], f"[ERR] {lv_md_name} not found on {host}\n")
            self.logger.info(f"setting key server>{name}>cvg[{i}]>m0d[0]>md_seg1"
                             f" with value {lv_path} in {self._motr_hare_conf}")
            Conf.set(self._index_motr_hare,f"server>{name}>cvg[{i}]>m0d[0]>md_seg1",f"{lv_path.strip()}")
            Conf.save(self._index_motr_hare)

    for value in nodes_info.values():
        if (hostname == value["hostname"]):
            continue
        else:
            host = value["hostname"]
            cmd = (f"scp  {self._motr_hare_conf}"
                    f" {host}:{self._motr_hare_conf}")
            execute_command(self, cmd)

# Get voulme groups created on metadata devices mentioned in config file
def get_vol_grps(self):
    cvg_cnt, cvg = get_cvg_cnt_and_cvg(self)

    vol_grps = []
    for i in range(int(cvg_cnt)):
        cvg_item = cvg[i]
        try:
            metadata_devices = cvg_item["metadata_devices"]
        except:
            raise MotrError(errno.EINVAL, "metadata devices not found\n")
        check_type(metadata_devices, list, "metadata_devices")
        self.logger.info(f"lvm metadata_devices: {metadata_devices}")

        for device in metadata_devices:
            cmd = f"pvs | grep {device} " "| awk '{print $2}'"
            ret = execute_command(self, cmd)
            if ret[0]:
                vol_grps.append(ret[0].strip())
    return vol_grps

def lvm_clean(self):
    self.logger.info("Removing cortx lvms")
    vol_grps = get_vol_grps(self)
    if (len(vol_grps) == 0):
        self.logger.info("No cortx volume groups (e.g. vg_srvnode-1_md1) are found \n")
        return
    self.logger.info(f"Volume groups found: {vol_grps}")
    self.logger.info("Executing swapoff -a")
    swap_off(self)
    self.logger.info(f"Removing cortx LVM entries from {FSTAB}")
    execute_command(self, f"sed -i.bak '/vg_srvnode/d' {FSTAB}")
    for vg in vol_grps:
        cmd = f"pvs|grep {vg} |" "awk '{print $1}'"
        pv_names = execute_command(self, cmd)[0].split('\n')[0:-1]
        cmd = f"lvs|grep {vg} |" "awk '{print $1}'"
        lv_names = execute_command(self, cmd)[0].split('\n')[0:-1]

        # Removing logical volumes
        for lv in lv_names:
            lv_path = f"/dev/{vg}/{lv}"
            self.logger.info(f"Executing lvchange -an {lv_path}")
            execute_command(self, f"lvchange -an {lv_path}")
            self.logger.info(f"Executing lvremove {lv_path}")
            execute_command(self, f"lvremove {lv_path}")
            if os.path.exists(lv_path):
                self.logger.info("Removing dmsetup entries using cmd "
                                 f"\'dmsetup remove {lv_path}\'")
                execute_command(self, f"dmsetup remove {lv_path}")

        # Removing volume groups
        self.logger.info(f"Executing vgchange -an {vg}")
        execute_command(self, f"vgchange -an {vg}")
        self.logger.info(f"Executing vgremove {vg}")
        execute_command(self, f"vgremove {vg}")

        # Removing physical volumes
        for pv in pv_names:
            self.logger.info(f"Executing pvremove {pv}")
            execute_command(self, f"pvremove {pv}")
            self.logger.info(f"Executing wipefs -a {pv}")
            execute_command(self, f"wipefs -a {pv}")

    # In some case, we still have dm entries even though all VG, LV, PV
    # are removed. This is observed in hw setups where lvms are not cleaned up
    remove_dm_entries(self)

def remove_dm_entries(self):
    cmd = "ls -l /dev/vg_srvnode*/* | awk '{print $9}'"
    lv_paths = execute_command(self, cmd)[0].split('\n')
    for lv_path in lv_paths:
        if lv_path == '':
            continue
        else:
            if os.path.exists(lv_path):
                self.logger.info(f"dmsetup remove {lv_path}")
                execute_command(self, f"dmsetup remove {lv_path}")

def get_disk_size(self, device):
    cmd = f"fdisk -l {device} |" f"grep {device}:" "| awk '{print $5}'"
    ret = execute_command(self, cmd)
    return ret[0].strip()

def read_config(file):
    fp = open(file, "r")
    file_data = fp.read()
    config_dict = {}
    for line in file_data.splitlines():
        if line.startswith('#') or (len(line.strip()) == 0):
            continue
        entry = line.split('=',1)
        config_dict[entry[0]] = entry[1]
    return config_dict

def part_clean(self):
    cvg_cnt, cvg = get_cvg_cnt_and_cvg(self)
    dev_count = 1
    ret = 0
    for i in range(int(cvg_cnt)):
        cvg_item = cvg[i]
        try:
            metadata_devices = cvg_item["metadata_devices"]
        except:
            raise MotrError(errno.EINVAL, "metadata devices not found\n")
        check_type(metadata_devices, list, "metadata_devices")
        self.logger.info(f"\nlvm metadata_devices: {metadata_devices}\n\n")
        for device in metadata_devices:
            ret = delete_parts(self, dev_count, device)
            #if ret != 0:
            #    return ret
            dev_count += 1
    return ret

# It will give num of partitions + 1.
# To get partition numbers, subract 1 from output
def get_part_count(self, device):
   fname = os.path.split(device)
   cmd = f"lsblk -o name | grep -c {fname}"
   ret = int(execute_command(self, cmd, verbose=True)[0]) - 1
   return ret

def delete_parts(self, dev_count, device):
    # Delete 2 partitions be_log, raw_md
    total_parts = get_part_count(self, device)
    if total_parts == 0:
        self.logger.info(f"No partitions found on {device}")
        return
    self.logger.info(f"No. of partitions={total_parts} on {device}")
    for i in range(int(total_parts)):
        part_num = i + 1
        ret = delete_part(self, device, part_num)
        if ret != 0:
            self.logger.error(f"Deletion of partition({part_num}) failed on {device}")
            return ret
        time.sleep(2)

def delete_part(self, device, part_num):
    cmd = f"fdisk {device}"
    stdin_str = str("d\n"+f"{part_num}"+"\n" + "w\n")
    ret = execute_command(self, cmd, stdin=stdin_str, verbose=True)
    return ret[1]

def get_fid(self, fids, service, idx):
    fids_list = []
    len_fids_list = len(fids)

    # Prepare list of all fids of matching service
    for i in range(len_fids_list):
        if fids[i]["name"] == service:
            fids_list.append(fids[i]["fid"])

    num_fids = len(fids_list)

    # --idx argument is started from index 1, to read fetch-fids from index 0
    idx = int(idx) - 1

    if num_fids > 0:
        if idx < num_fids:
            return fids_list[idx]
        else:
            self.logger.error(f"Invalid index({idx}) of service({service})"
                              f"Valid index should be in range [0-{num_fids-1}]."
                              "Returning -1.")
            return -1
    else:
        self.logger.error(f"No fids for service({service}). Returning -1.")
        return -1

# Fetch fid of service using command 'hctl fetch-fids'
# First populate a yaml file with the output of command 'hctl fetch-fids'
# Use this yaml file to get proper fid of required service.
def fetch_fid(self, service, idx):
    hare_lib_path = f"{self.local_path}/hare/config/{self.machine_id}"
    cmd = f"hctl fetch-fids --conf-dir {hare_lib_path}"
    out = execute_command(self, cmd)
    self.logger.info(f"Available fids:\n{out[0]}\n")
    fp = open(TEMP_FID_FILE, "w")
    fp.write(out[0])
    fp.close()
    fp = open(TEMP_FID_FILE, "r")
    fids = yaml.safe_load(fp)
    if len(fids) == 0:
        self.logger.error("No fids returned by 'hctl fetch-fids'. Returning -1.\n")
        return -1
    fid = get_fid(self, fids, service, idx)
    return fid

def getListOfm0dProcess():
    '''
    Get list of running m0d process
    '''
    listOfProc = []
    # Iterate over the list
    for proc in psutil.process_iter():
       try:
           # Fetch process details as dict
           pinfo = proc.as_dict(attrs=['pid', 'name', 'username'])
           if pinfo.get('name') == "m0d":
               # Append dict to list
               listOfProc.append(pinfo);
       except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
           pass
    return listOfProc

def receiveSigTerm(signalNumber, frame):
    for proc in getListOfm0dProcess():
        cmd=f"KILL -SIGTERM {proc.get('pid')}"
        execute_command_without_log(cmd)
    return

# If service is one of [ios,confd,hax] then we expect fid to start the service
# and start services using motr-mkfs and motr-server.
# For other services like 'motr-free-space-mon' we do nothing.
def start_service(self, service, idx):
    self.logger.info(f"service={service}\nidx={idx}\n")

    if service in ["fsm", "client", "motr_client"]:
        cmd = f"{MOTR_FSM_SCRIPT_PATH}"
        execute_command_verbose(self, cmd, set_timeout=False)
        return

    # Copy mini prov logrotate script
    cmd = f"cp {MOTR_MINI_PROV_LOGROTATE_SCRIPT} {CROND_DIR}"
    execute_command(self, cmd)
    # Start crond service
    cmd = "/usr/sbin/crond start"
    execute_command(self, cmd, timeout_secs=120)

    # Copy confd_path to /etc/sysconfig
    # confd_path = MOTR_M0D_CONF_DIR/confd.xc
    confd_path = f"{self.local_path}/motr/sysconfig/{self.machine_id}/confd.xc"
    create_dirs(self, ["/etc/motr"])

    cmd = f"cp -f {confd_path} /etc/motr/"
    execute_command(self, cmd)

    cmd = f"cp -v {self.local_path}/motr/sysconfig/{self.machine_id}/motr /etc/sysconfig/"
    execute_command(self, cmd)

    fid = fetch_fid(self, service, idx)
    if fid == -1:
        return -1

    #Start motr services
    cmd = f"{MOTR_SERVER_SCRIPT_PATH} m0d-{fid}"
    execute_command_console(self, cmd)
    return
