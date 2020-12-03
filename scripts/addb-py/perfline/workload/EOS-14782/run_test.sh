#!/bin/bash 

#TODO: 
# 1. Run cosbench with 32Mb mixed workload
# 2. Run s3bench with given workload
# 3. Sleep for 2 hrs
# 4. Stop all ongoing actions
# 5.

echo "Run Cosbench on remote node"
wid=`pdsh -S -w smc47-m07.colo.seagate.com "pushd /root/cos && ./cli.sh submit mixed_32_mb_8_buckets_100_workers_60min.xml && popd"`
wid=`echo $wid | grep "Accepted" | awk '{print $8}'`

echo "Started workload: $wid"

echo "Start s3bench(es)"
./longevity.sh -nc 128 -ns 10000 -s 128Mb -b 10 -t 60

sleep 7200

echo "Stop cosbench"
pdsh -S -w smc47-m07.colo.seagate.com "pushd /root/cos && ./cli.sh cancel $wid && popd"

echo "Stop s3becnh(es)"
pkill -9 -f s3bench
sleep 60




