# m0crate kvs testing
```
motr/m0crate dir contains scripts dir, which contains gen_index_yaml_run_workload
and m0crate-index.yaml.template.
gen_index_yaml_run_workload is a script and m0crate-index.yaml.template is a template
file can be used for kvs testing.

pasted cmd output for ref:
[cortx-motr]$ ls motr/m0crate/scripts/
gen_index_yaml_run_workload  m0crate-index.yaml.template  README.md
```
## gen\_index\_yaml\_run\_workload
```
gen_index_yaml_run_workload script can be used to generate and run workload in one go.
gen_index_yaml_run_workload uses m0crate-index.yaml.template to generate workload
.yaml files on the basis of key_sizes and value_sizes provided in the script.
```
## m0crate-index.yaml.template
```
m0crate-index.yaml.template is used by above script as template, so before starting executing it
makesure you have updated all the fields in yaml template file based on your requirements.
for example:
        MOTR_LOCAL_ADDR, MOTR_HA_ADDR, PROF, PROCESS_FID these are the fields which are
        related to you cluster setup (see cmd: hctl status)
        you probably may need to update kvs workload fieds too.
        for example:
                NUM_KVP, NXRECORDS, OP_COUNT, KEY_ORDER etc.
```
## Execution steps
```
1. Edit m0crate-index.yaml.template file.
   update yaml template fields according to your requirement,
   update cluster and workload conf fields.
2. Edit gen_index_yaml_run_workload script file.
   update key_size, value_size and start_of_indexfid values.
3. Execute ./gen_index_yaml_run_workload
4. After successful execution check execution logs and generated .yaml
	conf file in workload_logs dir. workload_logs will have timestamp based directories.

	pasted cmd output for ref:
	[temp]# ls workload_logs/
	010321-204231  010321-204704
	[temp]# ls workload_logs/010321-204704
	configs  output
	[temp]# ls workload_logs/010321-204704/output
	kv_run_output.csv  test_run.log
```
