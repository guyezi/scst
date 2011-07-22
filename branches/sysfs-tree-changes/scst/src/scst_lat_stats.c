/*
 *  scst_lat_stats.c
 *
 *  Copyright (C) 2009 - 2010 Vladislav Bolkhovitin <vst@vlnb.net>
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
#include <linux/list.h>
#include <linux/types.h>
#ifdef INSIDE_KERNEL_TREE
#include <scst/scst.h>
#include <scst/scst_debug.h>
#else
#include "scst.h"
#include "scst_debug.h"
#endif
#include "scst_debugfs.h"
#include "scst_priv.h"
#include "scst_lat_stats.h"

#ifdef CONFIG_SCST_MEASURE_LATENCY

/* Per LUN latency statistics. */

static char *scst_io_size_names[] = {
	"<=8K  ",
	"<=32K ",
	"<=128K",
	"<=512K",
	">512K "
};

static ssize_t scst_tgt_dev_latency_show(void *data,
					 const struct scst_debugfs_attr *attr,
					 char *buffer)
{
	struct scst_tgt_dev *tgt_dev = data;
	int res, i;
	char buf[50];

	BUG_ON(!tgt_dev);
	res = 0;
	for (i = 0; i < SCST_LATENCY_STATS_NUM; i++) {
		uint64_t scst_time_wr, tgt_time_wr, dev_time_wr;
		unsigned int processed_cmds_wr;
		uint64_t scst_time_rd, tgt_time_rd, dev_time_rd;
		unsigned int processed_cmds_rd;
		struct scst_ext_latency_stat *latency_stat;

		latency_stat = &tgt_dev->dev_latency_stat[i];
		scst_time_wr = latency_stat->scst_time_wr;
		scst_time_rd = latency_stat->scst_time_rd;
		tgt_time_wr = latency_stat->tgt_time_wr;
		tgt_time_rd = latency_stat->tgt_time_rd;
		dev_time_wr = latency_stat->dev_time_wr;
		dev_time_rd = latency_stat->dev_time_rd;
		processed_cmds_wr = latency_stat->processed_cmds_wr;
		processed_cmds_rd = latency_stat->processed_cmds_rd;

		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			 "%-5s %-9s %-15lu ", "Write", scst_io_size_names[i],
			(unsigned long)processed_cmds_wr);
		if (processed_cmds_wr == 0)
			processed_cmds_wr = 1;

		do_div(scst_time_wr, processed_cmds_wr);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_scst_time_wr,
			(unsigned long)scst_time_wr,
			(unsigned long)latency_stat->max_scst_time_wr,
			(unsigned long)latency_stat->scst_time_wr);
		res += scnprintf(&buffer[res], PAGE_SIZE - res, "%-47s", buf);

		do_div(tgt_time_wr, processed_cmds_wr);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_tgt_time_wr,
			(unsigned long)tgt_time_wr,
			(unsigned long)latency_stat->max_tgt_time_wr,
			(unsigned long)latency_stat->tgt_time_wr);
		res += scnprintf(&buffer[res], PAGE_SIZE - res, "%-47s", buf);

		do_div(dev_time_wr, processed_cmds_wr);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_dev_time_wr,
			(unsigned long)dev_time_wr,
			(unsigned long)latency_stat->max_dev_time_wr,
			(unsigned long)latency_stat->dev_time_wr);
		res += scnprintf(&buffer[res], PAGE_SIZE - res, "%-47s\n", buf);

		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-5s %-9s %-15lu ", "Read", scst_io_size_names[i],
			(unsigned long)processed_cmds_rd);
		if (processed_cmds_rd == 0)
			processed_cmds_rd = 1;

		do_div(scst_time_rd, processed_cmds_rd);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_scst_time_rd,
			(unsigned long)scst_time_rd,
			(unsigned long)latency_stat->max_scst_time_rd,
			(unsigned long)latency_stat->scst_time_rd);
		res += scnprintf(&buffer[res], PAGE_SIZE - res, "%-47s", buf);

		do_div(tgt_time_rd, processed_cmds_rd);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_tgt_time_rd,
			(unsigned long)tgt_time_rd,
			(unsigned long)latency_stat->max_tgt_time_rd,
			(unsigned long)latency_stat->tgt_time_rd);
		res += scnprintf(&buffer[res], PAGE_SIZE - res, "%-47s", buf);

		do_div(dev_time_rd, processed_cmds_rd);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_dev_time_rd,
			(unsigned long)dev_time_rd,
			(unsigned long)latency_stat->max_dev_time_rd,
			(unsigned long)latency_stat->dev_time_rd);
		res += scnprintf(&buffer[res], PAGE_SIZE - res, "%-47s\n", buf);
	}
	return res;
}

