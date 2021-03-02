pipeline {
    agent any
    parameters {
        booleanParam(name: 'Help', defaultValue: false, description: '''Unused param for description:
This job is created for single node vm motr+s3+hare deployment using mini provisioner.
Below wiki link have all detailed commands used in this job.
https://github.com/Seagate/cortx-motr/wiki/Single-node-motr--s3---hare-deployment-on-vm-using-mini-provisioner
If you found steps are changed then need to update below simple groovy script.
https://github.com/Seagate/cortx-motr/blob/motr-jenkins/scripts/jenkins/single-node-mini-provisioner.groovy
On successfull deployment, just check console log once to get clear understanding.
If vm reset failed, then perfom manually remaining reset steps using ssc-cloudform.(steps: stop-revert_snapshot-start)
If you get noting in failure console log, then check ssh connections on nodes. 
''')
        string(name: 'VM1_FQDN', defaultValue: '', description: 'FQDN of ssc-vm primary node (node-1). (user/password must be root/seagate)')
        string(name: 'REPO_URL', defaultValue: '', description: '''Target build URL 
Example: http://cortx-storage.colo.seagate.com/releases/cortx_builds/centos-7.8.2003/589/ 
This should contain directory structure like,
../
3rd_party/                                               24-Feb-2021 04:35                   -
cortx_iso/                                                24-Feb-2021 04:34                   -
iso/                                                          24-Feb-2021 04:34                   -
python_deps/                                          31-Oct-2020 12:45                   -
RELEASE.INFO                                         24-Feb-2021 04:35                1002
THIRD_PARTY_RELEASE.INFO                  24-Feb-2021 04:35               26196
''')
        string(name: 'SSC_AUTH_ID', defaultValue: '', description: '''Add onetime RedHatCloudform credentials using below link and use ID in this param if RESET_VM is checked. 
To add new: http://ssc-vm-c-0139.colo.seagate.com:8080/credentials/store/system/domain/_/newCredentials
To get existing: http://ssc-vm-c-0139.colo.seagate.com:8080/credentials/''')

        booleanParam(name: 'RESET_VM', defaultValue: false, description: '''Revert ssc-vm to first snapshot. (First snapshot with root/seagate user/password)
If vm reset fails, then perform manual reset using ssc-cloud.''')
        booleanParam(name: 'PRE_REQ', defaultValue: true, description: 'Perform pre-req')
        booleanParam(name: 'MINI_MOTR', defaultValue: true, description: 'Run motr mini prov')
        booleanParam(name: 'MINI_S3', defaultValue: true, description: 'Run s3 mini prov')
        booleanParam(name: 'MINI_HARE', defaultValue: true, description: 'Run hare mini prov')
        booleanParam(name: 'BOOTSTRAP', defaultValue: true, description: 'Bootstrap cluster')
        booleanParam(name: 'AWS_SETUP', defaultValue: true, description: 'Setup aws, make s3 a/c and create test bucket')
        booleanParam(name: 'S3BENCH_SETUP', defaultValue: true, description: 'Install s3bench and run IO size of 1k and 10 sample')
    }
    options {
        timeout(120)  // abort the build after that many minutes
        disableConcurrentBuilds()
        timestamps()
    }

    stages {
        stage('Build params') {
            steps {
                        sh '''
                        echo "Node 1: ${VM1_FQDN}"
                        echo "Target: ${REPO_URL}"
                        '''
            }
        }

        stage('Reset VM') {
            when { expression { params.RESET_VM } }
            environment {
                SSC_AUTH = credentials("${SSC_AUTH_ID}")
                VM_FQDN = "${VM1_FQDN}"
            }
            steps {
                sh '''curl "https://raw.githubusercontent.com/Seagate/cortx-motr/motr-jenkins/scripts/jenkins/vm-reset" -o vm-reset.sh
                chmod a+x vm-reset.sh
                VERBOSE=true ./vm-reset.sh
                '''
            }
        }

        stage('Add repo') {
            when { expression { params.PRE_REQ } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
yum-config-manager --add-repo=${REPO_URL}/cortx_iso/
yum-config-manager --add-repo=${REPO_URL}/3rd_party/
yum-config-manager --add-repo=${REPO_URL}/3rd_party/lustre/custom/tcp/
                    """
                }
            }
        }

        stage('Install rpm') {
            when { expression { params.PRE_REQ } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
yum install -y consul --nogpgcheck
yum install -y cortx-motr --nogpgcheck
yum install -y cortx-hare --nogpgcheck
yum install -y cortx-py-utils --nogpgcheck
yum localinstall -y https://bintray.com/rabbitmq-erlang/rpm/download_file?file_path=erlang%2F23%2Fel%2F7%2Fx86_64%2Ferlang-23.1.5-1.el7.x86_64.rpm
curl -s https://packagecloud.io/install/repositories/rabbitmq/rabbitmq-server/script.rpm.sh | sudo bash
yum install -y rabbitmq-server
yum install -y haproxy --nogpgcheck
yum install -y openldap-servers openldap-clients
yum install -y cortx-s3server --nogpgcheck
yum install -y cortx-s3iamcli --nogpgcheck
yum install -y gcc
yum install -y python3-devel
                    """
                }
            }
        }

        stage('Install py modules') {
            when { expression { params.PRE_REQ } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
pip3 install aiohttp==3.6.1
pip3 install elasticsearch-dsl==6.4.0
pip3 install python-consul==1.1.0
pip3 install schematics==2.1.0
pip3 install toml==0.10.0
pip3 install cryptography==2.8
pip3 install PyYAML==5.1.2
pip3 install configparser==4.0.2
pip3 install networkx==2.4
pip3 install numpy==1.19.5
pip3 install matplotlib==3.1.3
pip3 install argparse==1.4.0
pip3 install confluent-kafka==1.5.0
pip3 install python-crontab==2.5.1
pip3 install elasticsearch==6.8.1
pip3 install paramiko==2.7.1
pip3 install pyldap
true
                    """
                }
            }
        }

        stage('Create confstore json') {
            when { expression { params.PRE_REQ } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
rm -f /root/provisioner_cluster.json
rm -f /etc/machine-id /var/lib/dbus/machine-id
dbus-uuidgen --ensure=/etc/machine-id
dbus-uuidgen --ensure
systemctl status network
cat /etc/machine-id
MACHINEID=`cat /etc/machine-id`

#add patch here
curl https://raw.githubusercontent.com/Seagate/cortx-utils/delim_fix/py-utils/src/utils/conf_store/conf_cli.py -o /usr/lib/python3.6/site-packages/cortx/utils/conf_store/conf_cli.py

conf json:///root/provisioner_cluster.json set "cluster>server_nodes>\$MACHINEID=srvnode-1"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>machine_id=\$MACHINEID"

HOSTNAME=`hostname`
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>hostname=\$HOSTNAME"

conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>node_type=VM"

#EDIT HERE
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>storage>metadata_devices[0]=/dev/sdb"

#EDIT HERE
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>storage>data_devices[0]=/dev/sdc"
#conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>storage>data_devices[1]=/dev/sdd"
#EDIT HERE

conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>public_interfaces[0]=eth1"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>public_interfaces[1]=eth2"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>private_interfaces[0]=eth3"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>private_interfaces[1]=eth4"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>interface_type=tcp"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>transport_type=lnet"

GENERATEDKEY=`s3cipher generate_key --const_key openldap`
echo \$GENERATEDKEY

ENCPW=`s3cipher encrypt --data 'seagate' --key \$GENERATEDKEY`
echo \$ENCPW

CLUSTERID=`conf yaml:///opt/seagate/cortx/s3/s3backgrounddelete/s3_cluster.yaml get 'cluster_config>cluster_id'|cut -d '"' -f 2`
echo \$CLUSTERID

PBIP=`ip addr show eth1|grep "inet "|awk '{print \$2}'|cut -d '/' -f 1`
echo \$PBIP

PRIP=`ip addr show eth3|grep "inet "|awk '{print \$2}'|cut -d '/' -f 1`
echo \$PRIP

conf json:///root/provisioner_cluster.json set "cluster>cluster_id=\$CLUSTERID"
conf json:///root/provisioner_cluster.json set "cluster>mgmt_vip=127.0.0.1"
conf json:///root/provisioner_cluster.json set "cluster>cluster_ip=127.0.0.1"
conf json:///root/provisioner_cluster.json set "cluster>dns_servers[0]=8.8.8.8"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>public_ip=\$PBIP"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>private_ip=\$PRIP"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>netmask=255.255.255.0"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>gateway=255.255.255.0"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>network>data>roaming_ip=127.0.0.1"
conf json:///root/provisioner_cluster.json set "cluster>srvnode-1>s3_instances=1"

conf json:///root/provisioner_cluster.json set "openldap>root>secret=\$ENCPW"
conf json:///root/provisioner_cluster.json set "openldap>sgiam>secret=\$ENCPW"
conf json:///root/provisioner_cluster.json set "openldap>root>user=admin"
conf json:///root/provisioner_cluster.json set "openldap>sgiam>user=sgiamadmin"
                    """
                }
            }
        }

        stage('Run motr mini provisioner') {
            when { expression { params.MINI_MOTR } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
/opt/seagate/cortx/motr/bin/motr_setup post_install --config json:///root/provisioner_cluster.json
/opt/seagate/cortx/motr/bin/motr_setup config --config json:///root/provisioner_cluster.json
/opt/seagate/cortx/motr/bin/motr_setup init --config json:///root/provisioner_cluster.json
/opt/seagate/cortx/motr/bin/motr_setup test --config json:///root/provisioner_cluster.json
                        """
                }
            }
        }

        stage('Run S3 mini provisioner') {
            when { expression { params.MINI_S3 } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
systemctl start rabbitmq-server
systemctl enable rabbitmq-server
systemctl status rabbitmq-server

curl https://raw.githubusercontent.com/Seagate/cortx-s3server/main/scripts/kafka/install-kafka.sh -o /root/install-kafka.sh 
curl -O https://raw.githubusercontent.com/Seagate/cortx-s3server/main/scripts/kafka/create-topic.sh -o /root/create-topic.sh
chmod a+x /root/install-kafka.sh 
chmod a+x /root/create-topic.sh

HOSTNAME=`hostname`
/root/install-kafka.sh -c 1 -i \$HOSTNAME
/root/create-topic.sh -c 1 -i \$HOSTNAME
sed -i '/PROFILE=SYSTEM/d' /etc/haproxy/haproxy.cfg
mkdir /etc/ssl/stx/ -p
curl https://raw.githubusercontent.com/Seagate/cortx-prvsnr/pre-cortx-1.0/srv/components/misc_pkgs/ssl_certs/files/stx.pem -o /etc/ssl/stx/stx.pem
ls /etc/ssl/stx/stx.pem

/opt/seagate/cortx/s3/bin/s3_setup post_install --config json:///root/provisioner_cluster.json
/opt/seagate/cortx/s3/bin/s3_setup config --config json:///root/provisioner_cluster.json
/opt/seagate/cortx/s3/bin/s3_setup init --config json:///root/provisioner_cluster.json

systemctl restart s3authserver.service
systemctl start s3backgroundproducer
systemctl start s3backgroundconsumer

echo 127.0.0.1 iam.seagate.com s3.seagate.com >> /etc/hosts
cat /etc/hosts
                        """
                    echo "Result: " + commandResult
                }
            }
        }

        stage('Run hare mini provisioner') {
            when { expression { params.MINI_HARE } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
/opt/seagate/cortx/hare/bin/hare_setup --post_install
/opt/seagate/cortx/hare/bin/hare_setup --config json:///root/provisioner_cluster.json --filename '/var/lib/hare/cluster.yaml'

conf yaml:///var/lib/hare/cluster.yaml get "nodes[0]>m0_servers[0]>io_disks>meta_data"
conf yaml:///var/lib/hare/cluster.yaml set "nodes[0]>m0_servers[0]>io_disks>meta_data=/dev/vg_srvnode-1_md1/lv_raw_md1"
conf yaml:///var/lib/hare/cluster.yaml get "nodes[0]>m0_servers[0]>io_disks>meta_data"

conf yaml:///var/lib/hare/cluster.yaml get "nodes[0]>m0_servers[1]>io_disks>meta_data"
conf yaml:///var/lib/hare/cluster.yaml set "nodes[0]>m0_servers[1]>io_disks>meta_data=/dev/vg_srvnode-1_md1/lv_raw_md1"
conf yaml:///var/lib/hare/cluster.yaml get "nodes[0]>m0_servers[1]>io_disks>meta_data"
                        """
                    echo "Result: " + commandResult
                }
            }
        }

        stage('Bootstrap cluster') {
            when { expression { params.BOOTSTRAP } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
hctl bootstrap --mkfs /var/lib/hare/cluster.yaml
hctl status
                        """
                }
            }
        }
        
        stage('AWS setup') {
            when { expression { params.AWS_SETUP } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """

s3iamcli CreateAccount -n test -e cloud@seagate.com --ldapuser sgiamadmin --ldappasswd seagate --no-ssl > s3user.txt
cat s3user.txt

curl https://raw.githubusercontent.com/Seagate/cortx-s3server/main/ansible/files/certs/stx-s3-clients/s3/ca.crt -o /etc/ssl/ca.crt
AWSKEYID=`cat s3user.txt |cut -d ',' -f 4 |cut -d ' ' -f 4`
AWSKEY=`cat s3user.txt |cut -d ',' -f 5 |cut -d ' ' -f 4`
pip3 install awscli
pip3 install awscli-plugin-endpoint
aws configure set aws_access_key_id \$AWSKEYID
aws configure set aws_secret_access_key \$AWSKEY
aws configure set plugins.endpoint awscli_plugin_endpoint 
aws configure set s3.endpoint_url http://s3.seagate.com 
aws configure set s3api.endpoint_url http://s3.seagate.com
aws configure set ca_bundle '/etc/ssl/ca.crt'
cat .aws/config
cat .aws/credentials
aws s3 mb s3://test
aws s3 ls

                        """
                }
            }
        }
        
        stage('S3 bench') {
            when { expression { params.S3BENCH_SETUP } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
yum install -y go
go get github.com/igneous-systems/s3bench
acc_id=\$(cat ~/.aws/credentials | grep aws_access_key_id | cut -d= -f2)
acc_key=\$(cat ~/.aws/credentials | grep aws_secret_access_key | cut -d= -f2)
/root/go/bin/s3bench -accessKey \$acc_id -accessSecret \$acc_key -bucket test -endpoint http://s3.seagate.com -numClients 10 -numSamples 10 -objectNamePrefix=s3workload -objectSize 1024 -verbose
                        """
                }
            }
        }
    }
}

// Method returns VM Host Information ( host, ssh cred)
def getTestMachine(String host) {
    def remote = [:]
    remote.name = 'cortx-vm-name'
    remote.host = host
    remote.user =  'root'
    remote.password = 'seagate'
    remote.allowAnyHosts = true
    remote.fileTransfer = 'scp'

    return remote
}
