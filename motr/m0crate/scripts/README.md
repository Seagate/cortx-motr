
===============
m0crate kvs
===============

- motr/m0crate dir contains script dir, which contains gen_index_yaml_run_workload
- and m0crate-index.yaml.template.

- gen_index_yaml_run_workload can be used to generate and run workload in one go.
- gen_index_yaml_run_workload uses m0crate-index.yaml.template to generate workload .yaml files
- on the basis of key_sizes and value_sizes provided in the script.  

- m0crate-index.yaml.template is used by above script as template, so before starting executing it
- makesure you have updated all the fields in yaml template file based on your requirements.
- for example:
-	MOTR_LOCAL_ADDR, MOTR_HA_ADDR, PROF, PROCESS_FID these are the fields which are 
-	related to you cluster setup (see cmd: hctl status)
-       you probably may need to update kvs workload fieds too.
- 	 			