static struct scst_debugfs_attr scst_tgt_dev_lat_attr =
	__ATTR(latency, S_IRUGO, scst_tgt_dev_latency_show, NULL);

int scst_tgt_dev_lat_create(struct scst_tgt_dev *tgt_dev)
{
	tgt_dev->latency_file = scst_debugfs_create_file(tgt_dev->debugfs_dir,
					tgt_dev, &scst_tgt_dev_lat_attr);
	return tgt_dev->latency_file ? 0 : -ENOMEM;
}

void scst_tgt_dev_lat_remove(struct scst_tgt_dev *tgt_dev)
{
	scst_debugfs_remove_file(tgt_dev->latency_file);
	tgt_dev->latency_file = NULL;
}

/* Per session latency statistics. */

static ssize_t scst_sess_latency_show(void *data,
				      const struct scst_debugfs_attr *attr,
				      char *buffer)
{
	struct scst_session *sess = data;
	ssize_t res;
	int i;
	char buf[50];
	uint64_t scst_time, tgt_time, dev_time;
	unsigned int processed_cmds;

	BUG_ON(!sess);
	res = 0;
	res += scnprintf(&buffer[res], PAGE_SIZE - res,
		"%-15s %-15s %-46s %-46s %-46s\n",
		"T-L names", "Total commands", "SCST latency",
		"Target latency", "Dev latency (min/avg/max/all ns)");

	spin_lock_bh(&sess->lat_lock);

	for (i = 0; i < SCST_LATENCY_STATS_NUM ; i++) {
		uint64_t scst_time_wr, tgt_time_wr, dev_time_wr;
		unsigned int processed_cmds_wr;
		uint64_t scst_time_rd, tgt_time_rd, dev_time_rd;
		unsigned int processed_cmds_rd;
		struct scst_ext_latency_stat *latency_stat;

		latency_stat = &sess->sess_latency_stat[i];
		scst_time_wr = latency_stat->scst_time_wr;
		scst_time_rd = latency_stat->scst_time_rd;
		tgt_time_wr = latency_stat->tgt_time_wr;
		tgt_time_rd = latency_stat->tgt_time_rd;
		dev_time_wr = latency_stat->dev_time_wr;
		dev_time_rd = latency_stat->dev_time_rd;
		processed_cmds_wr = latency_stat->processed_cmds_wr;
		processed_cmds_rd = latency_stat->processed_cmds_rd;

		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-5s %-9s %-15lu ",
			"Write", scst_io_size_names[i],
			(unsigned long)processed_cmds_wr);
		if (processed_cmds_wr == 0)
			processed_cmds_wr = 1;

		do_div(scst_time_wr, processed_cmds_wr);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_scst_time_wr,
			(unsigned long)scst_time_wr,
			(unsigned long)latency_stat->max_scst_time_wr,
			(unsigned long)latency_stat->scst_time_wr);
		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-47s", buf);

		do_div(tgt_time_wr, processed_cmds_wr);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_tgt_time_wr,
			(unsigned long)tgt_time_wr,
			(unsigned long)latency_stat->max_tgt_time_wr,
			(unsigned long)latency_stat->tgt_time_wr);
		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-47s", buf);

		do_div(dev_time_wr, processed_cmds_wr);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_dev_time_wr,
			(unsigned long)dev_time_wr,
			(unsigned long)latency_stat->max_dev_time_wr,
			(unsigned long)latency_stat->dev_time_wr);
		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-47s\n", buf);

		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-5s %-9s %-15lu ",
			"Read", scst_io_size_names[i],
			(unsigned long)processed_cmds_rd);
		if (processed_cmds_rd == 0)
			processed_cmds_rd = 1;

		do_div(scst_time_rd, processed_cmds_rd);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_scst_time_rd,
			(unsigned long)scst_time_rd,
			(unsigned long)latency_stat->max_scst_time_rd,
			(unsigned long)latency_stat->scst_time_rd);
		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-47s", buf);

		do_div(tgt_time_rd, processed_cmds_rd);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_tgt_time_rd,
			(unsigned long)tgt_time_rd,
			(unsigned long)latency_stat->max_tgt_time_rd,
			(unsigned long)latency_stat->tgt_time_rd);
		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-47s", buf);

		do_div(dev_time_rd, processed_cmds_rd);
		snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
			(unsigned long)latency_stat->min_dev_time_rd,
			(unsigned long)dev_time_rd,
			(unsigned long)latency_stat->max_dev_time_rd,
			(unsigned long)latency_stat->dev_time_rd);
		res += scnprintf(&buffer[res], PAGE_SIZE - res,
			"%-47s\n", buf);
	}

	scst_time = sess->scst_time;
	tgt_time = sess->tgt_time;
	dev_time = sess->dev_time;
	processed_cmds = sess->processed_cmds;

	res += scnprintf(&buffer[res], PAGE_SIZE - res,
		"\n%-15s %-16d", "Overall ", processed_cmds);

	if (processed_cmds == 0)
		processed_cmds = 1;

	do_div(scst_time, processed_cmds);
	snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
		(unsigned long)sess->min_scst_time,
		(unsigned long)scst_time,
		(unsigned long)sess->max_scst_time,
		(unsigned long)sess->scst_time);
	res += scnprintf(&buffer[res], PAGE_SIZE - res,
		"%-47s", buf);

	do_div(tgt_time, processed_cmds);
	snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
		(unsigned long)sess->min_tgt_time,
		(unsigned long)tgt_time,
		(unsigned long)sess->max_tgt_time,
		(unsigned long)sess->tgt_time);
	res += scnprintf(&buffer[res], PAGE_SIZE - res,
		"%-47s", buf);

	do_div(dev_time, processed_cmds);
	snprintf(buf, sizeof(buf), "%lu/%lu/%lu/%lu",
		(unsigned long)sess->min_dev_time,
		(unsigned long)dev_time,
		(unsigned long)sess->max_dev_time,
		(unsigned long)sess->dev_time);
	res += scnprintf(&buffer[res], PAGE_SIZE - res,
		"%-47s\n\n", buf);

	spin_unlock_bh(&sess->lat_lock);
	return res;
}

