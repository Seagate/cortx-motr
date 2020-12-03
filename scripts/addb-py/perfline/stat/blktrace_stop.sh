#!/bin/bash

set -x

blk_pid=$(pidof blktrace)
kill -SIGINT $blk_pid

while kill -0 "$blk_pid"; do
    sleep 1
done

pushd /var/perfline/blktrace.$(hostname -s)
for d in $(ls -1 | grep dm | awk -F. '{print $1}' | sort | uniq); do
    echo $d
    blkparse -i "$d.blktrace.*" -d $d.dump -O
    iowatcher -t $d.dump -o $d.aggregated.svg
    for graph in io tput latency queue_depth iops; do
	iowatcher -t $d.dump -o $d.$graph.svg -O $graph
    done
done

dumps=$(ls -1 | grep "dm.*dump" | awk '{print "-t "$1}' | tr '\n' ' ')
iowatcher $dumps -o node.$(hostname -s).aggregated.svg
for graph in io tput latency queue_depth iops; do
    iowatcher $dumps -o node.$(hostname -s).$graph.svg -O $graph
done

ASSIGNED_IPS=$(ifconfig | grep inet | awk '{print $2}' )

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

# Print block devices
print(node_name)
EOF`

> nodes.mapping
echo "$current_node $(hostname)" >> nodes.mapping

popd
