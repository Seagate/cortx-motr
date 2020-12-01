#!/usr/bin/env bash

while [[ $# -gt 0 ]]; do
    echo "param: $1"
    shift
done

echo "workload emulation..."
sleep 30
echo "workload finished"