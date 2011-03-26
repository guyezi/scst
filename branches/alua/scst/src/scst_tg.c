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

static struct list_head scst_target_group_list;

void scst_tg_init(void)
{
	INIT_LIST_HEAD(&scst_target_group_list);
}

static void __scst_tg_destroy(struct scst_target_group *tg)
{
	kobject_del(&tg->kobj);
	list_del(&tg->entry);
	INIT_LIST_HEAD(&tg->tgt_list);
	INIT_LIST_HEAD(&tg->dev_list);
	kobject_put(&tg->kobj);
}

void scst_tg_cleanup(void)
{
	struct scst_target_group *tg;

	mutex_lock(&scst_mutex);
	while (!list_empty(&scst_target_group_list)) {
		tg = list_first_entry(&scst_target_group_list,
				      struct scst_target_group, entry);
		__scst_tg_destroy(tg);
	}
	mutex_unlock(&scst_mutex);
}

static struct scst_target_group *__lookup_tg(const char *name)
{
	struct scst_target_group *tg;

	list_for_each_entry(tg, &scst_target_group_list, entry)
		if (strcmp(tg->name, name) == 0)
			return tg;

	return NULL;
}

static struct scst_target_group_dev *
__lookup_tg_dev(struct scst_target_group *tg, struct scst_device *dev)
{
	struct scst_target_group_dev *tgdev;

	list_for_each_entry(tgdev, &tg->dev_list, entry)
		if (tgdev->dev == dev)
			return tgdev;

	return NULL;
}

static void scst_release_target_group(struct kobject *kobj)
{
	struct scst_target_group *tg;

	tg = container_of(kobj, struct scst_target_group, kobj);
	kfree(tg->name);
	kfree(tg);
}

static struct kobj_type scst_tg_ktype = {
	.sysfs_ops = &scst_sysfs_ops,
	.release = scst_release_target_group,
};

/**
 * scst_tg_create() - Create a new target group object and add it to sysfs.
 */
int scst_tg_create(struct kobject *parent, const char *name)
{
	struct scst_target_group *tg;
	int res;

	TRACE_ENTRY();

	res = mutex_lock_interruptible(&scst_mutex);
	if (res)
		goto out;
	res = -EEXIST;
	if (__lookup_tg(name))
		goto out_unlock;
	res = -ENOMEM;
	tg = kzalloc(sizeof(*tg), GFP_KERNEL);
	if (!tg)
		goto out_unlock;
	tg->name = kstrdup(name, GFP_KERNEL);
	if (!tg->name)
		goto out_free;
	INIT_LIST_HEAD(&tg->tgt_list);
	INIT_LIST_HEAD(&tg->dev_list);
	list_add_tail(&tg->entry, &scst_target_group_list);
	res = kobject_init_and_add(&tg->kobj, &scst_tg_ktype, parent, "%s",
				   name);
	if (res)
		goto out_del_put;
out_unlock:
	mutex_unlock(&scst_mutex);
out:
	TRACE_EXIT_RES(res);
	return res;

out_del_put:
	list_del(&tg->entry);
	kobject_put(&tg->kobj);
	goto out_unlock;
out_free:
	kfree(tg);
	goto out_unlock;
}

int scst_tg_destroy(const char *name)
{
	struct scst_target_group *tg;
	int res;

	res = mutex_lock_interruptible(&scst_mutex);
	if (res)
		goto out;
	res = -EINVAL;
	tg = __lookup_tg(name);
	if (!tg)
		goto out_unlock;
	__scst_tg_destroy(tg);
	res = 0;
out_unlock:
	mutex_unlock(&scst_mutex);
out:
	return res;
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
	struct scst_target_group *tg;
	struct scst_target_group_dev *tgdev;
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

	mutex_lock(&scst_mutex);

	list_for_each_entry(tg, &scst_target_group_list, entry) {
		/* Target port group descriptor header. */
		ret_data_len += 8;
		list_for_each_entry(tgt, &tg->tgt_list, target_group_entry) {
			/* Target port descriptor. */
			ret_data_len += 4;
		}
	}

	*length = ret_data_len + 4;

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

	list_for_each_entry(tg, &scst_target_group_list, entry) {
		tgdev = __lookup_tg_dev(tg, dev);
		if (!tgdev)
			continue;
		/* Target port group descriptor header. */
		*p++ = tgdev->state;
		*p++ = 0xCF; /* T_SUP; O_SUP; U_SUP; S_SUP; AN_SUP; AO_SUP */
		put_unaligned(cpu_to_be16(tg->group_id), (__be16 *)p);
		p += 2;
		p++;      /* reserved */
		*p++ = 2; /* status code: implicit transition */
		p++;      /* vendor specific */
		list_for_each_entry(tgdev, &tg->tgt_list, entry)
			(*p)++; /* target port count */
		p++;
		list_for_each_entry(tgt, &tg->tgt_list, target_group_entry) {
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
