# Perfline
This is perfline module that allows to run cortx-motr workloads. 
It consists of three modules:
- perfline - main functionality, report generator, statistic gather scripts
- webui - web server for presenting workload result and controlling over new workloads
- ansible_setup - ansible script for fast perfline setup

# Prerequisites
For automated deploying perfline on a cluster, following prerequisites must be satisfied:
1. Actual version of cortx-motr repo downloaded
2. Ansible package installed (ver. 2.9+)
3. Machine, from which installation will be performed, should have ssh access to cluster nodes by their dns-name/ip (ssh-copy-id).
4. Setup config for cluster nodes  
    4.1 Change ./ansible_setup/hosts file and set target cluster nodes dns-name/ip. Accordingly for master and secondary nodes.  
    `[master_node]`  
    `target1.seagate.com`  
    `[secondary_node]`  
    `target2.seagate.com`  
    `...`  
    4.2 Change ./perfline/sys/config.py and set target nodes names and ha_type.   
    `...`  
    `nodes = 'target1.seagate.com,target2.seagate.com'`  
    `ha_type = 'hare'`  
    `...`  
    For VMs configuration `ha_type = hare`  
    For cluster configuration `ha_type = pcs`  

# Installation
When prerequisites is satisfied you need to `# cd` to an ansible_setup directory and run:  
`# ansible-playbook -i hosts setup.yaml`  
After that wait till ansible will copy and install all required artifacts on target nodes.  

# Starting workload
Workload can be started only from master_node machine  
For starting workload on a cluster you need to run tasks, which describing amount/size of files you want to upload/download and how many clients will perform load.  
You can find examples of such tasks at ./perfline/examples  
After you choose/wrote task description, you need to `# cd /root/perfline/perfline` and than run task with:  
`# python3 perfline.py  -a < examples/s3bench.mkfs.yaml`  
or  
`# python3 perfline.py  -a < examples/your_name.yaml`  

# Additional info
# Webui
If ansible script finish it's job successfully, you should be able to access webui page from browser at 'taget_master_ip:8005'.
- At the main page you should see 4 graphs, that displays results of performance loads for different file sizes.
- At Queue page you can see current running tasks and short info about them 
- At Results page you can see all finished tasks, read/write results, it's statuses and links to their artifacts and report page.  
If you go to report page you could see detailed report for executed task, including hardware, network, block devices, statistic sctipts infos.  

##### 8005 is a default port. If you want to change it, you should do it before perfline installation with these steps:  
- in ./ansible_setup/setup.yaml find task named 'Open port for webui' and change mentioned port for desired:  
    `...`  
    ` - name: Open port for webui`  
    `firewalld: port='new_port'/tcp zone=public permanent=true state=enabled immediate=yes`  
    `...`  
- in ./webui/config.py change `server_ep` variable for port:  
    `...`  
    `server_ep = {'host': '0.0.0.0', 'port': 'new_port'}`  

