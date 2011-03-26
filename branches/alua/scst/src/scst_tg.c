/*
 *  scst_tg.c
 *
 *  Target group related code.
 *
 *  Copyright (C) 2011 Bart Van Assche <bvanassche@acm.org>.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#include <asm/unaligned.h>
#include "scst.h"
#include "scst_priv.h"

static struct list_head scst_dev_group_list;

/* Look up a device group by name. */
static struct scst_dev_group *__lookup_dg_by_name(const char *name)
{
	struct scst_dev_group *dg;

	list_for_each_entry(dg, &scst_dev_group_list, entry)
		if (strcmp(dg->name, name) == 0)
			return dg;

	return NULL;
}

/* Look up a device node in a device group by device. */
static struct scst_dg_dev *
__lookup_dg_dev(struct scst_dev_group *dg, struct scst_device *dev)
{
	struct scst_dg_dev *dgd;

	list_for_each_entry(dgd, &dg->dev_list, entry)
		if (dgd->dev == dev)
			return dgd;

	return NULL;
}

/* Look up a device group by device. */
static struct scst_dev_group *__lookup_dg_by_dev(struct scst_device *dev)
{
	struct scst_dev_group *dg;

	list_for_each_entry(dg, &scst_dev_group_list, entry)
		if (__lookup_dg_dev(dg, dev))
		    return dg;

	return NULL;
}

static void scst_release_dev_group(struct kobject *kobj)
{
	struct scst_dev_group *dg;

	dg = container_of(kobj, struct scst_dev_group, kobj);
	kfree(dg->name);
	kfree(dg);
}

static struct kobj_type scst_dg_ktype = {
	.sysfs_ops = &scst_sysfs_ops,
	.release = scst_release_dev_group,
};

/**
 * scst_dg_create() - Create a new device group object and add it to sysfs.
 */
int scst_dg_create(struct kobject *parent, const char *name)
{
	struct scst_dev_group *dg;
	int res;

	TRACE_ENTRY();

	res = mutex_lock_interruptible(&scst_mutex);
	if (res)
		goto out;
	res = -EEXIST;
	if (__lookup_dg_by_name(name))
		goto out_unlock;
	res = -ENOMEM;
	dg = kzalloc(sizeof(*dg), GFP_KERNEL);
	if (!dg)
		goto out_unlock;
	dg->name = kstrdup(name, GFP_KERNEL);
	if (!dg->name)
		goto out_free;
	INIT_LIST_HEAD(&dg->dev_list);
	INIT_LIST_HEAD(&dg->tg_list);
	list_add_tail(&dg->entry, &scst_dev_group_list);
	res = kobject_init_and_add(&dg->kobj, &scst_dg_ktype, parent, "%s",
				   name);
	if (res)
		goto out_del_put;
	res = -EEXIST;
	dg->dev_kobj = kobject_create_and_add("devices", &dg->kobj);
	if (!dg->dev_kobj)
		goto out_put_dev;
	dg->tg_kobj = kobject_create_and_add("target_groups", &dg->kobj);
	if (!dg->tg_kobj)
		goto out_put_tg;
	res = 0;
out_unlock:
	mutex_unlock(&scst_mutex);
out:
	TRACE_EXIT_RES(res);
	return res;

out_put_tg:
	kobject_del(dg->tg_kobj);
	kobject_put(dg->tg_kobj);
out_put_dev:
	kobject_del(dg->dev_kobj);
	kobject_put(dg->dev_kobj);
out_del_put:
	kobject_del(&dg->kobj);
	list_del(&dg->entry);
	kobject_put(&dg->kobj);
	goto out_unlock;
out_free:
	kfree(dg);
	goto out_unlock;
}

static void __scst_dg_destroy(struct scst_dev_group *dg)
{
	kobject_del(dg->tg_kobj);
	kobject_put(dg->tg_kobj);
	kobject_del(dg->dev_kobj);
	kobject_put(dg->dev_kobj);
	kobject_del(&dg->kobj);
	list_del(&dg->entry);
	while (!list_empty(&dg->dev_list))
		kfree(list_first_entry(&dg->dev_list, struct scst_dg_dev,
				       entry));
	while (!list_empty(&dg->tg_list))
		kfree(list_first_entry(&dg->tg_list, struct scst_tg_tgt,
				       entry));
	kobject_put(&dg->kobj);
}

