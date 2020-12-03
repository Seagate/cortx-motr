#!/bin/bash

set -x

rm -rf /var/perfline/dstat.$(hostname -s) || true
mkdir -p /var/perfline/dstat.$(hostname -s)

dstat --full --output /var/perfline/dstat.$(hostname -s)/dstat.csv &> /dev/null
