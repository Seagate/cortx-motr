/*
 * Copyright (c) 2014-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */

static struct m0_semaphore sem;

static int reqhut_fom_create(struct m0_fop *fop, struct m0_fom **out,
			     struct m0_reqh *reqh);
static int reqhut_fom_tick(struct m0_fom *fom);
static void reqhut_fom_fini(struct m0_fom *fom);
static size_t reqhut_find_fom_home_locality(const struct m0_fom *fom);

static const struct m0_fom_ops reqhut_fom_ops = {
	.fo_fini = reqhut_fom_fini,
	.fo_tick = reqhut_fom_tick,
	.fo_home_locality = reqhut_find_fom_home_locality
};

static const struct m0_fom_type_ops reqhut_fom_type_ops = {
	.fto_create = reqhut_fom_create,
};

static int reqhut_fom_create(struct m0_fop  *fop,
			     struct m0_fom **out,
			     struct m0_reqh *reqh)
{
	struct m0_fom *fom;

	M0_PRE(fop != NULL);
	M0_PRE(out != NULL);

	M0_ALLOC_PTR(fom);
	if (fom == NULL)
		return -ENOMEM;

	m0_fom_init(fom, &fop->f_type->ft_fom_type, &reqhut_fom_ops,
		    fop, NULL, reqh);

	*out = fom;

	return 0;
}

static size_t reqhut_find_fom_home_locality(const struct m0_fom *fom)
{
	return m0_fop_opcode(fom->fo_fop);
}


static int reqhut_fom_tick(struct m0_fom *fom)
{
	m0_semaphore_up(&sem);
	m0_fom_phase_set(fom, M0_FOPH_FINISH);
	return M0_FSO_WAIT;
}

static void reqhut_fom_fini(struct m0_fom *fom)
{
	m0_fom_fini(fom);
	m0_free(fom);
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
