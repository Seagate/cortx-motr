#!/bin/bash

set -x

rm -rf /var/perfline/iostat.$(hostname -s) || true
mkdir /var/perfline/iostat.$(hostname -s)

iostat -yxmt 1 &> /var/perfline/iostat.$(hostname -s)/iostat.log
