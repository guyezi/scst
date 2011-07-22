/*
 *  scst_tracing.h
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

#ifndef _SCST_TRACING_H_
#define _SCST_TRACING_H_

#ifdef INSIDE_KERNEL_TREE
#include <scst/scst.h>
#else
#include "scst.h"
#endif
#include "scst_debugfs.h"

/**
 * scst_trace_attr - One debugfs file for controlling tracing.
 */
struct scst_trace_attr {
	struct scst_debugfs_attr attr;
	struct scst_trace_data *td;
};

struct scst_attr_file {
	struct scst_debugfs_attr attr;
	struct scst_debugfs_file *file;
};

/**
 * scst_trace_files - debugfs "tracing" directory and the files below it.
 */
struct scst_trace_files {
	struct dentry		*tracing_dir;
	int			 count;
	struct scst_attr_file	 attr_file[0];
};

static inline size_t scst_trace_files_size(int n)
{
	return sizeof(struct scst_trace_files) +
		n * sizeof(struct scst_attr_file);
}

#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)

int scst_main_create_debugfs_files(struct dentry *dir);
void scst_main_remove_debugfs_files(struct dentry *dir);
int scst_tgtt_create_debugfs_files(struct scst_tgt_template *tgtt);
void scst_tgtt_remove_debugfs_files(struct scst_tgt_template *tgtt);
int scst_devt_create_debugfs_files(struct scst_dev_type *devt);
void scst_devt_remove_debugfs_files(struct scst_dev_type *devt);
int scst_dev_create_debugfs_files(struct scst_device *dev);
void scst_dev_remove_debugfs_files(struct scst_device *dev);

#else /*defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)*/

static inline int scst_main_create_debugfs_files(struct dentry *dir)
{
	return 0;
}

static inline void scst_main_remove_debugfs_files(struct dentry *dir)
{
}

static inline int scst_tgtt_create_debugfs_files(struct scst_tgt_template *tgtt)
{
	return 0;
}

static inline void scst_tgtt_remove_debugfs_files(struct scst_tgt_template *t)
{
}

static inline int scst_devt_create_debugfs_files(struct scst_dev_type *devt)
{
	return 0;
}

static inline void scst_devt_remove_debugfs_files(struct scst_dev_type *devt)
{
}

static inline int scst_dev_create_debugfs_files(struct scst_device *dev)
{
	return 0;
}

static inline void scst_dev_remove_debugfs_files(struct scst_device *dev)
{
}

#endif /*defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)*/

#endif /*defined(_SCST_TRACING_H_)*/
