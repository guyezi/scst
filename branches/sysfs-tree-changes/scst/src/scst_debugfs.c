/*
 *  scst_debugfs.c
 *
 *  Copyright (C) 2010 Bart Van Assche <bvanassche@acm.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/gfp.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#ifdef INSIDE_KERNEL_TREE
#include <scst/scst.h>
#include <scst/scst_debug.h>
#else
#include "scst.h"
#include "scst_debug.h"
#endif
#include "scst_mem.h"
#include "scst_debugfs.h"
#include "scst_priv.h"

#if !defined(CONFIG_SCST_PROC) && defined(CONFIG_DEBUG_FS)

static struct dentry *scst_root_debugfs_dir;
static struct dentry *scst_main_debugfs_dir;
static struct dentry *scst_tgtt_debugfs_dir;
static struct dentry *scst_tgt_debugfs_dir;
static struct dentry *scst_devt_debugfs_dir;
static struct dentry *scst_dev_debugfs_dir;

static int scst_debugfs_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static loff_t scst_debugfs_seek(struct file *file, loff_t offset, int origin)
{
	loff_t res = -EINVAL;

	switch (origin) {
	case 1:
		/* Relative to current position. */
		offset += file->f_pos;
		/* Fall-through. */
	case 0:
		/* Relative to start of file. */
		if (offset >= 0)
			res = file->f_pos = offset;
		break;
	default:
		WARN(true, "origin = %d", origin);
	}
	return res;
}

static ssize_t scst_debugfs_read_file(struct file *file, char __user *buf,
				      size_t count, loff_t *ppos)
{
	struct scst_debugfs_file *debugfs_file = file->private_data;
	const struct scst_debugfs_attr *attr = debugfs_file->attr;
	void *data = debugfs_file->data;
	unsigned long pg;
	void *contents;
	int len;
	ssize_t res;

	res = -EINVAL;
	if (!attr->show || *ppos >= PAGE_SIZE)
		goto out;
	res = -ENOMEM;
	pg = __get_free_page(GFP_KERNEL);
	if (!pg)
		goto out;
	contents = (void *)pg;
	len = (attr->show)(data, attr, contents);
	res = min_t(ssize_t, count, len - *ppos);
	if (res < 0)
		goto out_free;
	if (copy_to_user(buf, contents + *ppos, res))
		res = -EFAULT;
out_free:
	free_page(pg);
	if (res > 0)
		*ppos += res;
out:
	return res;
}

static ssize_t scst_debugfs_write_file(struct file *file,
				       const char __user *buf,
				       size_t count, loff_t *ppos)
{
	struct scst_debugfs_file *debugfs_file = file->private_data;
	const struct scst_debugfs_attr *attr = debugfs_file->attr;
	void *data = debugfs_file->data;
	unsigned long pg;
	void *contents;
	ssize_t res;

	res = -EINVAL;
	if (!attr->store || *ppos != 0)
		goto out;
	res = -ENOMEM;
	pg = __get_free_page(GFP_KERNEL);
	if (!pg)
		goto out;
	contents = (void *)pg;
	if (copy_from_user(contents, buf, count) == 0)
		res = (attr->store)(data, attr, contents, count);
	else
		res = -EFAULT;
	free_page(pg);
	if (res > 0)
		*ppos += res;
out:
	return res;
}

static const struct file_operations scst_debugfs_fops = {
	.read	=	scst_debugfs_read_file,
	.write	=	scst_debugfs_write_file,
	.open	=	scst_debugfs_open,
	.llseek	=	scst_debugfs_seek,
};

/**
 * scst_debugfs_create_file() - Create a single debugfs file.
 * @dir: Directory to create the file in.
 * @data: Data pointer that will be passed to show and store callbacks.
 * @attr: Static information about the file to create (name, mode, show, store).
 */
struct scst_debugfs_file *scst_debugfs_create_file(struct dentry *dir,
					void *data,
					const struct scst_debugfs_attr *attr)
{
	struct scst_debugfs_file *f;

	f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return NULL;
	f->attr = attr;
	f->data = data;
	f->dentry = debugfs_create_file(attr->attr.name, attr->attr.mode, dir,
					f, &scst_debugfs_fops);
	return f;
}

/**
 * scst_debugfs_remove_file() - Remove a single debugfs file.
 */
void scst_debugfs_remove_file(struct scst_debugfs_file *f)
{
	if (!f)
		return;
	debugfs_remove(f->dentry);
	kfree(f);
}

int scst_debugfs_init(void)
{
	int res;

	res = -ENOMEM;
	scst_root_debugfs_dir = debugfs_create_dir("scst", NULL);
	if (!scst_root_debugfs_dir) {
		PRINT_ERROR("%s", "Creation of /sys/kernel/debug/scst failed");
		goto out;
	}

	res = scst_sgv_debugfs_create(scst_root_debugfs_dir);
	if (res) {
		PRINT_ERROR("%s", "Creation of /sys/kernel/debug/scst/sgv"
			    " failed");
		goto err;
	}

	scst_tgtt_debugfs_dir = debugfs_create_dir("target_driver",
					     scst_root_debugfs_dir);
	if (!scst_tgtt_debugfs_dir) {
		PRINT_ERROR("%s", "Creation of /sys/kernel/debug/scst/"
			    "target_driver failed");
		goto err;
	}

	scst_tgt_debugfs_dir = debugfs_create_dir("target",
						  scst_root_debugfs_dir);
	if (!scst_tgt_debugfs_dir) {
		PRINT_ERROR("%s", "Creation of /sys/kernel/debug/scst/target"
			    " failed");
		goto err;
	}

	scst_devt_debugfs_dir = debugfs_create_dir("device_type",
					     scst_root_debugfs_dir);
	if (!scst_devt_debugfs_dir) {
		PRINT_ERROR("%s", "Creation of /sys/kernel/debug/scst/"
			    "device_type failed");
		goto err;
	}

	scst_dev_debugfs_dir = debugfs_create_dir("device",
						  scst_root_debugfs_dir);
	if (!scst_dev_debugfs_dir) {
		PRINT_ERROR("%s", "Creation of /sys/kernel/debug/scst/device"
			    " failed");
		goto err;
	}

	res = 0;

out:
	return res;
err:
	scst_debugfs_cleanup();
	goto out;
}

