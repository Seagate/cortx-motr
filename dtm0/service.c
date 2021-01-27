/* -*- C -*- */
/*
 * Copyright (c) 2011-2020 Seagate Technology LLC and/or its Affiliates
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


#include "lib/errno.h"
#include "lib/memory.h"

#include "reqh/reqh_service.h"
#include "dtm0/fop.h"

static int dtm0_service_start(struct m0_reqh_service *service);
static void dtm0_service_stop(struct m0_reqh_service *service);
static int dtm0_service_allocate(struct m0_reqh_service **service,
				 const struct m0_reqh_service_type *stype);
static void dtm0_service_fini(struct m0_reqh_service *service);


static const struct m0_reqh_service_type_ops dtm0_service_type_ops = {
	.rsto_service_allocate = dtm0_service_allocate
};

static const struct m0_reqh_service_ops dtm0_service_ops = {
	.rso_start = dtm0_service_start,
	.rso_stop  = dtm0_service_stop,
	.rso_fini  = dtm0_service_fini
};

struct m0_reqh_service_type dtm0_service_type = {
	.rst_name  = "M0_CST_DTM0",
	.rst_ops   = &dtm0_service_type_ops,
	.rst_level = M0_RS_LEVEL_LATE,
};

static int _dtm0_alloc(struct m0_reqh_service **service,
		     const struct m0_reqh_service_type *stype,
		     const struct m0_reqh_service_ops *ops)
{
	struct m0_reqh_service *s;

	M0_PRE(stype != NULL && service != NULL && ops != NULL);

	M0_ALLOC_PTR(s);
	if (s == NULL)
		return -ENOMEM;

	s->rs_type = stype;
	s->rs_ops = ops;
	*service = s;

	return 0;
}

static int dtm0_service_allocate(struct m0_reqh_service **service,
				const struct m0_reqh_service_type *stype)
{
	return _dtm0_alloc(service, stype, &dtm0_service_ops);
}

static int dtm0_service_start(struct m0_reqh_service *service)
{
        M0_PRE(service != NULL);
        return m0_dtm0_fop_init();
	/* TODO: Open a dtm0 log and initialise it. */
}

static void dtm0_service_stop(struct m0_reqh_service *service)
{

        M0_PRE(service != NULL);
	m0_dtm0_fop_fini();
}

static void dtm0_service_fini(struct m0_reqh_service *service)
{
	M0_PRE(service != NULL);
        m0_free(service);
}

int m0_dtm0_stype_init(void)
{
	return m0_reqh_service_type_register(&dtm0_service_type);
}

void m0_dtm0_stype_fini(void)
{
	m0_reqh_service_type_unregister(&dtm0_service_type);
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
