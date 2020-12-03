#!/bin/bash

set -x

CLUSTER_CONFIG_FILE="/var/lib/hare/cluster.yaml"
ASSIGNED_IPS=$(ifconfig | grep inet | awk '{print $2}' )

iostat_pid=$(pidof iostat)
kill $iostat_pid

while kill -0 "$iostat_pid"; do
    sleep 1
done

disks=`python3 - $CLUSTER_CONFIG_FILE $ASSIGNED_IPS <<EOF
import yaml
import sys

# Get ips from hosts file
hosts_file_path = '/etc/hosts'
hosts = open(hosts_file_path).readlines()
hosts = (pair for pair in hosts if len(pair.split()) == 2)
srv1_ip = next(pair.split()[0] for pair in hosts if pair.split()[1] == 'srvnode-1')
srv2_ip = next(pair.split()[0] for pair in hosts if pair.split()[1] == 'srvnode-2')
if not srv1_ip or not srv2_ip:
    print('srvnode-1 or srvnode-2 ips were not found in /etc/hosts file')
    sys.exit()
#print(srv1_ip)
#print(srv2_ip)

# Determine current node ip
ips = sys.argv[2:]
is_srv1 = any(srv1_ip in ip for ip in ips)
is_srv2 = any(srv2_ip in ip for ip in ips)
if is_srv1 and is_srv2:
    print('Current node have assigned ips for both srvnodes. Check network configuration')
    sys.exit()
if not is_srv1 and not is_srv2:
    print("Current node doesnt assigned to any of srvnodes. Check network configuration")
    sys.exit()
#print("Node determined. Its srv" + '1' if is_srv1 else '2')
node_name = 'srvnode-1' if is_srv1 else 'srvnode-2'

# Get list of block devices
with open(sys.argv[1], 'r') as config_file:
    config = yaml.safe_load(config_file)
m0servers = next(n['m0_servers'] for n in config['nodes'] if n['hostname'] == node_name)
devices = next(d for d in m0servers if next(iter(d)) == 'io_disks')['io_disks']['data']

# Print block devices
print(' '.join(devices))
EOF`

md=`mount | grep -e mero -e m0tr -e motr | awk '{print $1}'`
md_base=`echo $md | awk -F/ '{print $NF}'`
md_base=${md_base::-1}

pushd /var/perfline/iostat.$(hostname -s)

disks_dm=
> disks.mapping
for d in $disks; do
    dm=`realpath $d | xargs basename`
    disks_dm="$disks_dm $dm"
    echo "IO $d $dm" >> disks.mapping
done

dm=`multipath -ll | grep $md_base | awk '{print $3}'`
disks_dm="$disks_dm $dm"
echo "MD $md $dm" >> disks.mapping

iostat-cli --fig-size 20,20 --data iostat.log \
           --disks $disks_dm \
           --fig-output iostat.aggregated.png plot

for plot in io_rqm iops io_transfer "%util" avgrq-sz avgqu-sz await svctm; do
    iostat-cli --fig-size 20,20 --data iostat.log \
        --disks $disks_dm \
	--fig-output iostat.$plot.png plot --subplots $plot
done

current_node=`python3 - $ASSIGNED_IPS <<EOF
import sys

# Get ips from hosts file
hosts_file_path = '/etc/hosts'
hosts = open(hosts_file_path).readlines()
hosts = (pair for pair in hosts if len(pair.split()) == 2)
srv1_ip = next(pair.split()[0] for pair in hosts if pair.split()[1] == 'srvnode-1')
srv2_ip = next(pair.split()[0] for pair in hosts if pair.split()[1] == 'srvnode-2')
if not srv1_ip or not srv2_ip:
    print('ERROR: srvnode-1 or srvnode-2 ips were not found in /etc/hosts file')
    sys.exit()
#print(srv1_ip)
#print(srv2_ip)

# Determine current node ip
ips = sys.argv[1:]
is_srv1 = any(srv1_ip in ip for ip in ips)
is_srv2 = any(srv2_ip in ip for ip in ips)
if is_srv1 and is_srv2:
    print('Current node have assigned ips for both srvnodes. Check network configuration')
    sys.exit()
if not is_srv1 and not is_srv2:
    print("Current node doesnt assigned to any of srvnodes. Check network configuration")
    sys.exit()
#print("Node determined. Its srv" + '1' if is_srv1 else '2')
node_name = 'srvnode-1' if is_srv1 else 'srvnode-2'

print(node_name)
EOF`


> nodes.mapping
echo "$current_node $(hostname)" >> nodes.mapping

popd
