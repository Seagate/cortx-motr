#!/bin/bash

set -x

pids=`ps ax | grep dstat | grep -v grep | grep -v dstat_stop | grep -v dstat_start | awk '{print $1}'`
for pid in $pids ; do
    kill $pid

    while kill -0 "$pid"; do
	sleep 1
    done
done

