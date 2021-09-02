#!/bin/bash
	echo "Testing src/motr/st/utils/motr_sys_test.sh"
	echo "--------------START-----------------"
motr/st/utils/motr_sys_test.sh run local -i KVS
	sleep 2
	echo "--------------END-------------------"
for line in $(cat $1)
do
	echo "--------------START-----------------"
	echo "testing ST:$line"
	#$line
	sleep 2
	echo "--------------END-------------------"
done 
