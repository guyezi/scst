/*
 *  scst_mem_stats.c
 *
 *  Copyright (C) 2006 - 2010 Vladislav Bolkhovitin <vst@vlnb.net>
 *  Copyright (C) 2007 - 2010 ID7 Ltd.
 *  Copyright (C) 2010 Bart Van Assche <bvanassche@acm.org>.
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
#else
#include "scst.h"
#endif
#include "scst_debugfs.h"
#include "scst_mem.h"
#include "scst_mem_stats.h"
#include "scst_priv.h"

#if defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)

static struct dentry *sgv_debugfs_dir;
static struct scst_debugfs_file *sgv_global_stats_file;

static const struct scst_debugfs_attr sgv_pool_stats_attr =
	__ATTR(stats, S_IRUGO | S_IWUSR, sgv_pool_stats_show,
	       sgv_pool_stats_store);

int scst_sgv_pool_debugfs_create(struct sgv_pool *pool)
{
	int res;

	res = -ENOMEM;
	pool->debugfs_dir = debugfs_create_dir(pool->name, sgv_debugfs_dir);
	if (!pool->debugfs_dir)
		goto out;
	pool->stats_file = scst_debugfs_create_file(pool->debugfs_dir, pool,
						    &sgv_pool_stats_attr);
	if (!pool->stats_file)
		goto err;
	res = 0;
out:
	return res;
err:
	scst_sgv_pool_debugfs_remove(pool);
	goto out;
}

void scst_sgv_pool_debugfs_remove(struct sgv_pool *pool)
{
	scst_debugfs_remove_file(pool->stats_file);
	debugfs_remove_recursive(pool->debugfs_dir);
	pool->debugfs_dir = NULL;
}

static const struct scst_debugfs_attr sgv_global_stats_attr =
	__ATTR(global_stats, S_IRUGO | S_IWUSR, sgv_global_stats_show,
	       sgv_global_stats_store);

int scst_sgv_debugfs_create(struct dentry *parent)
{
	int res;

	res = -ENOMEM;
	sgv_debugfs_dir = debugfs_create_dir("sgv", parent);
	if (!sgv_debugfs_dir)
		goto out;
	sgv_global_stats_file = scst_debugfs_create_file(sgv_debugfs_dir, NULL,
						&sgv_global_stats_attr);
	if (!sgv_global_stats_file)
		goto err;
	res = 0;
out:
	return res;
err:
	scst_sgv_debugfs_remove();
	goto out;
}

void scst_sgv_debugfs_remove(void)
{
	scst_debugfs_remove_file(sgv_global_stats_file);
	debugfs_remove_recursive(sgv_debugfs_dir);
	sgv_debugfs_dir = NULL;
}

#endif /*defined(CONFIG_SCST_DEBUG) || defined(CONFIG_SCST_TRACING)*/
