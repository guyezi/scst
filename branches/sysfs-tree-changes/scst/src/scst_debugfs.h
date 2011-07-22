/*
 *  scst_debugfs.h
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

#ifndef _SCST_DEBUGFS_H_
#define _SCST_DEBUGFS_H_

#ifdef INSIDE_KERNEL_TREE
#include <scst/scst.h>
#else
#include "scst.h"
#endif

#if defined(CONFIG_DEBUG_FS)

struct scst_debugfs_attr {
	struct attribute attr;
	ssize_t (*show)(void *data, const struct scst_debugfs_attr *attr,
			char *buffer);
	ssize_t (*store)(void *data, const struct scst_debugfs_attr *attr,
			 const char *buffer, size_t count);
};

struct scst_debugfs_file {
	const struct scst_debugfs_attr	*attr;
	void				*data;
	struct dentry			*dentry;
};

struct scst_debugfs_file *__must_check
	scst_debugfs_create_file(struct dentry *dir, void *data,
				 const struct scst_debugfs_attr *attr);
void scst_debugfs_remove_file(struct scst_debugfs_file *dd);

int scst_debugfs_init(void);
void scst_debugfs_cleanup(void);
int scst_main_create_debugfs_dir(void);
void scst_main_remove_debugfs_dir(void);
struct dentry *scst_get_main_debugfs_dir(void);
int scst_tgtt_create_debugfs_dir(struct scst_tgt_template *tgtt);
void scst_tgtt_remove_debugfs_dir(struct scst_tgt_template *tgtt);
int scst_tgt_create_debugfs_dir(struct scst_tgt *tgt);
void scst_tgt_remove_debugfs_dir(struct scst_tgt *tgt);
int scst_sess_create_debugfs_dir(struct scst_session *sess);
void scst_sess_remove_debugfs_dir(struct scst_session *sess);
int scst_tgt_dev_create_debugfs_dir(struct scst_tgt_dev *tgt_dev);
void scst_tgt_dev_remove_debugfs_dir(struct scst_tgt_dev *tgt_dev);
int scst_devt_create_debugfs_dir(struct scst_dev_type *devt);
void scst_devt_remove_debugfs_dir(struct scst_dev_type *devt);
int scst_dev_create_debugfs_dir(struct scst_device *dev);
void scst_dev_remove_debugfs_dir(struct scst_device *dev);

#else /*defined(CONFIG_DEBUG_FS)*/

static inline int scst_debugfs_init(void)
{
	return 0;
}

static inline void scst_debugfs_cleanup(void)
{
}

static inline int scst_main_create_debugfs_dir(void)
{
	return 0;
}

static inline void scst_main_remove_debugfs_dir(void)
{
}

struct dentry *scst_get_main_debugfs_dir(void)
{
	return NULL
}

static inline int scst_tgtt_create_debugfs_dir(struct scst_tgt_template *tgtt)
{
	return 0;
}

static inline void scst_tgtt_remove_debugfs_dir(struct scst_tgt_template *tgtt)
{
}

static inline int scst_tgt_create_debugfs_dir(struct scst_tgt *tgt)
{
	return 0;
}

static inline void scst_tgt_remove_debugfs_dir(struct scst_tgt *tgt)
{
}

static inline int scst_sess_create_debugfs_dir(struct scst_session *sess)
{
	return 0;
}

static inline void scst_sess_remove_debugfs_dir(struct scst_session *sess)
{
}

static inline int scst_tgt_dev_create_debugfs_dir(struct scst_tgt_dev *tgt_dev)
{
	return 0;
}

static inline void scst_tgt_dev_remove_debugfs_dir(struct scst_tgt_dev *tgt_dev)
{
}

static inline int scst_devt_create_debugfs_dir(struct scst_dev_type *devt)
{
	return 0;
}

static inline void scst_devt_remove_debugfs_dir(struct scst_dev_type *devt)
{
}

static inline int scst_dev_create_debugfs_dir(struct scst_device *dev)
{
	return 0;
}

static inline void scst_dev_remove_debugfs_dir(struct scst_device *dev)
{
}

#endif /*defined(CONFIG_DEBUG_FS)*/

#endif /*_SCST_DEBUGFS_H_*/