static ssize_t scst_sess_latency_store(void *data,
				       const struct scst_debugfs_attr *attr,
				       const char *buffer, size_t count)
{
	struct scst_session *sess = data;
	int res, t;

	BUG_ON(!sess);
	res = mutex_lock_interruptible(&scst_mutex);
	if (res)
		goto out;

	PRINT_INFO("Zeroing latency statistics for initiator "
		"%s", sess->initiator_name);

	spin_lock_bh(&sess->lat_lock);

	sess->scst_time = 0;
	sess->tgt_time = 0;
	sess->dev_time = 0;
	sess->min_scst_time = 0;
	sess->min_tgt_time = 0;
	sess->min_dev_time = 0;
	sess->max_scst_time = 0;
	sess->max_tgt_time = 0;
	sess->max_dev_time = 0;
	sess->processed_cmds = 0;
	memset(sess->sess_latency_stat, 0,
		sizeof(sess->sess_latency_stat));

	for (t = SESS_TGT_DEV_LIST_HASH_SIZE-1; t >= 0; t--) {
		struct list_head *head = &sess->sess_tgt_dev_list[t];
		struct scst_tgt_dev *tgt_dev;
		list_for_each_entry(tgt_dev, head, sess_tgt_dev_list_entry) {
			tgt_dev->scst_time = 0;
			tgt_dev->tgt_time = 0;
			tgt_dev->dev_time = 0;
			tgt_dev->processed_cmds = 0;
			memset(tgt_dev->dev_latency_stat, 0,
				sizeof(tgt_dev->dev_latency_stat));
		}
	}

	spin_unlock_bh(&sess->lat_lock);

	mutex_unlock(&scst_mutex);

	res = count;
out:
	return res;
}

static const struct scst_debugfs_attr scst_sess_lat_attr =
	__ATTR(latency, S_IRUGO | S_IWUSR, scst_sess_latency_show,
	       scst_sess_latency_store);

int scst_sess_lat_create(struct scst_session *sess)
{
	sess->latency_file = scst_debugfs_create_file(sess->debugfs_dir, sess,
						      &scst_sess_lat_attr);
	return sess->latency_file ? 0 : -ENOMEM;
}

void scst_sess_lat_remove(struct scst_session *sess)
{
	scst_debugfs_remove_file(sess->latency_file);
	sess->latency_file = NULL;
}

#endif /*CONFIG_SCST_MEASURE_LATENCY*/
