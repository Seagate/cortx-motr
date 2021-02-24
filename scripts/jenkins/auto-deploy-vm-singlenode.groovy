pipeline {
    agent any

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
						echo "Node 2: ${VM2_FQDN}"
						echo "Target: ${REPO_URL}"
						'''
			}
        }
        
        
        stage('Reset VM') {
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
						sh '''curl "https://raw.githubusercontent.com/Seagate/cortx-motr/EOS-14750/scripts/jenkins/vm-reset" -o vm-reset.sh
						chmod a+x vm-reset.sh
						VERBOSE=true ./vm-reset.sh
						sleep 300
						'''
					}
				}
				stage ('reset-vm-node2'){
				    environment {
                        VM_FQDN = "${VM2_FQDN}"
                    }
                    when { expression { params.RESET_VM } }
					steps {
						sh '''curl "https://raw.githubusercontent.com/Seagate/cortx-motr/EOS-14750/scripts/jenkins/vm-reset" -o vm-reset.sh
						chmod a+x vm-reset.sh
						VERBOSE=true ./vm-reset.sh
						sleep 300
						'''
					}
				}
			}
        }

        stage('cortx-prereqs') {
            parallel {
                stage('cortx-prereqs-node1') {
                    when { expression { params.PRE_REQ } }
                    steps {
                        script {
                            def remote = getTestMachine(VM1_FQDN)
                            def commandResult = sshCommand remote: remote, command: """
                            curl https://raw.githubusercontent.com/Seagate/cortx-prvsnr/cortx-1.0/cli/src/cortx-prereqs.sh -o cortx-prereqs.sh
							chmod a+x cortx-prereqs.sh
							sh cortx-prereqs.sh --disable-sub-mgr
							true
                            """
                            echo "Result: " + commandResult
                        }
                    }
                }
                stage('cortx-prereqs-node2') {
                    when { expression { params.PRE_REQ } }
                    steps {
                        script {
                            def remote = getTestMachine(VM2_FQDN)
                            def commandResult = sshCommand remote: remote, command: """
                            curl https://raw.githubusercontent.com/Seagate/cortx-prvsnr/cortx-1.0/cli/src/cortx-prereqs.sh -o cortx-prereqs.sh
							chmod a+x cortx-prereqs.sh
							sh cortx-prereqs.sh --disable-sub-mgr
							true
                            """
                            echo "Result: " + commandResult
                        }
                    }
                }
            }
        }

        stage('Primary node setup') {
            when { expression { params.PRIMARY_SETUP } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
                        yum install git -y
                        yum install expect -y
						yum install -y python3
						python3 -m venv venv_cortx
						source venv_cortx/bin/activate
						pip3 install -U git+https://github.com/Seagate/cortx-prvsnr@cortx-1.0#subdirectory=api/python
						provisioner --version
                        """
                    echo "Result: " + commandResult
                }
            }
        }
        
        stage('config.ini') {
            when { expression { params.DEPLOY } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                        def commandResult = sshCommand remote: remote, command: """
						cat > ~/config.ini <<EOL
[storage_enclosure]
type=other

[srvnode-1]
hostname=${VM1_FQDN}
network.data_nw.pvt_ip_addr=None
network.data_nw.iface=eth1, eth2
network.mgmt_nw.iface=eth0
bmc.user=None
bmc.secret=None

[srvnode-2]
hostname=${VM2_FQDN}
network.data_nw.pvt_ip_addr=None
network.data_nw.iface=eth1, eth2
network.mgmt_nw.iface=eth0
bmc.secret=None
bmc.user=None
EOL
                        """
                        echo "Result: " + commandResult
                    
                }
            }
        }
        
        stage('Deploy') {
            when { expression { params.DEPLOY } }
            steps {
                script {
						def remote = getTestMachine(VM1_FQDN)
                        def commandResult = sshCommand remote: remote, command: """
cat > ~/deploy_spawn  <<EOL
#!/bin/bash
source venv_cortx/bin/activate
provisioner auto_deploy_vm --console-formatter full --logfile \
  --logfile-filename /var/log/seagate/provisioner/setup.log --source rpm \
  --config-path ~/config.ini --ha \
  --dist-type bundle \
  --target-build ${REPO_URL} \
  srvnode-1:${VM1_FQDN} \
  srvnode-2:${VM2_FQDN}
EOL
                        """
                        echo "Result: " + commandResult
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
                        echo "Result: " + commandResult
                    
                        commandResult = sshCommand remote: remote, command: """
chmod a+x deploy_spawn
chmod a+x deploy_expect
./deploy_expect
hctl status
                        """
                        echo "Result: " + commandResult
                    
                }
            }
        }
        
        stage('hctl status') {
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
                        echo "**********************************************"
                        hctl status
                        echo "**********************************************"
                        cat /etc/hosts
                        echo "**********************************************"
                        """
                }
            }
        }
        
        stage('S3 installation') {
            when { expression { params.S3_INSTALL } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
                    echo INSTALLING S3CLIENT
                    mv /etc/pip.conf /etc/pip.conf.bak
                    salt 'srvnode-1' state.apply components.s3clients
                    mv /etc/pip.conf.bak /etc/pip.conf
                    echo INSTALLING GO
                    yum install -y go
                    echo INSTALLING S3BENCH
                    go get github.com/igneous-systems/s3bench
                    echo FIX HOSTS
                    cp /etc/hosts /etc/hosts.bak
                    S3IP=`ip -4 addr show eth1 |grep inet|cut -d "/" -f1| cut -d " " -f6`
                    grep seagate /etc/hosts |sed -e "s/None/\${S3IP}/g" >> /etc/hosts
                    cat /etc/hosts|sed -e "s/None/127.0.0.1/g" > /etc/hosts.new
                    mv /etc/hosts.new /etc/hosts
                    cat /etc/hosts
                    """
                }
            }
        }
        
        stage('S3 create test account') {
            when { expression { params.S3_CREATE_AC } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
salt-call pillar.get openldap:iam_admin:secret --output=newline_values_only | xargs salt-call lyveutil.decrypt openldap |sed -n '2p' > ldappasswd.dynamic
DYPS=`cat ldappasswd.dynamic`
s3iamcli CreateAccount -n test -e cloud@seagate.com --ldapuser sgiamadmin --ldappasswd \$DYPS > testAc.txt
AWSKEYID=`cat testAc.txt |cut -d ',' -f 4 |cut -d ' ' -f 4`
AWSKEY=`cat testAc.txt |cut -d ',' -f 5 |cut -d ' ' -f 4`
\\cp -f .aws/credentials .aws/credentials.bak
echo "[default]" > .aws/credentials
echo "aws_access_key_id =" \$AWSKEYID >> .aws/credentials
echo "aws_secret_access_key= " \$AWSKEY >> .aws/credentials
cat .aws/credentials
                    """
                }
            }
        }
        
        stage('S3 IO run') {
            when { expression { params.S3_RUN_IO } }
            steps {
                script {
                    def remote = getTestMachine(VM1_FQDN)
                    def commandResult = sshCommand remote: remote, command: """
mkdir s3bench_test
aws s3 mb s3://test-bucket
acc_id=\$(cat ~/.aws/credentials | grep aws_access_key_id | cut -d= -f2)
acc_key=\$(cat ~/.aws/credentials | grep aws_secret_access_key | cut -d= -f2)
num_client=128
num_size=16
num_size_units=M
num_samples=10000
bucket="test-bucket"

case "\$num_size_units" in
        'K')
                size=\$((1024*\$num_size))
                ;;
        'M')
                size=\$((1024*1024*\$num_size))
                ;;
        'G')
                size=\$((1024*1024*1024*\$num_size))
                ;;
esac
/root/go/bin/s3bench -accessKey \$acc_id -accessSecret \$acc_key -bucket \$bucket -endpoint http://s3.seagate.com -numClients \$num_client -numSamples \$num_samples -objectNamePrefix=s3workload -objectSize \$size -verbose
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
