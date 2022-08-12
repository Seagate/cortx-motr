#!/usr/bin/env groovy
pipeline {
    agent {
        node {
           label 'sncr'
        }
    }

    options {
        timeout(time: 180, unit: 'MINUTES')
        timestamps()
        ansiColor('xterm')
        buildDiscarder(logRotator(numToKeepStr: "30"))
    }

     parameters {
        string(name: 'MOTR_REPO', defaultValue: 'https://github.com/Seagate/cortx-motr', description: 'Repo to be used for Motr build.')
        string(name: 'MOTR_BRANCH', defaultValue: 'main', description: 'Branch to be used for Motr build.')
    }

    environment {
        release_tag = "last_successful_prod"
        REPO_NAME = 'cortx-motr'
        GITHUB_TOKEN = credentials('cortx-admin-github') // To clone cortx-motr repo
        GPR_REPO = "https://github.com/${ghprbGhRepository}"
        MOTR_REPO = "${ghprbGhRepository != null ? GPR_REPO : MOTR_REPO}"
        MOTR_BRANCH = "${sha1 != null ? sha1 : MOTR_BRANCH}"
        MOTR_GPR_REFSPEC = "+refs/pull/${ghprbPullId}/*:refs/remotes/origin/pr/${ghprbPullId}/*"
        MOTR_BRANCH_REFSPEC = "+refs/heads/*:refs/remotes/origin/*"
        MOTR_PR_REFSPEC = "${ghprbPullId != null ? MOTR_GPR_REFSPEC : MOTR_BRANCH_REFSPEC}"
        repoURL = "${MOTR_REPO}".replace("github.com", "$GITHUB_TOKEN@github.com")
    }

    stages {
        stage ('Cleanup VM') {
            steps {
                checkout([$class: 'GitSCM', branches: [[name: "${MOTR_BRANCH}"]], doGenerateSubmoduleConfigurations: false, extensions: [[$class: 'CloneOption', depth: 0, noTags: false, reference: '', shallow: false,  timeout: 5], [$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: true, recursiveSubmodules: false, reference: '', trackingSubmodules: false]], submoduleCfg: [], userRemoteConfigs: [[credentialsId: 'cortx-admin-github', url: "${MOTR_REPO}",  name: 'origin', refspec: "${MOTR_PR_REFSPEC}"]]])
                script {
                    sh label: 'Clean up VM before use', script: '''
                        hostname
                        pwd
                        cd "$WORKSPACE"
                        #ln -s "$WORKSPACE" "$WORKSPACE/../cortx-motr"
                        ./scripts/install/usr/libexec/cortx-motr/motr-cleanup || true
                        losetup -D
                        make uninstall || true
                        cd "$WORKSPACE/../cortx-hare" || true
                        make uninstall || true
                        cd "$WORKSPACE/.."
                        rm -rf cortx-hare cortx-motr
                        yum remove cortx-hare cortx-motr{,-devel} cortx-py-utils consul -y
                        rm -rf /var/crash/* /var/log/seagate/* /var/log/hare/* /var/log/motr/* /var/lib/hare/* /var/motr/* /etc/motr/*
                        rm -rf /root/.cache/dhall* /root/rpmbuild
                        rm -rf /etc/yum.repos.d/motr_last_successful.repo /etc/yum.repos.d/motr_uploads.repo /etc/yum.repos.d/lustre_release.repo
                    '''
                }
            }
        }

        stage ('Create Single node cluster') {
            steps {
                script {
                    sh label: 'Download code and install single node cluster', script: '''
                        rm -rf $REPO_NAME
                        echo "MOTR_REPO: ${MOTR_REPO}"
                        git clone "$repoURL" "$REPO_NAME"
                        cd "${REPO_NAME}"
                        git fetch origin "${MOTR_PR_REFSPEC}"
                        git checkout "${MOTR_BRANCH}"
                        git log -1
                        ls -la
                        ./scripts/build-prep-1node.sh -dev
                        hctl bootstrap --mkfs ../singlenode.yaml
                        sleep 60
                        hctl status -d
                        dd if=/dev/urandom of=/tmp/128M bs=1M count=128
                        /opt/seagate/cortx/hare/libexec/m0crate-io-conf > ./crate.yaml
                        ./utils/m0crate -S ./crate.yaml
                    '''
                }
            }
        }
    }

    post {
        always {
            script {
                sh label: 'Clean up work space', returnStdout: true, script: """
                    pwd
                    cd "$WORKSPACE"
                    ./scripts/install/usr/libexec/cortx-motr/motr-cleanup || true
                    losetup -D
                    make uninstall || true
                    cd "$WORKSPACE/../cortx-hare" || true
                    make uninstall || true
                    cd "$WORKSPACE/.."
                    rm -rf cortx-hare cortx-motr
                    echo Done
                    """
                    cleanWs()
            }
        }
    }
}

