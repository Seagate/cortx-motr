#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#

prepare_datafiles_and_objects()
{
	local rc=0

	echo "creating the source file"
	dd if=/dev/urandom bs=$src_bs count=$src_count \
	   of="$MOTR_M0T1FS_TEST_DIR/srcfile" || return $?

	for ((i=0; i < ${#file[*]}; i++)) ; do
		local lid=${unit2id_map[${unit_size[$i]}]}
		local us=$((${unit_size[$i]} * 1024))

		MOTR_PARAM="-l ${lnet_nid}:$SNS_MOTR_CLI_EP  \
			    -H ${lnet_nid}:$HA_EP -p '$PROF_OPT' \
			    -P '$M0T1FS_PROC_ID' -L ${lid} -s ${us} "

		echo "creating object ${file[$i]} bs=${us} * c=${file_size[$i]}"
		dd bs=${us} count=${file_size[$i]}            \
		   if="$MOTR_M0T1FS_TEST_DIR/srcfile"         \
		   of="$MOTR_M0T1FS_TEST_DIR/src${file[$i]}"

		run "$M0_SRC_DIR/motr/st/utils/m0cp" ${MOTR_PARAM}  \
						-c ${file_size[$i]} \
						-o ${file[$i]}      \
				"$MOTR_M0T1FS_TEST_DIR/srcfile" || {
			rc=$?
			echo "Writing object ${file[$i]} failed"
		}
	done
	return $rc
}

motr_read_verify()
{
	local start_file=$1
	local rc=0

	for ((i=$start_file; i < ${#file[*]}; i++)) ; do
		local lid=${unit2id_map[${unit_size[$i]}]}
		local us=$((${unit_size[$i]} * 1024))

		MOTR_PARAM="-l ${lnet_nid}:$SNS_MOTR_CLI_EP       \
			      -H ${lnet_nid}:$HA_EP -p '$PROF_OPT' \
			      -P '$M0T1FS_PROC_ID' -L ${lid} -s ${us} "

		echo "Reading object ${file[$i]} ... and diff ..."
		rm -f "$MOTR_M0T1FS_TEST_DIR/${file[$i]}"
		run "$M0_SRC_DIR/motr/st/utils/m0cat" ${MOTR_PARAM}  \
						 -c ${file_size[$i]} \
						 -o ${file[$i]}      \
				"$MOTR_M0T1FS_TEST_DIR/${file[$i]}" || {
			rc=$?
			echo "reading ${file[$i]} failed"
		}

		diff "$MOTR_M0T1FS_TEST_DIR/${file[$i]}" \
		     "$MOTR_M0T1FS_TEST_DIR/src${file[$i]}" || {
			rc=$?
			echo "comparing ${file[$i]} with src${file[$i]} failed"
		}
	done

	if [[ $rc -eq 0 ]] ; then
		echo "file verification success"
	else
		echo "file verification failed"
	fi

	return $rc
}

motr_delete_objects()
{
	for ((i=0; i < 7; i++)) ; do
		echo "Deleting object ${file[$i]} "
		local lid=${unit2id_map[${unit_size[$i]}]}
		local us=$((${unit_size[$i]} * 1024))

		MOTR_PARAM="-l ${lnet_nid}:$SNS_MOTR_CLI_EP  \
			      -H ${lnet_nid}:$HA_EP -p $PROF_OPT \
			      -P $M0T1FS_PROC_ID -L ${lid} -s ${us} "

		$M0_SRC_DIR/motr/st/utils/m0unlink ${MOTR_PARAM} \
						     -o ${file[$i]} || return $?

### Make sure the object is really deleted, and reading will get -ENOENT.
		$M0_SRC_DIR/motr/st/utils/m0cat ${MOTR_PARAM}     \
						  -c ${file_size[$i]} \
						  -o ${file[$i]}      \
				  "$MOTR_M0T1FS_TEST_DIR/${file[$i]}" \
						  2>/dev/null && return -1
	done

	echo "objects deleted"
	return 0
}
