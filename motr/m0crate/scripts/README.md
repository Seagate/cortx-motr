## How to take test runs on kvs (index) workload?
We can use m0crate client utility directly or gen_index_yaml_run_workload script to take test runs on kvs
workload.

### 1.  Run kvs workload using m0crate client utility.
You can use m0crate client utility to run single kvs workload.  You can specify .yaml file as input to  m0crate. 
There are kvs .yaml template files available in __motr/m0crate/tests__ .yaml file contains 2 parts MOTR_CONFIG and WORKLOAD_SPEC.
MOTR_CONFIG contains motr cluster details, you would need to update these fields according to your
cluster setup (see hctl status).
WORKLOAD_SPEC contains workload details, you would need to update these fields according to your
kvs workload. 

	pasted cmd for ref:
	[cortx-motr]$ ls motr/m0crate/tests/
	test1_io.yaml  test1.yaml  test2.yaml  test3.yaml  test4.yaml  test5.yaml  test6.yaml
	[root@configs]# m0crate -S m0crate-index.yaml*

### 2. Run kvs workloads using script

motr/m0crate/scripts dir  contains gen_index_yaml_run_workload and m0crate-index.yaml.template. gen_index_yaml_run_workload is a script and m0crate-index.yaml.template is a template file can be used as kvs workload template.

	pasted cmd output for ref:
	[cortx-motr]$ ls motr/m0crate/scripts/
	gen_index_yaml_run_workload  m0crate-index.yaml.template  README.md*

__gen_index_yaml_run_workload__ script can be used to generate and run workload in one go.
gen_index_yaml_run_workload uses m0crate-index.yaml.template to generate workload 
.yaml files on the basis of key_sizes and value_sizes provided in the script.

__m0crate-index.yaml.template__ is used by above script as template, so before starting executing it
make sure you have updated all the fields in yaml template file based on your requirements.
	for example:
	MOTR_LOCAL_ADDR, MOTR_HA_ADDR, PROF, PROCESS_FID these are the fields which are
	related to you motr cluster setup (see cmd: hctl status)
	you probably may need to update kvs workload spec fields too.
	for example:
	NUM_KVP, NXRECORDS, OP_COUNT, KEY_ORDER etc.

__Script execution steps:__
1. Edit __m0crate-index.yaml.template__ file.
   update template workload fields according to your requirement,
   update __motr cluster conf__ and __workload spec__ fields in the template.
2. Edit __gen_index_yaml_run_workload script__ file.
   update __key_size__, __value_size__ and __start_of_indexfid__ values in the script.
3. Execute __./gen_index_yaml_run_workload__
4. After successful execution check execution logs and generated .yaml 
	conf file in __workload_logs__ dir. workload_logs will have timestamp based directories.

		pasted cmd output for ref:
		[temp]# ls workload_logs/
		010321-204231  010321-204704
		[temp]# ls workload_logs/010321-204704
		configs  output
		[temp]# ls workload_logs/010321-204704/output
		kv_run_output.csv  test_run.log
