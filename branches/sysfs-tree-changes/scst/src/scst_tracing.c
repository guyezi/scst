/*
 *  scst_tracing.c
 *
 *  Copyright (C) 2010 Bart Van Assche <bvanassche@acm.org>
 *  Copyright (C) 2009 - 2010 Vladislav Bolkhovitin <vst@vlnb.net>
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
#ifdef INSIDE_KERNEL_TREE
#include <scst/scst.h>
#include <scst/scst_debug.h>
#else
#include "scst.h"
#include "scst_debug.h"
#endif
#include "scst_mem.h"
#include "scst_priv.h"
#include "scst_pres.h"
#include "scst_tracing.h"

#if !defined(CONFIG_SCST_PROC) && (defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING))

static void scst_remove_trace_files(struct scst_trace_files *trace_files);

static struct scst_trace_files *scst_main_trace_files;
static DEFINE_MUTEX(scst_log_mutex);

static struct scst_trace_log scst_trace_tbl[] = {
	{ TRACE_OUT_OF_MEM,	"out_of_mem"	},
	{ TRACE_MINOR,		"minor"		},
	{ TRACE_SG_OP,		"sg"		},
	{ TRACE_MEMORY,		"mem"		},
	{ TRACE_BUFF,		"buff"		},
	{ TRACE_PID,		"pid"		},
	{ TRACE_LINE,		"line"		},
	{ TRACE_FUNCTION,	"function"	},
	{ TRACE_DEBUG,		"debug"		},
	{ TRACE_SPECIAL,	"special"	},
	{ TRACE_SCSI,		"scsi"		},
	{ TRACE_MGMT,		"mgmt"		},
	{ TRACE_MGMT_DEBUG,	"mgmt_dbg"	},
	{ TRACE_FLOW_CONTROL,	"flow_control"	},
	{ TRACE_PRES,		"pr"		},
	{ 0,			NULL		}
};

static struct scst_trace_log scst_local_trace_tbl[] = {
	{ TRACE_RTRY,			"retry"			},
	{ TRACE_SCSI_SERIALIZING,	"scsi_serializing"	},
	{ TRACE_DATA_SEND,		"data_send"		},
	{ TRACE_DATA_RECEIVED,		"data_received"		},
	{ 0,				NULL			}
};

static struct scst_trace_data scst_main_trace_data = {
	.default_trace_flags	= SCST_DEFAULT_LOG_FLAGS,
	.trace_flags		= &trace_flag,
	.trace_tbl		= scst_local_trace_tbl,
};

/**
 * scst_lookup_token() - Look up the name of a tracing token.
 *
 * Always returns a non-NULL pointer. If the token has been found, upon return
 * p->val != 0 and p->token != NULL. If the token has not been found, upon
 * return p->val == 0 and p->token == NULL.
 */
static struct scst_trace_log *scst_lookup_token(struct scst_trace_data *td,
						const char *name)
{
	struct scst_trace_log *p;

	if (td->trace_tbl)
		for (p = td->trace_tbl; p->token; ++p)
			if (strcmp(p->token, name) == 0)
				return p;

	for (p = scst_trace_tbl; p->token; ++p)
		if (strcmp(p->token, name) == 0)
			break;

	return p;
}

static ssize_t scst_trace_show(void *data, const struct scst_debugfs_attr *attr,
			       char *buf)
{
	struct scst_trace_data *td = data;
	struct scst_trace_log *p;
	unsigned long trace_flags;
	ssize_t res;

	BUG_ON(!td);
	p = scst_lookup_token(td, attr->attr.name);
	WARN_ON(!p->token || !p->val);
	res = mutex_lock_interruptible(&scst_log_mutex);
	if (res)
		goto out;
	trace_flags = *td->trace_flags;
	mutex_unlock(&scst_log_mutex);
	res = scnprintf(buf, PAGE_SIZE, "%d\n", (trace_flags & p->val) != 0);
out:
	return res;
}

static ssize_t scst_trace_store(void *data,
				const struct scst_debugfs_attr *attr,
				const char *buf, size_t count)
{
	struct scst_trace_data *td = data;
	struct scst_trace_log *p;
	unsigned long newval;
	char input[16];
	ssize_t res;

	BUG_ON(!td);
	snprintf(input, sizeof(input), "%.*s", (int)count, buf);
	res = strict_strtoul(input, 0, &newval);
	if (res)
		goto out;
	p = scst_lookup_token(td, attr->attr.name);
	WARN_ON(!p->token || !p->val);
	res = mutex_lock_interruptible(&scst_log_mutex);
	if (res)
		goto out;
	*td->trace_flags = (*td->trace_flags & ~p->val) | (newval ? p->val : 0);
	mutex_unlock(&scst_log_mutex);
	res = count;
out:
	return res;
}

