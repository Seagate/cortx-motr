pipeline { 

    agent {
        label "jenkins-client-for-cortx-motr"
    }

    parameters {
        string(name: 'NODE_HOST', defaultValue: '', description: 'Node 1 Host FQDN', trim: true)
        string(name: 'NODE_USER', defaultValue: '', description: 'Host machine root user', trim: true)
        string(name: 'NODE_PASS', defaultValue: '', description: 'Host machine root user password', trim: true)
        string(name: 'BRANCH', defaultValue: '', description: 'Branch name', trim: true)
        string(name: 'OPTIONS', defaultValue: '', description: 'Build options', trim: true)
        booleanParam(name: 'UT', defaultValue: true, description: 'Run UT')
        booleanParam(name: 'ST', defaultValue: false, description: 'Run ST')
    }

    options {
        timeout(time: 180, unit: 'MINUTES')
        timestamps()
        ansiColor('xterm') 
        buildDiscarder(logRotator(numToKeepStr: "30"))
    }

    environment {
        NODE_PASS = "${NODE_PASS.isEmpty() ? NODE_DEFAULT_SSH_CRED_PSW : NODE_PASS}"
    }

    stages {
        stage('Checkout') {
            steps {
                checkout([$class: 'GitSCM', branches: [[name: "main"]], doGenerateSubmoduleConfigurations: false, extensions: [[$class: 'PathRestriction', excludedRegions: '', includedRegions: 'scripts/third-party-rpm/.*']], submoduleCfg: [], userRemoteConfigs: [[credentialsId: 'cortx-admin-github', url: "https://github.com/Seagate/cortx-motr"]]])
            }
        }
        stage ('Compile, execute test and report code coverage') {
            steps {
                script { build_stage = env.STAGE_NAME }
                sh label: 'Clean up and build motr and run ut for code coverage', script: '''
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} hostname
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "yum install -y httpd lcov"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "systemctl start httpd.service"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "rm -rf /root/cortx-motr"
                    OLD_BUILD_NUMBER=$(echo "$BUILD_NUMBER - 10" | bc)
                    echo "Old build number is : $OLD_BUILD_NUMBER"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "rm -rf /var/www/html/$OLD_BUILD_NUMBER || true"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/ ; git clone --recursive -b ${BRANCH} https://github.com/Seagate/cortx-motr"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ; sudo ./scripts/install-build-deps"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ; ./autogen.sh"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ; ./configure --enable-coverage --enable-debug ${OPTIONS}"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ; make -j6"

                    if [ ${UT} == true ]
                    then
                       sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ; sudo ./scripts/m0 run-ut"
                    fi

                    if [ ${ST} == true ]
                    then
                        sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ; sudo ./scripts/m0 run-st"
                    fi

                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ; ./scripts/coverage/gcov-gen-html user ./ ./"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "mkdir -p /var/www/html/$BUILD_NUMBER"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cp -r /root/cortx-motr/* /var/www/html/$BUILD_NUMBER/"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "systemctl start firewalld.service"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "firewall-cmd --permanent --add-port=80/tcp"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "firewall-cmd --permanent --add-port=443/tcp"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "firewall-cmd --reload"
                    echo "http://${NODE_HOST}/${BUILD_NUMBER}/"

                '''
            }
        }
    }
}