void scst_debugfs_cleanup(void)
{
	scst_sgv_debugfs_remove();
	debugfs_remove_recursive(scst_root_debugfs_dir);
	scst_dev_debugfs_dir = NULL;
	scst_devt_debugfs_dir = NULL;
	scst_tgt_debugfs_dir = NULL;
	scst_tgtt_debugfs_dir = NULL;
	scst_root_debugfs_dir = NULL;
}

int scst_main_create_debugfs_dir(void)
{
	scst_main_debugfs_dir
		= debugfs_create_dir("main", scst_root_debugfs_dir);
	return scst_main_debugfs_dir ? 0 : -ENOMEM;
}

void scst_main_remove_debugfs_dir(void)
{
	debugfs_remove_recursive(scst_main_debugfs_dir);
	scst_main_debugfs_dir = NULL;
}

struct dentry *scst_get_main_debugfs_dir(void)
{
	return scst_main_debugfs_dir;
}

int scst_tgtt_create_debugfs_dir(struct scst_tgt_template *tgtt)
{
	tgtt->debugfs_dir = debugfs_create_dir(tgtt->name,
					       scst_tgtt_debugfs_dir);
	return tgtt->debugfs_dir ? 0 : -ENOMEM;
}

void scst_tgtt_remove_debugfs_dir(struct scst_tgt_template *tgtt)
{
	debugfs_remove_recursive(tgtt->debugfs_dir);
	tgtt->debugfs_dir = NULL;
}

int scst_tgt_create_debugfs_dir(struct scst_tgt *tgt)
{
	int res;

	res = -ENOMEM;
	tgt->debugfs_dir = debugfs_create_dir(tgt->tgt_name,
					      scst_tgt_debugfs_dir);
	if (!tgt->debugfs_dir) {
		PRINT_ERROR("Creation of directory %s failed", tgt->tgt_name);
		goto out;
	}
	tgt->sessions_dir = debugfs_create_dir("sessions", tgt->debugfs_dir);
	if (!tgt->sessions_dir) {
		PRINT_ERROR("Creation of directory %s/sessions failed",
			    tgt->tgt_name);
		goto out;
	}
	res = 0;
out:
	return res;
}

void scst_tgt_remove_debugfs_dir(struct scst_tgt *tgt)
{
	debugfs_remove_recursive(tgt->debugfs_dir);
	tgt->debugfs_dir = NULL;
}

int scst_sess_create_debugfs_dir(struct scst_session *sess)
{
	int res;

	res = -ENOMEM;
	sess->debugfs_dir = debugfs_create_dir(sess->initiator_name,
					       sess->tgt->sessions_dir);
	if (!sess->debugfs_dir) {
		PRINT_ERROR("Creation of directory %s failed",
			    sess->initiator_name);
		goto out;
	}
	sess->luns_dir = debugfs_create_dir("luns", sess->debugfs_dir);
	if (!sess->luns_dir) {
		PRINT_ERROR("Creation of directory %s/luns failed",
			    sess->initiator_name);
		goto out;
	}
	res = 0;
out:
	return res;
}

void scst_sess_remove_debugfs_dir(struct scst_session *sess)
{
	debugfs_remove_recursive(sess->debugfs_dir);
	sess->debugfs_dir = NULL;
}

int scst_tgt_dev_create_debugfs_dir(struct scst_tgt_dev *tgt_dev)
{
	int res;
	char lun[16];

	res = -ENOMEM;
	snprintf(lun, sizeof(lun), "%llx", tgt_dev->lun);
	tgt_dev->debugfs_dir = debugfs_create_dir(lun, tgt_dev->sess->luns_dir);
	if (!tgt_dev->debugfs_dir) {
		PRINT_ERROR("Creation of directory %s/luns/%s failed",
			    tgt_dev->sess->initiator_name, lun);
		goto out;
	}
	res = 0;
out:
	return res;
}

void scst_tgt_dev_remove_debugfs_dir(struct scst_tgt_dev *tgt_dev)
{
	debugfs_remove_recursive(tgt_dev->debugfs_dir);
	tgt_dev->debugfs_dir = NULL;
}

int scst_devt_create_debugfs_dir(struct scst_dev_type *devt)
{
	devt->debugfs_dir = debugfs_create_dir(devt->name,
					       scst_devt_debugfs_dir);
	return devt->debugfs_dir ? 0 : -ENOMEM;
}

void scst_devt_remove_debugfs_dir(struct scst_dev_type *devt)
{
	debugfs_remove_recursive(devt->debugfs_dir);
	devt->debugfs_dir = NULL;
}

int scst_dev_create_debugfs_dir(struct scst_device *dev)
{
	dev->debugfs_dir = debugfs_create_dir(dev->virt_name,
					      scst_dev_debugfs_dir);
	return dev->debugfs_dir ? 0 : -ENOMEM;
}

void scst_dev_remove_debugfs_dir(struct scst_device *dev)
{
	debugfs_remove_recursive(dev->debugfs_dir);
	dev->debugfs_dir = NULL;
}

#endif /*defined(CONFIG_DEBUG_FS)*/
