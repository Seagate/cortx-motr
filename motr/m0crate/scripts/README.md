## Steps to take test runs on kvs (index) workload
We can use __m0crate__ client utility directly or __gen_index_yaml_run_workload__ script to take test runs on kvs workload.

### 1.  Run kvs workload using m0crate client utility

You can use m0crate client utility to run single kvs workload.  You can specify .yaml file as input to  m0crate.
There are kvs .yaml template files available in __motr/m0crate/tests__ .yaml file contains 2 parts MOTR_CONFIG and WORKLOAD_SPEC.

*   MOTR_CONFIG contains motr cluster details, You would need to update these fields according to your cluster setup (see hctl status).
*   WORKLOAD_SPEC contains workload details, You would need to update these fields according to your kvs workload requirement.

```shell
[cortx-motr]$ ls motr/m0crate/tests/
test1_io.yaml  test1.yaml  test2.yaml  test3.yaml  test4.yaml  test5.yaml  test6.yaml
[root@configs]# m0crate -S m0crate-index.yaml
```

### 2. Run kvs workloads using script
motr/m0crate/scripts dir contains __gen_index_yaml_run_workload__ and __m0crate-index.yaml.template__. __gen_index_yaml_run_workload__ is a script and __m0crate-index.yaml.template__ is a template file can be used as kvs workload template.

```shell
[cortx-motr]$ ls motr/m0crate/scripts/
gen_index_yaml_run_workload  m0crate-index.yaml.template  README.md
```

__gen_index_yaml_run_workload__ script can be used to generate and run workload in one go. Script __gen_index_yaml_run_workload__ uses __m0crate-index.yaml.template__ to generate workload  .yaml files on the basis of __key_sizes__ and __value_sizes__ provided in the script.
__m0crate-index.yaml.template__ is used by above script as template, so before starting executing it make sure you have updated all the fields in yaml template file based on your requirements.

*   MOTR_LOCAL_ADDR, MOTR_HA_ADDR, PROF, PROCESS_FID these are the fields which are related to you motr cluster setup (see cmd: hctl status)
*   You probably may need to update kvs workload spec fields too, NUM_KVP, NXRECORDS, OP_COUNT, KEY_ORDER etc.

__Script execution steps__

1.  Edit __m0crate-index.yaml.template__ file, Update __MOTR_CONFIG__ and __WORKLOAD_SPEC__ fields in the template.

```shell
vim m0crate-index.yaml.template
```

2.  Edit __gen_index_yaml_run_workload__ script file, Update __key_size__, __value_size__ and __start_of_indexfid__ values in the script.

```shell
vim gen_index_yaml_run_workload
```

3.  Execute __gen_index_yaml_run_workload__ script

```shell
# ~/cortx-motr/motr/m0crate/scripts/gen_index_yaml_run_workload
```

4.  After successful execution check execution logs and generated .yaml conf file in __workload_logs__ dir. Directory __workload_logs__ will have timestamp based directories.

```shell
[temp]# ls workload_logs/
010321-204231  010321-204704
[temp]# ls workload_logs/010321-204704
configs  output
[temp]# ls workload_logs/010321-204704/output
kv_run_output.csv  test_run.log
```
