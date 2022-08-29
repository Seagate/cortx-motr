pipeline {

    agent {
        label "code-coverage"
    }

    parameters {
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
        TestList = "/tmp/list-of-unit-tests.txt"
    }

    stages {
        stage ('Compile, execute test and report code coverage') {
            steps {
                script { build_stage = env.STAGE_NAME }
                sh label: 'Clean up, build motr, run ut/st and generate code coverage report', script: '''
                    hostname
                    pwd
                    yum install -y httpd lcov
                    systemctl start httpd.service
                    make uninstall || true
                    make clean || true 
                    rm -rf cortx-motr
                    OLD_BUILD_NUMBER=$(echo "$BUILD_NUMBER - 10" | bc)
                    echo "Old build number is : $OLD_BUILD_NUMBER"
                    rm -rf /var/www/html/$OLD_BUILD_NUMBER || true
                    git clone --recursive -b ${BRANCH} https://github.com/Seagate/cortx-motr  cortx-motr
                    cd cortx-motr
                    sudo ./scripts/install-build-deps
                    ./autogen.sh
                    ./configure --enable-coverage --enable-debug ${OPTIONS}
                    make -j6

                    if [ ${UT} == true ]
                    then
                       ./scripts/m0 run-ut -l > "$TestList"
                       set +o errexit
                       while read Test
                       do
                               echo "Executing: $Test"
                               timeout --preserve-status -k 600s 600s ./scripts/m0 run-ut -t $Test 
                               echo "Test $Test Executed and exited with return code $?"
                       done < "$TestList"
                    fi

                    if [ ${ST} == true ]
                    then
                       ./scripts/m0 run-ut -l > "$TestList"
                       set +o errexit
                       while read Test
                       do
                               echo "Executing: $Test"
                               timeout --preserve-status -k 600s 600s ./scripts/m0 run-st -t $Test 
                               echo "Test $Test Executed and exited with return code $?"
                       done < "$TestList"
                    fi

                    ./scripts/coverage/gcov-gen-html user ./ ./
                    mkdir -p /var/www/html/$BUILD_NUMBER
                    cp -r * /var/www/html/$BUILD_NUMBER/
                    systemctl start firewalld.service
                    firewall-cmd --permanent --add-port=80/tcp
                    firewall-cmd --permanent --add-port=443/tcp
                    firewall-cmd --reload
                    echo "http://$(hostname)/${BUILD_NUMBER}/"

                '''
            }
        }
    }
}

