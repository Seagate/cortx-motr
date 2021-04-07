pipeline {
    agent any
    parameters {
        booleanParam(name: 'Help', defaultValue: false, description: '''Unused param for description:
This job is created for 3 node vm motr+s3+hare deployment using mini provisioner.
Below wiki link have all detailed commands used in this job.
https://github.com/Seagate/cortx-motr/wiki/Three-node-motr-s3-hare-deployment-on-vm-using-mini-provisioner
If you found steps are changed then need to update below simple groovy script.
https://github.com/Seagate/cortx-motr/blob/motr-jenkins/scripts/jenkins/three-node-mini-provisioner.groovy
On successfull deployment, just check console log once to get clear understanding.
If vm reset failed, then perfom manually remaining reset steps using ssc-cloudform.(steps: stop-revert_snapshot-start)
If you get noting in failure console log, then check ssh connections on nodes. 
''')
        string(name: 'VM1_FQDN', defaultValue: '', description: 'FQDN of ssc-vm primary node (node-1). (user/password must be root/seagate)')
        string(name: 'VM2_FQDN', defaultValue: '', description: 'FQDN of ssc-vm secondary node 1(node-2). (user/password must be root/seagate)')
        string(name: 'VM3_FQDN', defaultValue: '', description: 'FQDN of ssc-vm secondary node 2(node-3). (user/password must be root/seagate)')
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
To add new: http://ssc-vm-2590.colo.seagate.com:8080/credentials/store/system/domain/_/newCredentials
To get existing: http://ssc-vm-2590.colo.seagate.com:8080/credentials/''')

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
        timeout(120)
        timestamps()
    }

    stages {
        stage('Build params') {
            steps {
                        sh '''
                        echo "Node 1: ${VM1_FQDN}"
                        echo "Node 2: ${VM2_FQDN}"
                        echo "Node 3: ${VM3_FQDN}"
                        echo "Target: ${REPO_URL}"
                        '''
            }
        }

        stage('Reset VM') {
            when { expression { params.RESET_VM } }
            environment {
                SSC_AUTH = credentials("${SSC_AUTH_ID}")
            }
            parallel {
                stage ('reset-vm-node1'){
                    environment {
                        VM_FQDN = "${VM1_FQDN}"
                    }
                    when { expression { params.RESET_VM } }
                    steps {
                        sh '''curl "https://raw.githubusercontent.com/Seagate/cortx-motr/motr-jenkins/scripts/jenkins/vm-reset" -o vm-reset.sh
                        chmod a+x vm-reset.sh
                        VERBOSE=true ./vm-reset.sh
                        '''
                    }
                }
                stage ('reset-vm-node2'){
                    environment {
                        VM_FQDN = "${VM2_FQDN}"
                    }
                    when { expression { params.RESET_VM } }
                    steps {
                        sh '''curl "https://raw.githubusercontent.com/Seagate/cortx-motr/motr-jenkins/scripts/jenkins/vm-reset" -o vm-reset.sh
                        chmod a+x vm-reset.sh
                        VERBOSE=true ./vm-reset.sh
                        '''
                    }
                }
                stage ('reset-vm-node3'){
                    environment {
                        VM_FQDN = "${VM3_FQDN}"
                    }
                    when { expression { params.RESET_VM } }
                    steps {
                        sh '''curl "https://raw.githubusercontent.com/Seagate/cortx-motr/EOS-14750/scripts/jenkins/vm-reset" -o vm-reset.sh
                        chmod a+x vm-reset.sh
                        VERBOSE=true ./vm-reset.sh
                        '''
                    }
                }
            }
        }
        stage('Exchange ssh keys') {
            when { expression { params.PRE_REQ } }
            parallel {
                stage ('Exchange ssh keys node 1'){
                    steps {
                        script {
                            exchangeSSHKey(VM1_FQDN, VM2_FQDN, VM3_FQDN)
                        }
                    }
                }
                stage ('Exchange ssh keys node 2'){
                    steps {
                        script {
                            exchangeSSHKey(VM2_FQDN, VM1_FQDN, VM3_FQDN)
                        }
                    }
                }
                stage ('Exchange ssh keys node 3'){
                    steps {
                        script {
                            exchangeSSHKey(VM3_FQDN, VM1_FQDN, VM2_FQDN)
                        }
                    }
                }
            }
        }


        stage('Add repo') {
            when { expression { params.PRE_REQ } }
            parallel {
                stage ('Add repo node 1'){
                    steps {
                        script {
                            addRepo(VM1_FQDN)
                        }
                    }
                }
                stage ('Add repo node 2'){
                    steps {
                        script {
                            addRepo(VM2_FQDN)
                        }
                    }
                }
                stage ('Add repo node 3'){
                    steps {
                        script {
                            addRepo(VM3_FQDN)
                        }
                    }
                }
            }
        }

        stage('Install rpm') {
            when { expression { params.PRE_REQ } }
            parallel {
                stage ('Install RPM node 1'){
                    steps {
                        script {
                            installRPM(VM1_FQDN)
                        }
                    }
                }
                stage ('Install RPM node 2'){
                    steps {
                        script {
                            installRPM(VM2_FQDN)
                        }
                    }
                }
                stage ('Install RPM node 3'){
                    steps {
                        script {
                            installRPM(VM3_FQDN)
                        }
                    }
                }
            }
        }

        stage('Update machine id') {
            when { expression { params.PRE_REQ } }
            parallel {
                stage ('Update machine id node 1'){
                    steps {
                        script {
                            updateMachineID(VM1_FQDN)
                        }
                    }
                }
                stage ('Update machine id node 2'){
                    steps {
                        script {
                            updateMachineID(VM2_FQDN)
                        }
                    }
                }
                stage ('Update machine id node 3'){
                    steps {
                        script {
                            updateMachineID(VM3_FQDN)
                        }
                    }
                }
            }
        }

        stage('Create confstore json') {
            environment {
                        VM_FQDN = "${VM2_FQDN}"
                    }
            when { expression { params.PRE_REQ } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: '''

######node-1
#curl -o /opt/seagate/cortx/motr/bin/motr_mini_prov.py https://raw.githubusercontent.com/Seagate/cortx-motr/motr_mini_provisioner_30March/scripts/install/opt/seagate/cortx/motr/bin/motr_mini_prov.py
#curl -o /opt/seagate/cortx/motr/bin/motr_setup https://raw.githubusercontent.com/Seagate/cortx-motr/motr_mini_provisioner_30March/scripts/install/opt/seagate/cortx/motr/bin/motr_setup
#curl -o /opt/seagate/cortx/motr/conf/motr.post_install.tmpl https://raw.githubusercontent.com/Seagate/cortx-motr/motr_mini_provisioner_7April/scripts/install/opt/seagate/cortx/motr/conf/motr.post_install.tmpl
#curl -o /opt/seagate/cortx/motr/conf/motr.prepare.tmpl https://raw.githubusercontent.com/Seagate/cortx-motr/motr_mini_provisioner_7April/scripts/install/opt/seagate/cortx/motr/conf/motr.prepare.tmpl
echo "Node test : $VM_FQDN"
echo "Node 1: $VM1_FQDN"
echo "Node 2: $VM2_FQDN"
echo "Node 3: $VM3_FQDN"
echo "Target: $REPO_URL"

HOSTNAME1=`hostname`
HOSTNAME2=`cat /tmp/hostname2`
HOSTNAME3=`cat /tmp/hostname3`
MACHINEID1=`cat /etc/machine-id`
MACHINEID2=$(ssh $HOSTNAME2 cat /etc/machine-id)
MACHINEID3=$(ssh $HOSTNAME3 cat /etc/machine-id)
echo $HOSTNAME1 
echo $HOSTNAME2 
echo $HOSTNAME3
echo $MACHINEID1
echo $MACHINEID2
echo $MACHINEID3
CLUSTER_ID=5c427765-ecf5-4387-bfa4-d6d53494b159
nr_grp="2"
data="4"
parity="2"
spare="0"
POOL_TYPE="sns"

cp /opt/seagate/cortx/motr/conf/motr.post_install.tmpl /opt/seagate/cortx/motr/conf/motr.post_install_bak.tmpl
cp /opt/seagate/cortx/motr/conf/motr.prepare.tmpl /opt/seagate/cortx/motr/conf/motr.prepare_bak.tmpl
cp /opt/seagate/cortx/motr/conf/motr.config.tmpl /opt/seagate/cortx/motr/conf/motr.config_bak.tmpl
cp /opt/seagate/cortx/motr/conf/motr.test.tmpl /opt/seagate/cortx/motr/conf/motr.test_bak.tmpl

cp /opt/seagate/cortx/motr/conf/motr.post_install.tmpl /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
cp /opt/seagate/cortx/motr/conf/motr.post_install.tmpl /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
cp /opt/seagate/cortx/motr/conf/motr.post_install.tmpl /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl

cp /opt/seagate/cortx/motr/conf/motr.prepare.tmpl /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
cp /opt/seagate/cortx/motr/conf/motr.prepare.tmpl /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
cp /opt/seagate/cortx/motr/conf/motr.prepare.tmpl /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl

cp /opt/seagate/cortx/motr/conf/motr.config.tmpl /opt/seagate/cortx/motr/conf/motr.config_node1.tmpl
cp /opt/seagate/cortx/motr/conf/motr.config.tmpl /opt/seagate/cortx/motr/conf/motr.config_node2.tmpl
cp /opt/seagate/cortx/motr/conf/motr.config.tmpl /opt/seagate/cortx/motr/conf/motr.config_node3.tmpl

cp /opt/seagate/cortx/motr/conf/motr.test.tmpl /opt/seagate/cortx/motr/conf/motr.test_node1.tmpl
cp /opt/seagate/cortx/motr/conf/motr.test.tmpl /opt/seagate/cortx/motr/conf/motr.test_node2.tmpl
cp /opt/seagate/cortx/motr/conf/motr.test.tmpl /opt/seagate/cortx/motr/conf/motr.test_node3.tmpl



sed -i "s#TMPL_MACHINE_ID#$MACHINEID1#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_NAME#srvnode-1#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_DATADEVICE_00#/dev/sdc#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_DATADEVICE_01#/dev/sdd#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_METADATADEVICE_00#/dev/sdb#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_DATADEVICE_10#/dev/sdf#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_DATADEVICE_11#/dev/sdg#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_METADATADEVICE_10#/dev/sde#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_CVG_NR_GROUP#'$nr_grp'#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_CLUSTER_ID#$CLUSTER_ID#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_POOL_TYPE#$POOL_TYPE#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_POOL_DATA#$data#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_POOL_PARITY#$parity#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl
sed -i "s#TMPL_POOL_SPARE#$spare#" /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID2#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_NAME#srvnode-2#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_DATADEVICE_00#/dev/sdc#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_DATADEVICE_01#/dev/sdd#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_METADATADEVICE_00#/dev/sdb#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_DATADEVICE_10#/dev/sdf#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_DATADEVICE_11#/dev/sdg#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_METADATADEVICE_10#/dev/sde#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_CVG_NR_GROUP#'$nr_grp'#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_CLUSTER_ID#$CLUSTER_ID#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_POOL_TYPE#$POOL_TYPE#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_POOL_DATA#$data#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_POOL_PARITY#$parity#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl
sed -i "s#TMPL_POOL_SPARE#$spare#" /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID3#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_NAME#srvnode-3#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_DATADEVICE_00#/dev/sdc#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_DATADEVICE_01#/dev/sdd#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_METADATADEVICE_00#/dev/sdb#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_DATADEVICE_10#/dev/sdf#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_DATADEVICE_11#/dev/sdg#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_METADATADEVICE_10#/dev/sde#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_CVG_NR_GROUP#'$nr_grp'#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_CLUSTER_ID#$CLUSTER_ID#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_POOL_TYPE#$POOL_TYPE#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_POOL_DATA#$data#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_POOL_PARITY#$parity#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl
sed -i "s#TMPL_POOL_SPARE#$spare#" /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl


sed -i "s#TMPL_MACHINE_ID#$MACHINEID1#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_DATADEVICE_00#/dev/sdc#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_DATADEVICE_01#/dev/sdd#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_DATADEVICE_10#/dev/sdf#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_DATADEVICE_11#/dev/sdg#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_CVG_NR_GROUP#'$nr_grp'#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_IFACE_TYPE#tcp#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_INTERFACE#eth1#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_XPORT_TYPE#lnet#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_CLUSTER_ID#$CLUSTER_ID#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_POOL_TYPE#$POOL_TYPE#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_POOL_DATA#'$data'#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_POOL_PARITY#'$parity'#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl
sed -i "s#TMPL_POOL_SPARE#'$spare'#" /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl


sed -i "s#TMPL_MACHINE_ID#$MACHINEID2#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_DATADEVICE_00#/dev/sdc#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_DATADEVICE_01#/dev/sdd#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_DATADEVICE_10#/dev/sdf#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_DATADEVICE_11#/dev/sdg#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_CVG_NR_GROUP#'$nr_grp'#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_IFACE_TYPE#tcp#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_INTERFACE#eth1#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_XPORT_TYPE#lnet#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_CLUSTER_ID#$CLUSTER_ID#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_POOL_TYPE#$POOL_TYPE#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_POOL_DATA#'$data'#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_POOL_PARITY#'$parity'#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl
sed -i "s#TMPL_POOL_SPARE#'$spare'#" /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl


sed -i "s#TMPL_MACHINE_ID#$MACHINEID3#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_DATADEVICE_00#/dev/sdc#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_DATADEVICE_01#/dev/sdd#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_DATADEVICE_10#/dev/sdf#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_DATADEVICE_11#/dev/sdg#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_CVG_NR_GROUP#'$nr_grp'#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_IFACE_TYPE#tcp#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_INTERFACE#eth1#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_XPORT_TYPE#lnet#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_CLUSTER_ID#$CLUSTER_ID#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_POOL_TYPE#$POOL_TYPE#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_POOL_DATA#'$data'#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_POOL_PARITY#'$parity'#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl
sed -i "s#TMPL_POOL_SPARE#'$spare'#" /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID1#" /opt/seagate/cortx/motr/conf/motr.config_node1.tmpl
sed -i "s#TMPL_TYPE#VM#" /opt/seagate/cortx/motr/conf/motr.config_node1.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID2#" /opt/seagate/cortx/motr/conf/motr.config_node2.tmpl
sed -i "s#TMPL_TYPE#VM#" /opt/seagate/cortx/motr/conf/motr.config_node2.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID3#" /opt/seagate/cortx/motr/conf/motr.config_node3.tmpl
sed -i "s#TMPL_TYPE#VM#" /opt/seagate/cortx/motr/conf/motr.config_node3.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID1#" /opt/seagate/cortx/motr/conf/motr.test_node1.tmpl
sed -i "s#TMPL_HOSTNAME#$HOSTNAME1#" /opt/seagate/cortx/motr/conf/motr.test_node1.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID2#" /opt/seagate/cortx/motr/conf/motr.test_node2.tmpl
sed -i "s#TMPL_HOSTNAME#$HOSTNAME2#" /opt/seagate/cortx/motr/conf/motr.test_node2.tmpl

sed -i "s#TMPL_MACHINE_ID#$MACHINEID3#" /opt/seagate/cortx/motr/conf/motr.test_node3.tmpl
sed -i "s#TMPL_HOSTNAME#$HOSTNAME3#" /opt/seagate/cortx/motr/conf/motr.test_node3.tmpl


curl -o /var/lib/hare/cluster.yaml https://raw.githubusercontent.com/Seagate/cortx-motr/create_confstorekey/scripts/install/opt/seagate/cortx/motr/share/examples/threenode.yaml
sed -i "s/{HOSTNAME1}/$HOSTNAME1/" /var/lib/hare/cluster.yaml
sed -i "s/{HOSTNAME2}/$HOSTNAME2/" /var/lib/hare/cluster.yaml
sed -i "s/{HOSTNAME3}/$HOSTNAME3/" /var/lib/hare/cluster.yaml

#cp /root/provisioner_cluster.json on all three nodes on same location.
#scp /root/provisioner_cluster.json $HOSTNAME2:/root/provisioner_cluster.json
#scp /root/provisioner_cluster.json $HOSTNAME3:/root/provisioner_cluster.json

cp /opt/seagate/cortx/motr/conf/motr.post_install_node1.tmpl /opt/seagate/cortx/motr/conf/motr.post_install.tmpl
scp /opt/seagate/cortx/motr/conf/motr.post_install_node2.tmpl $HOSTNAME2:/opt/seagate/cortx/motr/conf/motr.post_install.tmpl
scp /opt/seagate/cortx/motr/conf/motr.post_install_node3.tmpl $HOSTNAME3:/opt/seagate/cortx/motr/conf/motr.post_install.tmpl

cp /opt/seagate/cortx/motr/conf/motr.prepare_node1.tmpl /opt/seagate/cortx/motr/conf/motr.prepare.tmpl
scp /opt/seagate/cortx/motr/conf/motr.prepare_node2.tmpl $HOSTNAME2:/opt/seagate/cortx/motr/conf/motr.prepare.tmpl
scp /opt/seagate/cortx/motr/conf/motr.prepare_node3.tmpl $HOSTNAME3:/opt/seagate/cortx/motr/conf/motr.prepare.tmpl

cp /opt/seagate/cortx/motr/conf/motr.config_node1.tmpl /opt/seagate/cortx/motr/conf/motr.config.tmpl
scp /opt/seagate/cortx/motr/conf/motr.config_node2.tmpl $HOSTNAME2:/opt/seagate/cortx/motr/conf/motr.config.tmpl
scp /opt/seagate/cortx/motr/conf/motr.config_node3.tmpl $HOSTNAME3:/opt/seagate/cortx/motr/conf/motr.config.tmpl

cp /opt/seagate/cortx/motr/conf/motr.test_node1.tmpl /opt/seagate/cortx/motr/conf/motr.test.tmpl
scp /opt/seagate/cortx/motr/conf/motr.test_node2.tmpl $HOSTNAME2:/opt/seagate/cortx/motr/conf/motr.test.tmpl
scp /opt/seagate/cortx/motr/conf/motr.test_node3.tmpl $HOSTNAME3:/opt/seagate/cortx/motr/conf/motr.test.tmpl
                    '''
                }
            }
        }

        stage('Run motr mini prov') {
            when { expression { params.MINI_MOTR } }
            parallel {
                stage ('Install motr node 1'){
                    steps {
                        script {
                            miniMotr(VM1_FQDN)
                        }
                    }
                }
                stage ('Install motr node 2'){
                    steps {
                        script {
                            miniMotr(VM2_FQDN)
                        }
                    }
                }
                stage ('Install motr node 3'){
                    steps {
                        script {
                            miniMotr(VM3_FQDN)
                        }
                    }
                }
            }
        }
    
        
    
        stage('Run S3 mini prov') {
            when { expression { params.MINI_S3 } }
            parallel {
                stage ('Install s3 node 1'){
                    steps {
                        script {
                            miniS3(VM1_FQDN)
                        }
                    }
                }
                stage ('Install s3 node 2'){
                    steps {
                        script {
                            miniS3(VM2_FQDN)
                        }
                    }
                }
                stage ('Install s3 node 3'){
                    steps {
                        script {
                            miniS3(VM3_FQDN)
                        }
                    }
                }
            }
        }

        stage('Run hare mini prov') {
            when { expression { params.MINI_HARE } }
            parallel {
                stage ('Install hare node 1'){
                    steps {
                        script {
                            miniHare(VM1_FQDN)
                        }
                    }
                }
                stage ('Install hare node 2'){
                    steps {
                        script {
                            miniHare(VM2_FQDN)
                        }
                    }
                }
                stage ('Install hare node 3'){
                    steps {
                        script {
                            miniHare(VM3_FQDN)
                        }
                    }
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

        stage('Run motr mini prov test') {
            when { expression { params.MINI_MOTR } }
            parallel {
                stage ('Test motr node 1'){
                    steps {
                        script {
                            miniMotrtest(VM1_FQDN)
                        }
                    }
                }
                stage ('Test motr node 2'){
                    steps {
                        script {
                            miniMotrtest(VM2_FQDN)
                        }
                    }
                }
                stage ('Test motr node 3'){
                    steps {
                        script {
                            miniMotrtest(VM3_FQDN)
                        }
                    }
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
                       """
                    commandResult = sshCommand remote: remote, command: """
acc_id=\$(cat ~/.aws/credentials | grep aws_access_key_id | cut -d= -f2)
acc_key=\$(cat ~/.aws/credentials | grep aws_secret_access_key | cut -d= -f2)
/root/go/bin/s3bench -accessKey \$acc_id -accessSecret \$acc_key -bucket test -endpoint http://s3.seagate.com -numClients 10 -numSamples 10 -objectNamePrefix=s3workload -objectSize 1024 -verbose
sleep 30
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
def addRepo(String host) {
    def remote = getTestMachine(host)
    def commandResult = sshCommand remote: remote, command: """
curl -s http://cortx-storage.colo.seagate.com/releases/cortx/third-party-deps/rpm/install-cortx-prereq.sh | bash
yum-config-manager --add-repo=${REPO_URL}/cortx_iso/
yum-config-manager --add-repo=${REPO_URL}/3rd_party/
yum-config-manager --add-repo=${REPO_URL}/3rd_party/lustre/custom/tcp/
echo "Node 1: $VM1_FQDN"
echo "Node 2: $VM2_FQDN"
echo "Node 3: $VM3_FQDN"
echo "Target: $REPO_URL"
echo $VM1_FQDN > /tmp/hostname1
echo $VM2_FQDN > /tmp/hostname2
echo $VM3_FQDN > /tmp/hostname3
    """
}
def installRPM(String host) {
    def remote = getTestMachine(host)
    def commandResult = sshCommand remote: remote, command: """
yum install -y cortx-motr cortx-hare cortx-py-utils  --nogpgcheck
    """
}

def updateMachineID(String host) {
    def remote = getTestMachine(host)
    def commandResult = sshCommand remote: remote, command: """
rm -f /etc/machine-id /var/lib/dbus/machine-id
dbus-uuidgen --ensure=/etc/machine-id
dbus-uuidgen --ensure
systemctl status network
cat /etc/machine-id
    """
}

def miniMotr(String host) {
    def remote = getTestMachine(host)
    def commandResult = sshCommand remote: remote, command: """
/opt/seagate/cortx/motr/bin/motr_setup post_install --config yaml:///opt/seagate/cortx/motr/conf/motr.post_install.tmpl
/opt/seagate/cortx/motr/bin/motr_setup prepare --config yaml:///opt/seagate/cortx/motr/conf/motr.prepare.tmpl
/opt/seagate/cortx/motr/bin/motr_setup config --config yaml:///opt/seagate/cortx/motr/conf/motr.config.tmpl
    """
}

def miniMotrtest(String host) {
    def remote = getTestMachine(host)
    def commandResult = sshCommand remote: remote, command: """
/opt/seagate/cortx/motr/bin/motr_setup test --config yaml:///opt/seagate/cortx/motr/conf/motr.test.tmpl
dd if=/dev/urandom of=/tmp/128M bs=1M count=128
/opt/seagate/cortx/hare/libexec/m0crate-io-conf > /tmp/m0crate-io.yaml
m0crate -S /tmp/m0crate-io.yaml
    """
}

def miniS3(String host) {
    def remote = getTestMachine(host)
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
}

def miniHare(String host) {
    def remote = getTestMachine(host)
    def commandResult = sshCommand remote: remote, command: """
/opt/seagate/cortx/hare/bin/hare_setup post_install
/opt/seagate/cortx/hare/bin/hare_setup config --config json:///root/provisioner_cluster.json --file '/var/lib/hare/cluster.yaml'
    """
}

def exchangeSSHKey(String host1, String host2, String host3) {
    def remote = getTestMachine(host1)
    def commandResult = sshCommand remote: remote, command: """
cat > ~/deploy_spawn  <<EOL
#!/bin/bash
ssh-keygen -q -t rsa -N '' -f ~/.ssh/id_rsa <<<y 2>&1 >/dev/null
ssh-copy-id -o "StrictHostKeyChecking=no" ${host2}
ssh-copy-id -o "StrictHostKeyChecking=no" ${host3}
EOL
                    """
                    commandResult = sshCommand remote: remote, command: """
cat > ~/deploy_expect  <<EOL
#!/usr/bin/expect -f
set timeout 300
spawn ./deploy_spawn
expect "Password:"
send -- "seagate\\n"
expect "Password:"
send -- "seagate\\n"
interact
EOL
                    """
                    commandResult = sshCommand remote: remote, command: """
yum install -y expect
chmod a+x deploy_spawn
chmod a+x deploy_expect
./deploy_expect
                    """
    
}


