#!/usr/bin/env bash
#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#
#set -x

MOTR_CONF_FILE="/opt/seagate/cortx/motr/conf/motr.conf"
ETC_SYSCONFIG_MOTR="/etc/sysconfig/motr"

source "/opt/seagate/cortx/motr/common/cortx_error_codes.sh"
source "/opt/seagate/cortx/motr/common/cortx_util_funcs.sh"

help()
{
cat << EOF
    CAUTION !!!

    The [$0] script can be invoked by the provisioner, or for
    modifying specific key value pairs in [$ETC_SYSCONFIG_MOTR].
    Should be invoked with sudo.

    Usage:
    sudo $0 [ACTION] ARGS
        ACTION:  [ACTION -g|-e|-d|-s] [ARGS]
            ACTION    : Invokes the provisioner action, when
                        [sudo m0provision config] is invoked
            ACTION -g : get the value & state (enabled | disabled) for key
                        sudo $0 -g MOTR_M0D_DATA_UNITS /etc/sysconfig/motr
            ACTION -e : enable the key
                        sudo $0 -e MOTR_M0D_DATA_UNITS /etc/sysconfig/motr
            ACTION -d : disable the key
                        sudo $0 -d MOTR_M0D_DATA_UNITS /etc/sysconfig/motr
            ACTION -s : set the value for key, also enables the key
                        sudo $0 -s MOTR_M0D_DATA_UNITS 0 /etc/sysconfig/motr
EOF
}

workflow()
{
cat << EOF
    This script [$0] will take the following actions when executed by the
    m0provision:
        1. Verify that the script is invoked for physical cluster.
        2. If not physical cluster (i.e., virtual cluster), the script will
           terminate with ERR_NOT_IMPLEMENTED [$ERR_NOT_IMPLEMENTED].
        3. If physical cluster, the script will read the
           /opt/seagate/cortx/motr/conf/motr.conf and update the params in the
           /etc/sysconfig/motr.

EOF
}

