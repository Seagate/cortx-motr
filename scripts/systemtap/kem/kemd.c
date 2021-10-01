/* -*- C -*- */
/*
 * Copyright (c) 2019-2020 Seagate Technology LLC and/or its Affiliates
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/smp.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>

#include "scripts/systemtap/kem/kem.h"
#include "scripts/systemtap/kem/kem_dev.h"

/**
 * @addtogroup kem_dev KEM Kernel-space module
 *
 * @{
 */

#define NUM(dev) (MINOR(dev) & 0xf)

static int kemd_major = KEMD_MAJOR;
static int kemd_minor = KEMD_MINOR;
static int kemd_nr_cpus;

struct kem_rb *kemd_rbs;

static struct kem_dev *kemd_devices;

static int kemd_init_module(void);
static void kemd_cleanup_module(void);
static int kemd_open(struct inode *inode, struct file *filp);
static int kemd_release(struct inode *inode, struct file *filp);
static ssize_t kemd_read(struct file *filp, char *buf,
			 size_t len, loff_t *offset);
static ssize_t kemd_write(struct file *filp, const char *buf,
			  size_t len, loff_t *offset);

static struct file_operations kemd_fops = {
	.owner   = THIS_MODULE,
	.read    = kemd_read,
	.write   = kemd_write,
	.open    = kemd_open,
	.release = kemd_release
};

static void kemd_dev_destroy(int i)
{
	cdev_del(&kemd_devices[i].kd_cdev);
}

static int kemd_dev_create(int i)
{
	dev_t devno;

	devno = MKDEV(kemd_major, kemd_minor + i);
	cdev_init(&kemd_devices[i].kd_cdev, &kemd_fops);
	kemd_devices[i].kd_cdev.owner = THIS_MODULE;
	kemd_devices[i].kd_cdev.ops = &kemd_fops;
	kemd_devices[i].kd_num = i;
	kemd_devices[i].kd_rb = &kemd_rbs[i];
	atomic_set(&kemd_devices[i].kd_busy, 0);

	return cdev_add(&kemd_devices[i].kd_cdev, devno, 1);
}

static int kemd_devs_create(void)
{
	int err = 0;
	int i;

	for (i = 0; i < kemd_nr_cpus; i++) {
		err = kemd_dev_create(i);
		if (err < 0)
			break;
	}

	if (err < 0) {
		printk(KERN_WARNING "Can't create cdev for kemd%d.\n", i);
		for (i -= 1; i > 0; i--)
			kemd_dev_destroy(i);
	}

	return err;
}

static void kemd_devs_destroy(void)
{
	int i;

	for (i = 0; i < kemd_nr_cpus; i++) {
		kemd_dev_destroy(i);
	}
}

void kemd_rbs_free(struct kem_rb *rbs)
{
	int i;

	for (i = 0; i < kemd_nr_cpus; i++) {
		kfree(rbs[i].kr_buf);
	}
}

int kemd_rbs_init(struct kem_rb *rbs)
{
	struct ke_msg *ptr;
	int            i;
	int            j;

	for (i = 0; i < kemd_nr_cpus; i++) {
		ptr = kzalloc(KEMD_BUFFER_SIZE * sizeof(struct ke_msg),
				GFP_KERNEL);
		if (ptr == NULL)
			goto err;
		rbs[i].kr_buf  = ptr;
		rbs[i].kr_size = KEMD_BUFFER_SIZE;
	}
	return 0;
err:
	printk(KERN_WARNING "Can't allocate buffers %dth obj.\n", i);
	for (j = 0; j < i; j++)
		kfree(rbs[j].kr_buf);
	return -ENOMEM;
}

