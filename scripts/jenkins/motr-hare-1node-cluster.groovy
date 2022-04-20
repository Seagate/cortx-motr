#!/usr/bin/env groovy
pipeline { 
    agent {
        node {
           label 'ssc-vm-g3-rhev4-1327'
        }
    }
	
    parameters {
        string(name: 'NODE_HOST', defaultValue: '', description: 'Node 1 Host FQDN',  trim: true)
        string(name: 'NODE_USER', defaultValue: '', description: 'Host machine root user',  trim: true)
        string(name: 'NODE_PASS', defaultValue: '', description: 'Host machine root user password',  trim: true)
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
        stage ('Build rpm packages') {
            steps {
                script { build_stage = env.STAGE_NAME }
                sh label: 'to build motr and hare rpm', script: '''
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} hostname
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} git clone https://github.com/Seagate/cortx-motr
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "cd /root/cortx-motr ;  /root/cortx-motr/scripts/build-prep-1node.sh -dev"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "hctl bootstrap --mkfs singlenode.yaml"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "dd if=/dev/urandom of=/tmp/128M bs=1M count=128"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "/opt/seagate/cortx/hare/libexec/m0crate-io-conf > /root/crate.yaml"
                    sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} "/root/cortx-motr/utils/m0crate -S /root/crate.yaml"
                '''
            }
        }
    }
    
    post {
        always {
            script {
                sh label: 'download_log_files', returnStdout: true, script: """ 
                        sshpass -p ${NODE_PASS} ssh -o StrictHostKeyChecking=no ${NODE_USER}@${NODE_HOST} rm -rf /root/cortx-motr
                    """
            }
        }
    }
}