static struct scst_trace_files *scst_create_trace_files(struct dentry *dir,
						struct scst_trace_data *td)
{
	struct scst_trace_files *trace_files;
	struct scst_attr_file *attr_file;
	struct dentry *tracing_dir;
	const struct scst_trace_log *p;
	const struct scst_trace_log *tracing_table[] = {
		scst_trace_tbl,
		td->trace_tbl,
	};
	int attr_count, i;

	BUG_ON(!dir);
	BUG_ON(!td);

	attr_count = 0;
	for (i = 0; i < ARRAY_SIZE(tracing_table); ++i)
		for (p = tracing_table[i]; p && p->val; ++p)
			attr_count++;

	trace_files = kzalloc(scst_trace_files_size(attr_count), GFP_KERNEL);
	if (!trace_files)
		goto err;

	tracing_dir = debugfs_create_dir("tracing", dir);
	if (!tracing_dir) {
		PRINT_ERROR("%s", "Creation of tracing dir failed");
		goto err;
	}
	trace_files->tracing_dir = tracing_dir;

	attr_file = trace_files->attr_file;
	for (i = 0; i < ARRAY_SIZE(tracing_table); ++i) {
		for (p = tracing_table[i]; p && p->val; ++p) {
			struct scst_debugfs_attr *attr;

			attr = &attr_file->attr;
			attr->attr.name = p->token;
			attr->attr.mode = S_IRUGO | S_IWUSR;
			attr->show = scst_trace_show;
			attr->store = scst_trace_store;
			attr_file->file =
				scst_debugfs_create_file(tracing_dir, td, attr);
			if (!attr_file->file) {
				PRINT_ERROR("Creation of tracing file %s"
					    "failed", p->token);
				goto err;
			}
			attr_file++;
		}
	}
out:
	return trace_files;
err:
	scst_remove_trace_files(trace_files);
	trace_files = NULL;
	goto out;
}

static void scst_remove_trace_files(struct scst_trace_files *trace_files)
{
	int i;

	if (!trace_files)
		return;
	for (i = trace_files->count - 1; i >= 0; --i)
		scst_debugfs_remove_file(trace_files->attr_file[i].file);
	debugfs_remove_recursive(trace_files->tracing_dir);
	kfree(trace_files);
}

int scst_main_create_debugfs_files(struct dentry *dir)
{
	scst_main_trace_files =
		scst_create_trace_files(dir, &scst_main_trace_data);
	return scst_main_trace_files ? 0 : -ENOMEM;
}

void scst_main_remove_debugfs_files(struct dentry *dir)
{
	scst_remove_trace_files(scst_main_trace_files);
}

int scst_tgtt_create_debugfs_files(struct scst_tgt_template *tgtt)
{
	if (tgtt->trace_data.trace_flags) {
		tgtt->trace_files = scst_create_trace_files(tgtt->debugfs_dir,
							    &tgtt->trace_data);
		if (!tgtt->trace_files)
			return -ENOMEM;
	}
	return 0;
}

void scst_tgtt_remove_debugfs_files(struct scst_tgt_template *tgtt)
{
	if (tgtt->trace_data.trace_flags) {
		scst_remove_trace_files(tgtt->trace_files);
		tgtt->trace_files = NULL;
	}
}

int scst_devt_create_debugfs_files(struct scst_dev_type *devt)
{
	if (devt->trace_data.trace_flags &&
	    !scst_create_trace_files(devt->debugfs_dir, &devt->trace_data))
		return -ENOMEM;
	return 0;
}

void scst_devt_remove_debugfs_files(struct scst_dev_type *devt)
{
	if (devt->trace_data.trace_flags)
		scst_remove_trace_files(devt->trace_files);
}

static ssize_t scst_dump_pr_write(void *data,
				  const struct scst_debugfs_attr *attr,
				  const char *buf, size_t count)
{
	struct scst_device *dev = data;

	scst_pr_dump_prs(dev, true);
	return count;
}

static struct scst_debugfs_attr scst_dump_pr_attr =
	__ATTR(dump_pr, S_IWUSR, NULL, scst_dump_pr_write);

int scst_dev_create_debugfs_files(struct scst_device *dev)
{
	dev->dump_pr_file = scst_debugfs_create_file(dev->debugfs_dir,
						     dev,
						     &scst_dump_pr_attr);
	if (!dev->dump_pr_file) {
		PRINT_ERROR("Creating /sys/kernel/debug/device/%s/dump_pr"
			    " failed", dev->virt_name);
		return -ENOMEM;
	}
	return 0;
}

void scst_dev_remove_debugfs_files(struct scst_device *dev)
{
	scst_debugfs_remove_file(dev->dump_pr_file);
	dev->dump_pr_file = NULL;
}

#endif /*defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)*/
