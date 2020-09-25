/* -*- C -*- */
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


#pragma once

#ifndef __MOTR_LIB_FS_H__
#define __MOTR_LIB_FS_H__

/**
 * Removes directory along with its files.
 * Subdirectories are not allowed.
 *
 * @note returns 0 in case when @dir does not exist.
 * @note works in user-space only for now.
 */
M0_INTERNAL int m0_cleandir(const char *dir);

M0_INTERNAL char *m0_getcwd(void);

#ifndef __KERNEL__
/**
 * Reads file contents into dynamically allocated string.
 *
 * @note If the call succeeds, the user is responsible for freeing
 *       allocated memory with m0_free(*out).
 */
M0_INTERNAL int m0_file_read(const char *path, char **out);
M0_INTERNAL int m0_fdatasync(int fd);
M0_INTERNAL int m0_syncfs(int fd);
#endif

#endif /* __MOTR_LIB_FS_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
