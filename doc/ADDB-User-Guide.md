# How to use ADDB
- Can try it on jupyter notebook on [Chameleon Trovi](https://chameleoncloud.org/experiment/share/84c1d598-699b-4e8d-bee5-6fb7f5937504)
- Or on a Skylake CENTOS 7.9 baremetal

## Part 1 - Building Motr
```
### First, build Motr ###
sudo mkdir -p /mnt/extra
sudo chown cc -R /mnt

# checkout the July 22's commit of Motr
cd /mnt/extra
git clone --recursive https://github.com/Seagate/cortx-motr.git
cd /mnt/extra/cortx-motr
git checkout 58f3d69e51a049eb5d5e2576040cc5ee732e2410

# install pip and python
yum group install -y -q "Development Tools"
yum install -y -q python-devel ansible tmux
curl https://bootstrap.pypa.io/pip/2.7/get-pip.py -o get-pip.py
python get-pip.py pip==19.3.1            
sudo pip -q install --target=/usr/lib64/python2.7/site-packages ipaddress

# force ansible to use python2
sudo su
echo "all:" >> /etc/ansible/hosts
echo "  ansible_python_interpreter: \"/usr/bin/python2\"" >> /etc/ansible/hosts
exit

# build dependencies (9 min)
sudo ./scripts/install-build-deps > motr-build-deps.log
tail /mnt/extra/cortx-motr/motr-build-deps.log

# configure libfabric
wget https://github.com/Seagate/cortx/releases/download/build-dependencies/libfabric-1.11.2-1.el7.x86_64.rpm
wget https://github.com/Seagate/cortx/releases/download/build-dependencies/libfabric-devel-1.11.2-1.el7.x86_64.rpm
sudo rpm -i libfabric-1.11.2-1.el7.x86_64.rpm
sudo rpm -i libfabric-devel-1.11.2-1.el7.x86_64.rpm
sudo sed -i 's|tcp(eth1)|tcp(eth0)|g' /etc/libfab.conf

# autogen, config, and make
cd /mnt/extra/cortx-motr
sudo ./autogen.sh
sudo ./configure --with-trace-max-level=M0_DEBUG
sudo make -j48 > /mnt/extra/cortx-motr/motr-build.log
tail /mnt/extra/cortx-motr/motr-build.log

# build python util
cd /mnt/extra/
sudo yum install -y gcc rpm-build python36 python36-pip python36-devel python36-setuptools openssl-devel libffi-devel python36-dbus
git clone --recursive https://github.com/Seagate/cortx-utils -b main

cd /mnt/extra/cortx-utils
./jenkins/build.sh -v 2.0.0 -b 2 > build-jenkins.log
sudo pip3 install -r https://raw.githubusercontent.com/Seagate/cortx-utils/main/py-utils/python_requirements.txt
sudo pip3 install -r https://raw.githubusercontent.com/Seagate/cortx-utils/main/py-utils/python_requirements.ext.txt

cd /mnt/extra/cortx-utils/py-utils/dist
sudo yum install -y cortx-py-utils-*.noarch.rpm
sudo find /mnt/extra -type f -name '*noarch.rpm'

### Second, build hare ###
cd /mnt/extra
git clone https://github.com/Seagate/cortx-hare.git && cd cortx-hare

# install fecter
sudo yum -y install python3 python3-devel yum-utils
sudo yum localinstall -y https://yum.puppetlabs.com/puppet/el/7/x86_64/puppet-agent-7.0.0-1.el7.x86_64.rpm
sudo ln -sf /opt/puppetlabs/bin/facter /usr/bin/facter

# install consul
sudo yum -y install yum-utils
sudo yum-config-manager --add-repo https://rpm.releases.hashicorp.com/RHEL/hashicorp.repo
sudo yum -y install consul-1.9.1

# build and install motr
cd /mnt/extra/cortx-motr && time sudo ./scripts/install-motr-service --link
export M0_SRC_DIR=$PWD

# build hare 
cd /mnt/extra/cortx-hare
sudo make &>/dev/null
sudo make install &>/dev/null

# create hare group
sudo groupadd --force hare
sudo usermod --append --groups hare cc
sudo su

# add path to bash
echo 'PATH=/opt/seagate/cortx/hare/bin:$PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/mnt/extra/cortx-motr/motr/.libs/' >> ~/.bashrc
source ~/.bashrc

# use ifconfig to check which ethX is up
sudo ifconfig | grep MULTICAST | grep RUNNING

# if eth0 is up, change lnet's default eth1 to eth0
sudo sed -i 's|tcp(eth1)|tcp(eth0)|g' /etc/modprobe.d/lnet.conf
sudo modprobe lnet
sudo lctl list_nids
  # sample output: 10.52.0.167@tcp
```

## Part 2 - Creating Loop Devices
- Can skip this step if the instance has multiple hard drives
```
# create five files, 25 GB each
mkdir -p /mnt/extra/loop-files/
cd /mnt/extra/loop-files/
dd if=/dev/zero of=loopbackfile1.img bs=100M count=250
cp loopbackfile1.img loopbackfile2.img
cp loopbackfile1.img loopbackfile3.img
cp loopbackfile1.img loopbackfile4.img
cp loopbackfile1.img loopbackfile5.img
        
# create the loop device
cd /mnt/extra/loop-files/
sudo losetup -fP loopbackfile1.img
sudo losetup -fP loopbackfile2.img
sudo losetup -fP loopbackfile3.img
sudo losetup -fP loopbackfile4.img
sudo losetup -fP loopbackfile5.img

# format devices into filesystems
printf "y" | sudo mkfs.ext4 /mnt/extra/loop-files/loopbackfile1.img 
printf "y" | sudo mkfs.ext4 /mnt/extra/loop-files/loopbackfile2.img 
printf "y" | sudo mkfs.ext4 /mnt/extra/loop-files/loopbackfile3.img 
printf "y" | sudo mkfs.ext4 /mnt/extra/loop-files/loopbackfile4.img 
printf "y" | sudo mkfs.ext4 /mnt/extra/loop-files/loopbackfile5.img 

# mount loop devices 
mkdir -p /mnt/extra/loop-devs/loop0
mkdir -p /mnt/extra/loop-devs/loop1
mkdir -p /mnt/extra/loop-devs/loop2
mkdir -p /mnt/extra/loop-devs/loop3
mkdir -p /mnt/extra/loop-devs/loop4
cd /mnt/extra/loop-devs/
sudo mount -o loop /dev/loop0 /mnt/extra/loop-devs/loop0
sudo mount -o loop /dev/loop1 /mnt/extra/loop-devs/loop1
sudo mount -o loop /dev/loop2 /mnt/extra/loop-devs/loop2
sudo mount -o loop /dev/loop3 /mnt/extra/loop-devs/loop3
sudo mount -o loop /dev/loop4 /mnt/extra/loop-devs/loop4
lsblk -f
df -h 
```

## Part 3 - Starting a Hare cluster 
```
# Prepare cdf file 
cd /mnt/extra
cp /opt/seagate/cortx/hare/share/cfgen/examples/singlenode.yaml CDF.yaml

sudo sed -i "s|hostname: localhost|hostname: $hostname |g" CDF.yaml
sudo sed -i "s|node: localhost|node: $hostname |g" CDF.yaml
sudo sed -i 's|data_iface: eth1|data_iface: eth0|g' CDF.yaml
# sudo sed -i 's|data_iface: eth0|data_iface: eth1|g' CDF.yaml

# remove the unavailable storage devices 
sed -i '/loop0/d' CDF.yaml
sed -i '/loop1/d' CDF.yaml
sed -i '/loop2/d' CDF.yaml
sed -i '/loop3/d' CDF.yaml
sed -i '/loop4/d' CDF.yaml
sed -i '/loop8/d' CDF.yaml

# set the disk for logging
sudo sed -i "s|loop9|loop8|g" CDF.yaml

# Check the modification
cd /mnt/extra
cat CDF.yaml| grep eth
cat CDF.yaml| grep hostname
cat CDF.yaml| grep node:            # only 3 loop devices 

# bootstrap the cluster
cd /mnt/extra
sudo hctl shutdown
sudo hctl bootstrap --mkfs CDF.yaml
hctl status
```

## Part 4 - Cloning a multi-threaded client
- credit to [Daniar Kurniawan's repository](https://github.com/daniarherikurniawan/cortx-bench-extra.git)
```
# copy a multi-threaded client into motr-examples
cd /mnt/extra 
git clone https://github.com/daniarherikurniawan/cortx-bench-extra.git
cd /mnt/extra/
cp cortx-bench-extra/motr-clients/*  cortx-motr/motr/examples/      # copy Motr sample client 
cp -r cortx-bench-extra/script  cortx-motr/motr/examples/
ls /mnt/extra/cortx-motr/motr/examples/

sudo su
echo "export LD_LIBRARY_PATH=/mnt/extra/cortx-motr/motr/.libs" >> /etc/bashrc
exit

# find parameters from hctl status
hctl status > temp
export HA_ADDR=$(grep hax temp | sed 's/.*inet/inet/') && echo $HA_ADDR
export LOCAL_ADDR=$(grep -m 1 m0_client_other temp | sed 's/.*inet/inet/') && echo $LOCAL_ADDR
export PROFILE_FID=$(grep "None None" temp | sed s/.\'default.*// | sed 's/ *0x/"<0x/;s/$/>"/') && echo $PROFILE_FID
export PROCESS_FID=$(grep -m 1 m0_client_other temp | sed '0,/.*m0_client_other */s//"</' | sed 's/ *inet.*/>"/') && echo $PROCESS_FID
export obj_id=12345670

# edit the client's block size and number of threads
# BLOCK_SIZE: "4096" "8192" "16384" "32768" "65536" "131072" "262144" "524288" "1048576" "2097152" "4194304" "8388608" "16777216"
./script/modify_param.py -file /mnt/extra/cortx-motr/motr/examples/example1_multithd_dan.c -params "N_REQUEST=100 BLOCK_SIZE=1048576 N_PARALLEL_THD=1"

# gcc compile the client
cd /mnt/extra/cortx-motr/motr/examples
export LD_LIBRARY_PATH=/mnt/extra/cortx-motr/motr/.libs/
gcc -I/mnt/extra/cortx-motr -DM0_EXTERN=extern -DM0_INTERNAL= -Wno-attributes -L/mnt/extra/cortx-motr/motr/.libs -lmotr -lpthread example1_multithd_dan.c -o example1_multithd_dan

# preform a write, read, and delete
  # Write/Read/Delete
  #   1  /  1 /   1
./example1_multithd_dan $HA_ADDR $LOCAL_ADDR $PROFILE_FID $PROCESS_FID $obj_id 1 0 0
./example1_multithd_dan $HA_ADDR $LOCAL_ADDR $PROFILE_FID $PROCESS_FID $obj_id 0 1 0
./example1_multithd_dan $HA_ADDR $LOCAL_ADDR $PROFILE_FID $PROCESS_FID $obj_id 0 0 1
```

## Part 5 - Generating ADDB graphs
- some important output are indented and commented  
```
# install dependencies
pip3 install pandas 
pip3 install graphviz peewee 
pip3 install numpy tqdm plumbum graphviz
sudo yum install -y python-pydot python-pydot-ng graphviz

# clone seagate-tools
cd /mnt/extra/
git clone --recursive https://github.com/Seagate/seagate-tools
ln -s seagate-tools/performance/PerfLine/roles/perfline_setup/files/chronometry chronometry

# choose a addb folder and set the variable
ls /mnt/extra/cortx-motr/motr/examples/ | grep addb_

    # addb_260150
    # addb_260499
    # addb_260863
    
addb_folder="addb_260150"

# dump the addb stobs
cd /mnt/extra/cortx-motr/motr/examples/
sudo /mnt/extra/cortx-motr/utils/m0addb2dump -f --  /mnt/extra/cortx-motr/motr/examples/$addb_folder/o/100000000000000\:2 >  $addb_folder.addb2dump
echo "Output at: $addb_folder.addb2dump"

# generate a database
python3 /mnt/extra/chronometry/addb2db.py --dumps $addb_folder.addb2dump --db $addb_generated_db
echo "Output at: $addb_generated_db"

# find an interesting request_id
cd /mnt/extra/cortx-motr/motr/examples/
cat $addb_folder.addb2dump | grep "o--> initialised" | wc | awk "{print \\\$1}"

    # Output at: addb_260863.addb2dump
    # Number of potentially interesting request_id = 0

# read the content in the database
echo "Reading addb_folder = $addb_folder" 
echo "Reading addb_generated_db = $addb_generated_db" 
cd /mnt/extra/cortx-motr/motr/examples/
echo ".table" | sqlite3 $addb_generated_db

echo "List of tables = $list_tables"

echo ""
echo "Size (#row) of each table:"

cd /mnt/extra/cortx-motr/motr/examples/
for table in $list_tables; do
    echo -n "\$table     ";
    echo "SELECT count(*) FROM \$table;" | sqlite3 $addb_generated_db
done

    # Reading addb_folder = addb_260499
    # Reading addb_generated_db = seagate_tools_addb_260499.db
    # List of tables = attr            host            relation        request         s3_request_uid

    # Size (#row) of each table:
    # attr     895
    # host     0
    # relation     799
    # request     2233
    # s3_request_uid     0


echo "List of top $top_n most interesting request id:"
echo "<occurence> <request_id>"
cd /mnt/extra/cortx-motr/motr/examples/
cat $addb_folder.addb2dump | grep "_id:" | awk "{print \\\$5}" | tr ',' " " | awk -F '[[:digit:]]' 'NF > 4' |  sort | uniq -c | sort -nr | head  -$top_n > arr_request_id.txt
cat arr_request_id.txt

    # List of top 10 most interesting request id:
    # <occurence> <request_id>
    #       7 1878 
    #       7 1868 
    #       7 1858 
    #       7 1848 
    #       7 1838 
    #       7 1828 
    #       7 1818 
    #       7 1808 
    #       7 1798 
    #       7 1788 

cd /mnt/extra/cortx-motr/motr/examples/
cat arr_request_id.txt | awk "{print \\\$2}" | while read request_id ; do
echo -n "Generating graph for pid = $pid     request id = \$request_id       #components = "

# Generate Attribute Diagram  
python3 /mnt/extra/chronometry/req_graph.py -d $addb_generated_db -p $pid \$request_id | wc -l

# Generate Timeline Diagram  
python3 /mnt/extra/chronometry/req_timelines.py -d $addb_generated_db -p $pid \$request_id --output-file req_timelines_"$pid"_\$request_id.png       
done

    # Generating graph for pid = 260499     request id = 1878       #components = 3
    # Process pid 260499, id 1878
    # 260499_1878 -> 260499_1879 (bulk_to_rpc);
    # 260499_1879 -> -4475295883372724080_168 (rpc_to_sxid);
    # Generating graph for pid = 260499     request id = 1868       #components = 4
    # Process pid 260499, id 1868
    # 260499_1868 -> 260499_1869 (bulk_to_rpc);
    # 260499_1869 -> -4475295883372724080_166 (rpc_to_sxid);
    # -4475295883372724080_166 -> 260499_1870 (sxid_to_rpc);
    # Generating graph for pid = 260499     request id = 1858       #components = 4
    # ...

# choose a request_id, set ssh key and server ip, and download the graphs
request_id=1868
scp -i $SSHKEY_FILE cc@$server_ip:/mnt/extra/cortx-motr/motr/examples/attr_graph_"$pid"_"$request_id".png .
scp -i $SSHKEY_FILE cc@$server_ip:/mnt/extra/cortx-motr/motr/examples/req_timelines_"$pid"_"$request_id".png .

    # attr_graph_260499_1868.png                    100% 8260   235.9KB/s   00:00    
    # req_timelines_260499_1868.png                 100%   29KB 432.7KB/s   00:00    
```

## Contributors
- Daniar Kurniawan: daniar@uchicago.edu
- Faradawn Yang: faradawn@uchicago.edu