static int kemd_init_module(void)
{
	dev_t devno;
	int   err = 0;

	kemd_nr_cpus = num_online_cpus();

	printk(KERN_INFO "Number of CPUs: %d.\n", kemd_nr_cpus);

	kemd_rbs = kzalloc(kemd_nr_cpus * sizeof(*kemd_rbs), GFP_KERNEL);
	if (kemd_rbs == NULL) {
		printk(KERN_WARNING "Can't allocate kemd_rbs.\n");
		return -ENOMEM;
	}
	err = kemd_rbs_init(kemd_rbs);
	if(err != 0) {
		printk(KERN_WARNING "Can't allocate kemd_rbs.\n");
		goto out_rbs;
	}
	kemd_devices = kzalloc(kemd_nr_cpus * sizeof(*kemd_devices),
			       GFP_KERNEL);
	if (kemd_devices == NULL) {
		printk(KERN_WARNING "Can't allocate mem for kemd_devices.\n");
		goto out_rbs_inited;
	}

	if (kemd_major) {
		devno = MKDEV(kemd_major, kemd_minor);
		err = register_chrdev_region(devno, kemd_nr_cpus,
					     KEMD_DEV_NAME);
	} else {
		err = alloc_chrdev_region(&devno, kemd_minor,
					  kemd_nr_cpus, KEMD_DEV_NAME);
		kemd_major = MAJOR(devno);
	}

	printk(KERN_INFO "Init devno: %d.\n", devno);

	if (err < 0) {
		printk(KERN_WARNING "Can't get major %d.\n", kemd_major);
		goto out_devs;
	}

	err = kemd_devs_create();
	if (err < 0) {
		printk(KERN_WARNING "Can't init kemd chrdevs.\n");
		goto out_chrdev;
	}

	printk(KERN_INFO KEMD_DEV_NAME " major %d inited.\n", kemd_major);

	return 0;

 out_chrdev:
	unregister_chrdev_region(devno, kemd_nr_cpus);
 out_devs:
	kfree(kemd_devices);
 out_rbs_inited:
	kemd_rbs_free(kemd_rbs);
 out_rbs:
	kfree(kemd_rbs);

	return err;
}

static void kemd_cleanup_module(void)
{
	dev_t devno;

	devno = MKDEV(kemd_major, kemd_minor);

	printk(KERN_INFO "kemd cleanup.\n");

	kemd_devs_destroy();
	printk(KERN_INFO "devs destroyed.\n");

	unregister_chrdev_region(devno, kemd_nr_cpus);
	printk(KERN_INFO "chrdev unreg.\n");

	kfree(kemd_devices);
	printk(KERN_INFO "kfree kemd_devices.\n");

	kemd_rbs_free(kemd_rbs);
	kfree(kemd_rbs);
	printk(KERN_INFO "kfree kemd_rbs.\n");
}

static int kemd_open(struct inode *inode, struct file *filp)
{
	int             num;
	struct kem_dev *kemd_dev;

	num = NUM(inode->i_rdev);
	kemd_dev = &kemd_devices[num];

	if (atomic_read(&kemd_dev->kd_busy) != 0)
		return -EBUSY;

	filp->private_data = &kemd_devices[num];
	atomic_inc(&kemd_dev->kd_busy);

	return 0;
}

static int kemd_release(struct inode *inode, struct file *filp)
{
	int num;
	struct kem_dev *kemd_dev;

	num = NUM(inode->i_rdev);
	kemd_dev = &kemd_devices[num];
	atomic_dec(&kemd_dev->kd_busy);

	return 0;
}

static ssize_t kemd_read(struct file *filp, char *buf,
                         size_t len, loff_t *offset)
{
	int             bytes_read = 0;
	struct kem_dev *kemd_dev;
	struct kem_rb  *rb;
	struct ke_msg  *ent;
	unsigned int    nr_ents;
	unsigned int    nr_written;
	int             i;
	int             err;

	kemd_dev = (struct kem_dev *)filp->private_data;
	rb = kemd_dev->kd_rb;

	nr_ents = len / sizeof(*ent);

	nr_written = atomic_read(&rb->kr_written);
	if (nr_ents > nr_written)
		nr_ents = nr_written;

	if (nr_ents > KEMD_READ_PORTION)
		nr_ents = KEMD_READ_PORTION;

	for (i = 0; i < nr_ents; i++) {
		ent = &rb->kr_buf[rb->kr_read_idx];
		err = copy_to_user(buf, ent, sizeof(*ent));
		if (err) {
			bytes_read = -EFAULT;
			break;
		}

		bytes_read += sizeof(*ent);

		rb->kr_read_idx = (rb->kr_read_idx + 1) % KEMD_BUFFER_SIZE;
		atomic_dec(&rb->kr_written);
		nr_written--;
	}

	return bytes_read;
}

static ssize_t kemd_write(struct file *filp, const char *buf,
                          size_t len, loff_t *off)
{
	return -EINVAL;
}

EXPORT_SYMBOL(kemd_rbs);

module_init(kemd_init_module);
module_exit(kemd_cleanup_module);

/** @} end of kem_dev group */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
