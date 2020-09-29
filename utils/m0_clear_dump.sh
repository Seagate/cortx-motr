#!/bin/bash

count=1
if [[ ! $# -eq 2 ]]
then
	echo "Invalid usage: m0_clear_dump -i <ioservice name>"
	exit 2
fi
while getopts i: flag
do
	case "${flag}" in
		i) ioservice=${OPTARG};;
	esac
done
ios_dir="/var/motr/m0d-$ioservice"
if [ ! -d "$ios_dir" ] 
then
	echo "Error: directory for $ioservice doest not exist" 
	exit -20 # ENOTDIR
fi
m0crate_pid=($(pgrep m0crate))
if [ -z "${m0crate_pid[0]}" ]
then
	echo "m0crate process not found, exitting"
	exit
fi

while (( ${#m0crate_pid[@]} )); do

	echo '-----------'
	echo 'Count :' $count
	echo '-----------'
	((count++))
	sleep 1000
	rm -rf "$ios_dir"/m0trace.*
	rm -rf /var/crash/core.0.*
	m0crate_pid=($(pgrep m0crate))
done
