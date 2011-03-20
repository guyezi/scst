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

static struct scst_target_group_dev *
__lookup_tg_dev(struct scst_target_group *tg, struct scst_device *dev)
{
	struct scst_target_group_dev *tgdev;

	list_for_each_entry(tgdev, &tg->dev_list, entry)
		if (tgdev->dev == dev)
			return tgdev;

	return NULL;
}

/**
 * scst_tg_get_group_info() - Build REPORT TARGET GROUPS response.
 * @buf: Pointer to a pointer to which the result buffer pointer will be set.
 * @response_length: Response length.
 * @dev: Pointer to the SCST device for which to obtain group information.
 * @data_format: Three-bit response data format specification.
 */
int scst_tg_get_group_info(void **buf, uint32_t *response_length,
			   struct scst_device *dev, uint8_t data_format)
{
	struct scst_target_group *tg;
	struct scst_target_group_dev *tgdev;
	struct scst_tgt *tgt;
	uint8_t *p;
	int res;

	BUG_ON(!buf);
	BUG_ON(!response_length);

	*response_length = 0;

	/* Return data length. */
	*response_length += 4;

	res = -EINVAL;
	switch (data_format) {
	case 0:
		break;
	case 1:
		/* Extended header */
		*response_length += 4;
		break;
	default:
		goto out;
	}

	mutex_lock(&scst_mutex);

	list_for_each_entry(tg, &scst_target_group_list, entry) {
		/* Target port group descriptor header. */
		*response_length += 8;
		list_for_each_entry(tgt, &tg->tgt_list, target_group_entry) {
			/* Target port descriptor. */
			*response_length += 4;
		}
	}

	res = -ENOMEM;
	*buf = kzalloc(*response_length, GFP_KERNEL);
	if (!*buf)
		goto out_unlock;

	p = *buf;
	/* Return data length. */
	put_unaligned(cpu_to_be32(*response_length), (__be32 *)p);
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

	WARN_ON(p - (uint8_t *)*buf != *response_length);

	res = 0;

out_unlock:
	mutex_unlock(&scst_mutex);
out:
	return res;
}
EXPORT_SYMBOL(scst_tg_get_group_info);
