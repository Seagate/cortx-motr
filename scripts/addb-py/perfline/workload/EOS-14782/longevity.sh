#! /bin/bash
ACCESSKEY=`cat /root/.aws/credentials | grep -A 3 default | grep aws_access_key_id | cut -d " " -f3`
SECRETKEY=`cat /root/.aws/credentials | grep -A 3 default | grep secret_access_key | cut -d " " -f3`
#ENDPOINTS=https://s3.seagate.com
ENDPOINTS=http://s3.seagate.com
CURRENTPATH=`pwd`
BENCHMARKLOG=$CURRENTPATH/benchmark.log
CLIENTS=""
BUCKET=""
SAMPLES=""
pid=
SIZE=""
#runtime="8640 minute"
RUNTIME=
runtime="$RUNTIME minute"
endtime=$(date -ud "$runtime" +%s)
RUNID=run_id_`date +%F-%T`
TIMESTAMP=`date +%F-%T`


validate_args() {
    
        if [ -z $CLIENTS ] || [ -z $SAMPLES ] || [ -z $SIZE ] || [ -z $BUCKET ] || [ -z $RUNTIME ]; then
                show_usage
        fi

}

show_usage() {
        echo -e "\n \t  Usage : ./longevity.sh -nc NO_OF_CLIENTS -ns NO_OF_SAMPLES -s \"iosize\"  -b BUCKET -t runtime \n"
        echo -e "\t -nc\t:\t number of clients \n"
        echo -e "\t -ns\t:\t number of samples\n"
        echo -e "\t -s\t:\t size of the objects in bytes\n"
        echo -e "\t -b\t:\t number of buckets\n"
        echo -e "\t -t\t:\t runtime in minutes"
        echo -e "\tExample\t:\t ./longevity.sh -nc 128 -ns 1024 -s 128Mb -b 15 -t 60 \n"
        exit 1
}

s3benchmark() {

    CLIENT=`expr  $CLIENTS / $BUCKET`
    value=$(echo "$SIZE" | sed -e 's/Kb//g' | sed -e 's/Mb//g' )
    units=$(echo "$SIZE" | sed -e 's/[0-9]//g' )
    case "$units" in
       Mb)   let 'value *= 1024 * 1024'  ;;
       Kb)   let 'value *= 1024' ;;
       b|'')   let 'value += 0'    ;;
       *)
             value=
             echo "Unsupported units '$units'" >&2
             ;;
    esac

    mkdir $BENCHMARKLOG/$RUNID
#    for index in ${!CLIENT[*]}; 
    for i in $(seq 1 $BUCKET)
    do
        bucket=seagate-longevity-$RANDOM
        aws s3 mb s3://$bucket
        

        echo "Executing:  /root/go/bin/s3bench -accessKey $ACCESSKEY -accessSecret $SECRETKEY -bucket $bucket -endpoint $ENDPOINTS -numClients $CLIENT -numSamples $SAMPLES -objectSize $value   > $BENCHMARKLOG/$RUNID/s3output_CLIENTS_$CLIENT\_SIZE_$SIZE\_`date +%F-%T`"
        /root/go/bin/s3bench -accessKey $ACCESSKEY -accessSecret $SECRETKEY -bucket $bucket -endpoint $ENDPOINTS -numClients $CLIENT -numSamples $SAMPLES -objectSize $value -verbose > $BENCHMARKLOG/$RUNID/s3output_CLIENTS_$CLIENT\_SIZE_$SIZE\_`date +%F-%T` &
        pid[$index]=$!
        echo "S3bench PIDs:  ${pid[@]}"
        

    done

    while [[ $(date -u +%s) -le $endtime ]]
#while [ True ]
    do
        for index in ${!pid[*]};
        do
            if ! kill -0 ${pid[$index]} 2>/dev/null; then
                bucket=seagate-moto1-$RANDOM
                aws s3 mb s3://$bucket
                echo "Re-executing:  /root/go/bin/s3bench -accessKey $ACCESSKEY -accessSecret $SECRETKEY -bucket $bucket -endpoint $ENDPOINTS -numClients $CLIENT -numSamples $SAMPLES -objectSize $value > $BENCHMARKLOG/$RUNID/s3output_CLIENT_$CLIENT\_SIZE_$SIZE\_`date +%F-%T`"
                /root/go/bin/s3bench -accessKey $ACCESSKEY -accessSecret $SECRETKEY -bucket $bucket -endpoint $ENDPOINTS -numClients $CLIENT -numSamples $SAMPLES -objectSize $value -verbose > $BENCHMARKLOG/$RUNID/s3output_CLIENTS_$CLIENT\_SIZE_$SIZE\_`date +%F-%T` &
                pid[$index]=$!
                echo "S3bench PIDs: ${pid[@]}"
            fi
        done

    done
}

while [ ! -z $1 ]; do

        case $1 in
        -nc)    shift
                CLIENTS="$1"
        ;;

        -ns)    shift
                SAMPLES="$1"
        ;;

        -s)     shift
                SIZE="$1"
        ;;

        -b)     shift
                BUCKET="$1"
        ;;
        
        -t)     shift
                RUNTIME="$1"
        ;;
        
        *)
                show_usage
                break
        esac
        shift
done
validate_args

if [ ! -d $BENCHMARKLOG ]; then
      mkdir $BENCHMARKLOG
      s3benchmark
else
      mv $BENCHMARKLOG $CURRENTPATH/benchmark.bak_$TIMESTAMP
      mkdir $BENCHMARKLOG
      s3benchmark
fi

