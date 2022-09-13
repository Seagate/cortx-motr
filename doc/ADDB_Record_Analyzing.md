# How-to Analyse IO Operation using ADDB [WORK IN PROGRESS]
[ADDB](https://github.com/Seagate/cortx-motr/blob/main/doc/ADDB.md) records the activities of Motr system, therefore can furnish valuable information on what procedures serve as the bottleneck of the system. This guide provides an example on how to use ADDB to generate a request timeline graph to analyze an I/O process.

- Can try it on [this jupyter notebook](https://chameleoncloud.org/experiment/share/84c1d598-699b-4e8d-bee5-6fb7f5937504)
- Or build [cortx-motr](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst) and [cortx-hare](https://github.com/Seagate/cortx-motr/blob/main/doc/Quick-Start-Guide.rst) from the guides, and enable `M0_DEBUG` when building motr:
    ```
    ./autogen.sh && ./configure --with-trace-max-level=M0_DEBUG && make
    ```


## Part 1 - Perform a simple I/O
- The sample client is credited to [Daniar Kurniawan's repository](https://github.com/daniarherikurniawan/cortx-bench-extra.git)
- Change every `/mnt/extra` in the following to your home directory
```
# clone the sample client
cortx-motr/motr/examples/
wget https://github.com/daniarherikurniawan/cortx-bench-extra/blob/main/motr-clients/example1_multithd_dan.c
wget https://raw.githubusercontent.com/daniarherikurniawan/cortx-bench-extra/main/script/modify_param.py

# edit the parameters
./modify_param.py -file example1_multithd_dan.c -params "N_REQUEST=100 BLOCK_SIZE=1048576 N_PARALLEL_THD=1"

# save hctl status
hctl status > temp
export HA_ADDR=$(grep hax temp | sed 's/.*inet/inet/') && echo $HA_ADDR
export LOCAL_ADDR=$(grep -m 1 m0_client_other temp | sed 's/.*inet/inet/') && echo $LOCAL_ADDR
export PROFILE_FID=$(grep "None None" temp | sed s/.\'default.*// | sed 's/ *0x/"<0x/;s/$/>"/') && echo $PROFILE_FID
export PROCESS_FID=$(grep -m 1 m0_client_other temp | sed '0,/.*m0_client_other */s//"</' | sed 's/ *inet.*/>"/') && echo $PROCESS_FID
export obj_id=12345670

# compile the client
export LD_LIBRARY_PATH=/mnt/extra/cortx-motr/motr/.libs/
gcc -I/mnt/extra/cortx-motr -DM0_EXTERN=extern -DM0_INTERNAL= -Wno-attributes -L/mnt/extra/cortx-motr/motr/.libs -lmotr -lpthread example1_multithd_dan.c -o example1_multithd_dan

# preform a write, read, and delete
./example1_multithd_dan $HA_ADDR $LOCAL_ADDR $PROFILE_FID $PROCESS_FID $obj_id 1 0 0
./example1_multithd_dan $HA_ADDR $LOCAL_ADDR $PROFILE_FID $PROCESS_FID $obj_id 0 1 0
./example1_multithd_dan $HA_ADDR $LOCAL_ADDR $PROFILE_FID $PROCESS_FID $obj_id 0 0 1
```

## Part 2 - Generating ADDB graphs
```
# install dependencies
pip3 install pandas 
pip3 install graphviz peewee 
pip3 install numpy tqdm plumbum graphviz
sudo yum install -y python-pydot python-pydot-ng graphviz

# clone seagate-tools
cd ~
git clone --recursive https://github.com/Seagate/seagate-tools
ln -s seagate-tools/performance/PerfLine/roles/perfline_setup/files/chronometry chronometry

# choose a addb folder (e.g. addb_260150) in the directory
cd /mnt/extra/cortx-motr/motr/examples/
ls | grep addb_
addb_folder="addb_260150"
export pid="260150" # same as the addb folder number

# dump the addb stobs
sudo /mnt/extra/cortx-motr/utils/m0addb2dump -f --  /mnt/extra/cortx-motr/motr/examples/$addb_folder/o/100000000000000\:2 >  $addb_folder.addb2dump

# generate a database
export addb_generated_db="seagate_tools_$addb_folder.db"
python3 /mnt/extra/chronometry/addb2db.py --dumps $addb_folder.addb2dump --db $addb_generated_db

# find the request_id with the most occurrance
cat $addb_folder.addb2dump | grep "_id:" | awk "{print \\\$5}" | tr ',' " " | awk -F '[[:digit:]]' 'NF > 4' |  sort | uniq -c | sort -nr | head  -1 > arr_request_id.txt

export request_id=`cat arr_request_id.txt`

# generate a request timeline graph
python3 /mnt/extra/chronometry/req_timelines.py -d $addb_generated_db -p $pid "$request_id" --output-file req_timelines_"$pid"_"$request_id".png       
```

## Contributors
- Daniar Kurniawan, University of Chicago, daniar@uchicago.edu
- Faradawn Yang, University of Chicago, faradawn@uchicago.edu

