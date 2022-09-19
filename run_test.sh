#!/bin/bash

#m0boot /root/singlenode_1ios_1disk.yaml
start_time=$(cat /tmp/.m0boot-start-time)
cmd="journalctl -S "$start_time" \| grep -o 'LIBMOTR-V2.*'"
tmp_stap_out=/tmp/motr-logs
ios_num=1
conf_num=1
stat_cmd_file="/tmp/.run.collect.stats.cmd"
echo -n > $stat_cmd_file
for ios in $(hctl status | grep $(hostname) -A5 | grep -E 'ioservice|confd' | awk '{print $3}')
do
	if [ $(ps aux | grep $ios | grep -c confd.xc) -ge 1 ]; then
		conf_pid=$(ps aux | grep m0d | grep $ios | awk '{print $2}')
		cmd_awk="awk '{if (\$4 == "$conf_pid") {\$4 = "CONFD$conf_num:"}; print }'"
		cmd="$cmd $cmd_awk"
		sed -i "s/$conf_pid/CONFD$conf_num:/g" /tmp/motr-m0d-$ios.log
		echo -n "/tmp/motr-m0d-$ios.log " >> $stat_cmd_file
		(( conf_num++ ))
		continue
	fi
	ios_pid=$(ps aux | grep m0d | grep $ios | awk '{print $2}')
	cmd_awk="awk '{if (\$4 == "$ios_pid") {\$4 = "IOS$ios_num:"}; print }'"
	cmd="$cmd $cmd_awk"
	sed -i "s/$ios_pid/IOS$ios_num:/g" /tmp/motr-m0d-$ios.log
	echo -n "/tmp/motr-m0d-$ios.log " >> $stat_cmd_file
	file="$file /tmp/motr-m0d-$ios.log "
	(( ios_num++ ))
done

echo "awk '{if (\$4 == "$(pgrep hax)") {\$4 = "HAX:"}; print }'"
sed -i "s/$(pgrep hax)/HAX:/g" /tmp/motr-hare-hax.log
echo -n " /tmp/motr-hare-hax.log " >> $stat_cmd_file
if [ -n "$client_file" ]; then
	sed -i "s/$client_pid/CLIENT:/g" $client_file
	echo -n "$client_file " >> $stat_cmd_file
fi
cat $(cat $stat_cmd_file) | sort -k 2 | sed '/^$/d' > $tmp_stap_out
grep -vE 'RPC |NET |FOP |FORMATION ' $tmp_stap_out  > ${tmp_stap_out}-filtered.log
echo "start_time = $start_time output in $tmp_stap_out fileter logs in ${tmp_stap_out}-filtered.log"
