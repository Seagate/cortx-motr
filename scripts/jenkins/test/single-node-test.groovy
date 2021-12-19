pipeline {
    agent none
	options {
		timestamps ()
		timeout(time: 10, unit: 'HOURS')
	}	
 	stages {
		stage("Start test") {
			stages {
				stage('Checkout') {
					when { expression { true } }
					parallel {
						stage('node5') {
							agent {
								node {
									label 'motr-remote-controller-vm-1322'
								}
							}
							environment {
								PULL_ID = "${MOTR_PR}".substring("${MOTR_PR}".lastIndexOf('/') + 1)
								GITHUB_PR_API_URL = "https://api.github.com/repos/seagate/cortx-motr/pulls/${PULL_ID}"
								SOURCE_REPO = getRepoInfo(GITHUB_PR_API_URL, ".head.repo.full_name")  
								REPO_BRANCH = getRepoInfo(GITHUB_PR_API_URL, ".head.ref")
								REPO_URL = "https://github.com/${SOURCE_REPO}"
								SSC_AUTH = credentials("${SSC_AUTH_ID}")
							}																			
							steps {
								script {  
									currentBuild.upstreamBuilds?.each { b -> env.upstream_project = "${b.getProjectName()}";env.upstream_build = "${b.getId()}" }
									TRIGGER = (env.upstream_project != null) ? "${env.upstream_project}/${env.upstream_build}" : currentBuild.getBuildCauses('hudson.model.Cause$UserIdCause')[0].userName
                                    manager.addHtmlBadge("&emsp;<b>Pull Request :</b> <a href=\"${MOTR_PR}\"><b>PR-${PULL_ID}</b></a> <br /> <b>Triggered By :</b> <b>${TRIGGER}</b>")
								}

						        dir('motr') {
									checkout([$class: 'GitSCM', branches: [[name: "*/${REPO_BRANCH}"]], doGenerateSubmoduleConfigurations: false, extensions: [[$class: 'AuthorInChangelog'], [$class: 'SubmoduleOption', disableSubmodules: false, parentCredentials: true, recursiveSubmodules: true, reference: '', trackingSubmodules: false]], submoduleCfg: [], userRemoteConfigs: [[credentialsId: 'cortx-admin-github', url: "${REPO_URL}"]]])
								}
								dir('xperior') {
									git credentialsId: 'cortx-admin-github', url: "https://github.com/Seagate/xperior.git", branch: "main"
								}
								dir('xperior-perl-libs') {
									git credentialsId: 'cortx-admin-github', url: "https://github.com/Seagate/xperior-perl-libs.git", branch: "main"
								}
								dir('seagate-ci') {
									git credentialsId: 'cortx-admin-github', url: "https://github.com/Seagate/seagate-ci", branch: "main"
								}
								dir('seagate-eng-tools') {
									git credentialsId: 'cortx-admin-github', url: "https://github.com/Seagate/seagate-eng-tools.git", branch: "main"
								}
							}
						}
					}
				}
		
				stage('Run Test') {
					when { expression { true } }
				
					parallel {
						stage('node5') {
							agent {
								node {
									label 'motr-remote-controller-vm-1322'
								}
							}
							environment {
								SSC_AUTH = credentials("${SSC_AUTH_ID}")
								TRANSPORT = 'libfabric'					
							}																	
							steps {
								script{
									sh '''
										set -ae
										set
										WD=$(pwd)
										hostname
										id
										ls
										export DO_MOTR_BUILD=yes
										export TESTDIR=motr/.xperior/testds/
										export XPERIOR="${WD}/xperior"
										export ITD="${WD}/seagate-ci/xperior"
										export XPLIB="${WD}/xperior-perl-libs/extlib/lib/perl5"
										export PERL5LIB="${XPERIOR}/mongo/lib:${XPERIOR}/lib:${ITD}/lib:${XPLIB}/"
										export PERL_HOME="/opt/perlbrew/perls/perl-5.22.0/"
										export PATH="${PERL_HOME}/bin/:$PATH:/sbin/:/usr/sbin/"
										export RWORKDIR='motr/motr_test_github_workdir/workdir'
										export IPMIDRV=lan
										export BUILDDIR="/root/${RWORKDIR}"
										export XPEXCLUDELIST=""
										export UPLOADTOBOARD=
										export PRERUN_REBOOT=yes
										export TRANSPORT="${TRANSPORT}"

										${PERL_HOME}/bin/perl "${ITD}/contrib/run_motr_single_test.pl"
									'''
								}
							}
							post {
								always {
									script {
										archiveArtifacts artifacts: 'workdir/**/*.*, build*.*, artifacts/*.*', fingerprint: true, allowEmptyArchive: true
										summary = junit testResults: '**/junit/*.junit', allowEmptyResults : true, testDataPublishers: [[$class: 'AttachmentPublisher']]     
										cleanWs()
									}                            
								}
							}
						} 
					}
				}    	
			}
		}
	}
}

// Get PR info from github api
def getRepoInfo(api, data_path) {

    withCredentials([usernamePassword(credentialsId: "cortx-admin-github", passwordVariable: 'GITHUB_TOKEN', usernameVariable: 'USER')]) {
        result = sh(script: """ curl -s  -H "Authorization: token ${GITHUB_TOKEN}" "${api}" | jq -r "${data_path}" """, returnStdout: true).trim()
    }
    return result 
}
