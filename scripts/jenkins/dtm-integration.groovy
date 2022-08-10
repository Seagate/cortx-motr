#!/usr/bin/env groovy
pipeline {
    agent {
        node {
           label 'dtm-integration'
        }
    }

    parameters {
        string(name: 'MOTR_BRANCH', defaultValue: 'main', description: 'Motr branch name', trim: true)
        string(name: 'HARE_BRANCH', defaultValue: 'main', description: 'Hare branch name', trim: true)
        string(name: 'MOTR_REPO', defaultValue: 'https://github.com/Seagate/cortx-motr', description: 'Motr Repository URL', trim: true)
        string(name: 'HARE_REPO', defaultValue: 'https://github.com/Seagate/cortx-hare', description: 'Hare Repository URL', trim: true)
    }

    options {
        timeout(time: 40, unit: 'MINUTES')
        timestamps()
        ansiColor('xterm')
        buildDiscarder(logRotator(numToKeepStr: "30"))
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
                script {
                    sh label: 'Clean up VM before use', script: '''
                        hostname
                        pwd
                        cd "$WORKSPACE"
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

        stage ('Compile motr and hare') {
            steps {
                script { build_stage = env.STAGE_NAME }
                sh label: 'Clean up and build motr and run ut for code coverage', script: '''
                    hostname
                    pkill -9 m0d || true
                    pkill -9 mkfs || true
                    pkill -9 m0 || true
                    hctl shutdown ||  true
                    rm -rf "$WORKSPACE"/cortx-motr "$WORKSPACE"/cortx-hare "$WORKSPACE"/cortx-utils /var/motr/*
                    cd "$WORKSPACE"
                    git clone --recursive ${MOTR_REPO} cortx-motr && cd cortx-motr && git checkout ${MOTR_BRANCH}
                    cd "$WORKSPACE"/cortx-motr
                    sudo ./scripts/install-build-deps
                    ./autogen.sh
                    ./configure --enable-dtm0 --enable-debug
                    make -j6
                    scripts/install-motr-service --link
                    cd "$WORKSPACE"/
                    git clone --recursive -b ${HARE_BRANCH} ${HARE_REPO}
                    yum -y install python3 python3-devel yum-utils
                    yum-config-manager --add-repo https://rpm.releases.hashicorp.com/RHEL/hashicorp.repo
                    yum -y install consul-1.9.1
                    #yum install -y gcc rpm-build python36 python36-pip python36-devel python36-setuptools openssl-devel libffi-devel python36-dbus  # for centos7.9
                    yum install -y gcc rpm-build python38 python38-pip python38-devel python38-setuptools openssl-devel libffi-devel python3-dbus
                    cd "$WORKSPACE"/
                    git clone --recursive https://github.com/Seagate/cortx-utils -b main
                    cd "$WORKSPACE"/cortx-utils
                    ./jenkins/build.sh -v 2.0.0 -b 2
                    # yum install -y gcc python36 python36-pip python36-devel python36-setuptools openssl-devel libffi-devel python36-dbus # for centos7.9
                    yum install -y gcc python38 python38-pip python38-devel python38-setuptools openssl-devel libffi-devel python3-dbus
                    sudo pip3 install -r https://raw.githubusercontent.com/Seagate/cortx-utils/main/py-utils/python_requirements.txt
                    sudo pip3 install -r https://raw.githubusercontent.com/Seagate/cortx-utils/main/py-utils/python_requirements.ext.txt
                    cd "$WORKSPACE"/cortx-utils/py-utils/dist
                    yum localinstall -y cortx-py-utils-*.noarch.rpm
                    cd "$WORKSPACE"/cortx-hare
                    make uninstall && make clean && make distclean
                    make -j6 && make devinstall
                '''
            }
        }
        stage ('execute dtm integration test') {
            steps {
                sh '''
                    HOST=$(hostname)
                    sed -i "s/localhost/$HOST/g" "$WORKSPACE"/cortx-motr/dtm0/it/all2all/cdf.yaml
                    cd "$WORKSPACE"/cortx-motr/dtm0/it/all2all ; ./all2all rec
                    cd "$WORKSPACE"/cortx-motr ; sudo ./utils/m0run -- m0ut -t dtm0-ut -n 2
                '''
            }

            post {
                success {
                    script {
                        sh label: 'Clean up workspace', script: '''
                            pkill -9 m0d || true
                            pkill -9 mkfs || true
                            pkill -9 m0 || true
                            hctl shutdown ||  true
                            cd "$WORKSPACE"/cortx-hare ; make uninstall && make clean && make distclean
                            cd "$WORKSPACE"/cortx-motr ; make uninstall && make clean && make distclean
                            rm -rf "$WORKSPACE"/cortx-motr
                            rm -rf "$WORKSPACE"/cortx-hare
                            rm -rf "$WORKSPACE"/cortx-utils
                            '''
                            cleanWs()
                            }
                        }
                }
        }
    }
}