int scst_dg_destroy(const char *name)
{
	struct scst_dev_group *dg;
	int res;

	res = mutex_lock_interruptible(&scst_mutex);
	if (res)
		goto out;
	res = -EINVAL;
	dg = __lookup_dg_by_name(name);
	if (!dg)
		goto out_unlock;
	__scst_dg_destroy(dg);
	res = 0;
out_unlock:
	mutex_unlock(&scst_mutex);
out:
	return res;
}

void scst_tg_init(void)
{
	INIT_LIST_HEAD(&scst_dev_group_list);
}

void scst_tg_cleanup(void)
{
	struct scst_dev_group *tg;

	mutex_lock(&scst_mutex);
	while (!list_empty(&scst_dev_group_list)) {
		tg = list_first_entry(&scst_dev_group_list,
				      struct scst_dev_group, entry);
		__scst_dg_destroy(tg);
	}
	mutex_unlock(&scst_mutex);
}

/**
 * scst_tg_get_group_info() - Build REPORT TARGET GROUPS response.
 * @buf: Pointer to a pointer to which the result buffer pointer will be set.
 * @length: Response length, including the "RETURN DATA LENGTH" field.
 * @dev: Pointer to the SCST device for which to obtain group information.
 * @data_format: Three-bit response data format specification.
 */
int scst_tg_get_group_info(void **buf, uint32_t *length,
			   struct scst_device *dev, uint8_t data_format)
{
	struct scst_dev_group *dg;
	struct scst_target_group *tg;
	struct scst_tg_tgt *tgtgt;
	struct scst_tgt *tgt;
	uint8_t *p;
	uint32_t ret_data_len;
	int res;

	BUG_ON(!buf);
	BUG_ON(!length);

	ret_data_len = 0;

	res = -EINVAL;
	switch (data_format) {
	case 0:
		break;
	case 1:
		/* Extended header */
		ret_data_len += 4;
		break;
	default:
		goto out;
	}

	*length = 4;

	mutex_lock(&scst_mutex);

	res = 0;
	dg = __lookup_dg_by_dev(dev);
	if (!dg)
		goto out_unlock;

	list_for_each_entry(tg, &dg->tg_list, entry) {
		/* Target port group descriptor header. */
		ret_data_len += 8;
		list_for_each_entry(tgtgt, &tg->tgt_list, entry) {
			/* Target port descriptor. */
			ret_data_len += 4;
		}
	}

	*length += ret_data_len;

	res = -ENOMEM;
	*buf = kzalloc(*length, GFP_KERNEL);
	if (!*buf)
		goto out_unlock;

	p = *buf;
	/* Return data length. */
	put_unaligned(cpu_to_be32(ret_data_len), (__be32 *)p);
	p += 4;
	if (data_format == 1) {
		/* Extended header */
		*p++ = 0x10; /* format = 1 */
		*p++ = 0x00; /* implicit transition time = 0 */
		p += 2;      /* reserved */
	}

	list_for_each_entry(tg, &dg->tg_list, entry) {
		/* Target port group descriptor header. */
		*p++ = tg->state;
		*p++ = 0xCF; /* T_SUP; O_SUP; U_SUP; S_SUP; AN_SUP; AO_SUP */
		put_unaligned(cpu_to_be16(tg->group_id), (__be16 *)p);
		p += 2;
		p++;      /* reserved */
		*p++ = 2; /* status code: implicit transition */
		p++;      /* vendor specific */
		list_for_each_entry(tgtgt, &tg->tgt_list, entry)
			(*p)++; /* target port count */
		p++;
		list_for_each_entry(tgtgt, &tg->tgt_list, entry) {
			tgt = tgtgt->tgt;
			/* Target port descriptor. */
			p += 2; /* reserved */
			/* Relative target port identifier. */
			put_unaligned(cpu_to_be16(tgt->rel_tgt_id),
				      (__be16 *)p);
			p += 2;
		}
	}

	WARN_ON(p - (uint8_t *)*buf != *length);

	res = 0;

out_unlock:
	mutex_unlock(&scst_mutex);
out:
	return res;
}
EXPORT_SYMBOL(scst_tg_get_group_info);