file_exists_else_die() # ARG1 [FILE]
{
    if [ $# != 1 ]; then
        err "Args required [FILE]."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local FILE=$1
    if [ ! -f "$FILE" ]; then
        err "File [$FILE] not found."
        err "Number of args sent [$#]. Args [$@]"
        die $ERR_CFG_FILE_NOTFOUND
    fi
}

key_valid_else_die() # ARG1 [KEY]
{
    if [ $# != 1 ]; then
        err "Args required [KEY]."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi
    local KEY=$1
    dbg "KEY [$KEY]"
}

val_valid_else_die() # ARG1 [VALUE]
{
    if [ $# != 1 ]; then
        err "Args required [VALUE]."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi
    local VALUE=$1
    dbg "VALUE [$VALUE]"
}

set_key_value() # ARG1 [KEY] ARG2 [VALUE] ARG3 [FILE]
{
    if [ $# != 3 ]; then
        err "Args required key, value & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local VALUE=$2
    local CFG_FILE=$3

    dbg "Updating KEY [$KEY]; VALUE [$VALUE] CFG_FILE [$CFG_FILE]"

    key_valid_else_die $KEY
    val_valid_else_die "$VALUE"
    file_exists_else_die $CFG_FILE

    local TMP_DST_FILE="$CFG_FILE.`date '+%d%m%Y%H%M%s%S'`"
    local CFG_LINE_TO_WRITE=""
    local KEY_FOUND=$NO
    local CFG_LINE=""

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do

        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            dbg "Key [$KEY] in [$CFG_LINE] in founnd file [$CFG_FILE]."
            KEY_FOUND=$YES
            CFG_LINE_TO_WRITE="$KEY=$VALUE"
        else
            dbg "NO_MATCH CFG_LINE [$CFG_LINE]"
            CFG_LINE_TO_WRITE="$CFG_LINE"
        fi
        echo "$CFG_LINE_TO_WRITE" >> $TMP_DST_FILE
        echo "" >> $TMP_DST_FILE
    done

    if [ $KEY_FOUND == $NO ]; then
        dbg "Key [$KEY] not found in file [$CFG_FILE]"
        dbg "Key [$KEY] will be added and set to Value [$VALUE]."
        CFG_LINE_TO_WRITE="$KEY=$VALUE"
        echo "$CFG_LINE_TO_WRITE" >> $TMP_DST_FILE
        echo "" >> $TMP_DST_FILE
    fi
    unset $IFS

    cat $TMP_DST_FILE > $CFG_FILE
    rm -f $TMP_DST_FILE

    return $ERR_SUCCESS

}

chk_key_value()
{
    _key=$1
    _value=$2
    ret=1 # 0: value are same 1: value are different
    IFS=$'\n'
    for CFG_LINE in `cat $ETC_SYSCONFIG_MOTR`; do       
        if [[ "$CFG_LINE" == "#"* ]]; then

            # If key is commented then still we need to update
            # /etc/sysconfig/motr
            if [[ "$CFG_LINE" == *"$_key="* ]]; then
                ret=1
                break
            fi 
            dbg "Commented line [$CFG_LINE]"
        else
            IFS='='; CFG_LINE_ARRAY=($CFG_LINE); unset IFS;
            local KEY=${CFG_LINE_ARRAY[0]}
            local VALUE=${CFG_LINE_ARRAY[1]}
            KEY=$(echo $KEY | xargs)
            VALUE=$(echo $VALUE | xargs)
            if [[ "$KEY" == "$_key" && "$VALUE" == "$_value" ]];then
                ret=0
                break  
            fi
        fi
    done
    unset $IFS
    echo $ret
}

lvm_size_percentage_diff()
{
    MAX=$1
    MIN=$2
    local percent=0

   percent=$((100 * (MAX - MIN)/MAX))
   echo $percent
}

do_m0provision_action()
{
    local CFG_LINE=""
    local LVM_SIZE_MIN=0
    local LVM_SIZE_MAX=0
    local MAX_DIFF_TOLERANCE=5
    local LVM_SIZE=0
    local MD_DEVICE_CNT=0

    msg "Configuring host [`hostname -f`]"
    if [ ! -x /usr/sbin/lvs ]; then
        err "lvs command not available."
    else
        MD_DEVICES=($(lvs -o lv_path 2>/dev/null | grep "lv_raw_md" | grep srvnode  | sort -u))

        for i in "${MD_DEVICES[@]}";
        do
            LVM_SIZE=$(lvs "$i" -o LV_SIZE \
                           --noheadings --units b --nosuffix | xargs)
            ANY_ERR=$(echo "$LVM_SIZE" | grep -i ERROR | wc -l)
            if [[ ( "$ANY_ERR" != "0" ) || ( -z "$LVM_SIZE" ) ]]; then
                err "lvs $i command failed."
                msg "[$LVM_SIZE]"
            elif [[ "$MD_DEVICE_CNT" -eq 0 ]]; then
                # Initializing MIN and MAX to the first lvm size in the list. 
                LVM_SIZE_MIN=$LVM_SIZE
                LVM_SIZE_MAX=$LVM_SIZE
            elif [[ "$LVM_SIZE" -lt "$LVM_SIZE_MIN" ]]; then
                LVM_SIZE_MIN=$LVM_SIZE
            elif [[ "$LVM_SIZE" -gt "$LVM_SIZE_MAX" ]]; then
                LVM_SIZE_MAX=$LVM_SIZE
            fi
            MD_DEVICE_CNT=$((MD_DEVICE_CNT + 1))
        done
        if [[ "$LVM_SIZE_MIN" -eq 0 || "$LVM_SIZE_MAX" -eq 0 ]]; then
            err "lvm size invalid [$LVM_SIZE_MIN]"
        else
             diff_per=$(lvm_size_percentage_diff "$LVM_SIZE_MAX" "$LVM_SIZE_MIN");
             if [ "$diff_per" -lt "$MAX_DIFF_TOLERANCE" ]; then
                 msg "LVM_SIZE_MIN = $LVM_SIZE_MIN"
                 sed -i "s/MOTR_M0D_IOS_BESEG_SIZE=.*/MOTR_M0D_IOS_BESEG_SIZE=$LVM_SIZE_MIN/g" $MOTR_CONF_FILE
             else
                 err "This setup configuration seems invalid"
                 err "Difference between metadata volume size is beyond tolerance level, [ $LVM_SIZE_MAX > $LVM_SIZE_MIN ]"
             fi 
        fi
    fi 
   
    platform=$(get_platform)
    echo "Server type is $platform"
    
    if [[ $platform != "physical" ]]; then
        msg "Server type is [$platform]. Config operation is not allowed."
        msg "Only physical clusters will be configured here. "
        die $ERR_NOT_IMPLEMENTED
    fi
    
    while IFS= read -r CFG_LINE
    do
        if [[ "$CFG_LINE" == "#"* ]]; then
            dbg "Commented line [$CFG_LINE]"
        else
            dbg "Not a commented line [$CFG_LINE]"
            IFS='='; CFG_LINE_ARRAY=($CFG_LINE); unset IFS;
            local KEY=${CFG_LINE_ARRAY[0]}
            local VALUE=${CFG_LINE_ARRAY[1]}
            KEY=$(echo $KEY | xargs)
            VALUE=$(echo $VALUE | xargs)
            if [ "$KEY" != "" ]; then
                msg "Updating KEY [$KEY]; VALUE [$VALUE]"
                # update /etc/sysconfig/motr file
                # only when the key values are different
                # from motr.conf file
                res=$(chk_key_value $KEY "$VALUE")
                if [[ $res -ne 0 ]];then
                    set_key_value $KEY "$VALUE" $ETC_SYSCONFIG_MOTR
                fi
            else
                dbg "Not processing [$CFG_LINE]"
            fi
        fi
    done < $MOTR_CONF_FILE
    die $ERR_SUCCESS
}

get_option() # ARG1 [KEY] ARG2 [FILE]
{
    if [ $# != 2 ]; then
        err "Args required key & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local CFG_FILE=$2

    key_valid_else_die $KEY
    file_exists_else_die $CFG_FILE

    local KEY_FOUND=$NO
    local CFG_LINE=""

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do
        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            IFS='='; CFG_LINE_ARRAY=($CFG_LINE); IFS=$'\n'
            local K1=${CFG_LINE_ARRAY[0]}
            local V1=${CFG_LINE_ARRAY[1]}
            K1=$(echo $K1 | xargs)
            V1=$(echo $V1 | xargs)
            if [[ "$CFG_LINE" == "#"* ]]; then
                dbg "Key found in disabled CFG_LINE [$CFG_LINE]"
                msg "Disabled key [$KEY] has value [$V1]."
            else
                dbg "Key found in CFG_LINE [$CFG_LINE]"
                msg "Key [$K1] has value [$V1]."
                die $ERR_SUCCESS
            fi
        fi
    done
    unset IFS

    die $ERR_CFG_KEY_NOTFOUND
}

dsb_option() # ARG1 [KEY] ARG2 [FILE]
{
    if [ $# != 2 ]; then
        err "Args required key & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local CFG_FILE=$2

    key_valid_else_die $KEY
    file_exists_else_die $CFG_FILE

    local TMP_DST_FILE="$CFG_FILE.`date '+%d%m%Y%H%M%s%S'`"
    local CFG_LINE_TO_WRITE=""
    local KEY_FOUND=$NO
    local CFG_LINE=""

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do
        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            if [[ "$CFG_LINE" == "#"* ]]; then
                msg "Already disabled [$CFG_LINE]"
                rm -f $TMP_DST_FILE
                die $ERR_CFG_KEY_OPNOTALLOWED
            else
                CFG_LINE_TO_WRITE="# $CFG_LINE"
                KEY_FOUND=$YES
            fi
        else
            CFG_LINE_TO_WRITE="$CFG_LINE"
        fi
        echo "$CFG_LINE_TO_WRITE" >> $TMP_DST_FILE
        echo "" >> $TMP_DST_FILE
    done

    unset $IFS

    if [ $KEY_FOUND == $YES ]; then
        cat $TMP_DST_FILE > $CFG_FILE
        rm -f $TMP_DST_FILE
        die $ERR_SUCCESS
    fi
    rm -f $TMP_DST_FILE
    die $ERR_CFG_KEY_NOTFOUND
}

enb_option() # ARG1 [KEY] ARG2 [CFG_FILE]
{
    if [ $# != 2 ]; then
        err "Args required key & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local CFG_FILE=$2

    key_valid_else_die $KEY
    file_exists_else_die $CFG_FILE

    IFS=$'\n'
    for CFG_LINE in `cat $CFG_FILE`; do
        if [[ "$CFG_LINE" == *"$KEY="* ]]; then
            IFS='='; CFG_LINE_ARRAY=($CFG_LINE); IFS=$'\n'
            local K1=${CFG_LINE_ARRAY[0]}
            local V1=${CFG_LINE_ARRAY[1]}
            K1=$(echo $K1 | xargs)
            V1=$(echo $V1 | xargs)
            if [[ "$CFG_LINE" == "#"* ]]; then
                dbg "Key found in disabled CFG_LINE [$CFG_LINE]"
                msg "Enabled key [$K1] has value [$V1]."
                set_key_value $KEY "$V1" $CFG_FILE
                die $ERR_SUCCESS
            else
                dbg "Key found in CFG_LINE [$CFG_LINE]"
                msg "Key [$K1] has value [$V1] is already enabled."
                die $ERR_CFG_KEY_OPNOTALLOWED
            fi
        fi
    done
    unset IFS

    err "Key [$KEY] not found."
    die $ERR_CFG_KEY_NOTFOUND
}

set_option() # ARG1 [KEY] ARG2 [VALUE] ARG3 [CFG_FILE]
{
    if [ $# != 3 ]; then
        err "Args required key, value & file."
        err "Number of args sent [$#]. Args [$@]"
        die $EER_ERR_INVALID_ARGS
    fi

    local KEY=$1
    local VALUE=$2
    local CFG_FILE=$3

    key_valid_else_die $KEY
    val_valid_else_die "$VALUE"
    file_exists_else_die $CFG_FILE

    dbg "Updating KEY [$KEY]; VALUE [$VALUE] CFG_FILE [$CFG_FILE]"
    set_key_value $KEY "$VALUE" $CFG_FILE
    die $ERR_SUCCESS
}

## main
is_sudoed

workflow

if [ "$1" == "-e" ]; then
    enb_option "$2" "$3"
elif [ "$1" == "-d" ]; then
    dsb_option "$2" "$3"
elif [ "$1" == "-g" ]; then
    get_option "$2" "$3"
elif [ "$1" == "-s" ]; then
    set_option "$2" "$3" "$4"
else
    if [ $# == 0 ]; then
        PCMD=$(ps -o args= $PPID)
        dbg "PCMD [$PCMD]"
        do_m0provision_action
        #if [[ "$PCMD" == *"m0provision config" ]]; then
        #    do_m0provision_action
        #else
        #    err "The m0provision may only call this script without any args."
        #    err "PCMD [$PCMD]"
        #fi
    fi
fi

err "Invalid args. Count [$#] Args [$@]"
help
die $ERR_INVALID_ARGS
