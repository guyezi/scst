#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ctype.h>

#include "scst.h"
#include "scst_priv.h"
#include "scst_mem.h"

#define SCST_SYSFS_BLOCK_SIZE (PAGE_SIZE - 512)

static DEFINE_MUTEX(scst_sysfs_mutex);

static DECLARE_COMPLETION(scst_sysfs_root_release_completion);

static struct kobject scst_sysfs_root_kobj;
static struct kobject *scst_targets_kobj;
static struct kobject *scst_devices_kobj;
static struct kobject *scst_sgv_kobj;
static struct kobject *scst_back_drivers_kobj;

static struct sysfs_ops scst_sysfs_ops;

static void scst_sysfs_release(struct kobject *kobj)
{
	kfree(kobj);
}

int scst_create_tgtt_sysfs(struct scst_tgt_template *tgtt)
{
	int retval = 0;

	TRACE_ENTRY();

	tgtt->tgtt_kobj = kobject_create_and_add(tgtt->name, scst_targets_kobj);
	if (!tgtt->tgtt_kobj)
		retval = -EINVAL;

	TRACE_EXIT_RES(retval);
	return retval;
}

void scst_cleanup_tgtt_sysfs(struct scst_tgt_template *tgtt)
{
	kobject_del(tgtt->tgtt_kobj);
	kobject_put(tgtt->tgtt_kobj);
}

static void scst_tgt_free(struct kobject *kobj)
{
	struct scst_tgt *tgt;

	TRACE_ENTRY();

	tgt = container_of(kobj, struct scst_tgt, tgt_kobj);

	kfree(tgt);

	TRACE_EXIT();
	return;
}

static struct kobj_type tgt_ktype = {
	.release = scst_tgt_free,
};

int scst_create_tgt_sysfs(struct scst_tgt *tgt)
{
	int retval;

	TRACE_ENTRY();

	retval = kobject_init_and_add(&tgt->tgt_kobj, &tgt_ktype,
			tgt->tgtt->tgtt_kobj, tgt->tgt_name);
	if (retval != 0) {
		PRINT_ERROR("Can't add tgt %s to sysfs", tgt->tgt_name);
		goto out;
	}

	tgt->tgt_sess_kobj = kobject_create_and_add("sessions", &tgt->tgt_kobj);
	if (!tgt->tgt_sess_kobj) {
		PRINT_ERROR("Can't create sess kobj for tgt %s", tgt->tgt_name);
		goto out_sess_obj_err;
	}

	tgt->tgt_luns_kobj = kobject_create_and_add("luns", &tgt->tgt_kobj);
	if (!tgt->tgt_luns_kobj) {
		PRINT_ERROR("Can't create luns kobj for tgt %s", tgt->tgt_name);
		goto luns_kobj_err;
	}

	tgt->tgt_ini_grp_kobj = kobject_create_and_add("ini_group",
						    &tgt->tgt_kobj);
	if (!tgt->tgt_ini_grp_kobj) {
		PRINT_ERROR("Can't create ini_grp kobj for tgt %s",
			tgt->tgt_name);
		goto ini_grp_kobj_err;
	}

out:
	TRACE_EXIT_RES(retval);
	return retval;

ini_grp_kobj_err:
	kobject_del(tgt->tgt_luns_kobj);
	kobject_put(tgt->tgt_luns_kobj);

luns_kobj_err:
	kobject_del(tgt->tgt_sess_kobj);
	kobject_put(tgt->tgt_sess_kobj);

out_sess_obj_err:
	kobject_del(&tgt->tgt_kobj);
	retval = -ENOMEM;
	goto out;
}

/* tgt can be dead upon exit from this function! */
void scst_cleanup_tgt_sysfs_put(struct scst_tgt *tgt)
{
	kobject_del(tgt->tgt_sess_kobj);
	kobject_put(tgt->tgt_sess_kobj);

	kobject_del(tgt->tgt_luns_kobj);
	kobject_put(tgt->tgt_luns_kobj);

	kobject_del(tgt->tgt_ini_grp_kobj);
	kobject_put(tgt->tgt_ini_grp_kobj);

	kobject_del(&tgt->tgt_kobj);
	kobject_put(&tgt->tgt_kobj);
	return;
}

struct kobj_attribute sgv_stat_attr = 
	__ATTR(stats, S_IRUGO, sgv_sysfs_stat_show, NULL);

static struct attribute *sgv_attrs[] = {
	&sgv_stat_attr.attr,
	NULL,
};

static void sgv_release(struct kobject *kobj)
{
	struct sgv_pool *pool;

	TRACE_ENTRY();

	pool = container_of(kobj, struct sgv_pool, sgv_kobj);

	kfree(pool);

	TRACE_EXIT();
	return;
}

static struct kobj_type sgv_pool_ktype = {
	.sysfs_ops = &scst_sysfs_ops,
	.release = sgv_release,
	.default_attrs = sgv_attrs,
};

int scst_create_sgv_sysfs(struct sgv_pool *pool)
{
	int retval;

	TRACE_ENTRY();

	retval = kobject_init_and_add(&pool->sgv_kobj, &sgv_pool_ktype,
			scst_sgv_kobj, pool->name);
	if (retval != 0) {
		PRINT_ERROR("Can't add sgv pool %s to sysfs", pool->name);
		goto out;
	}

out:
	TRACE_EXIT_RES(retval);
	return retval;
}

/* pool can be dead upon exit from this function! */
void scst_cleanup_sgv_sysfs_put(struct sgv_pool *pool)
{
	kobject_del(&pool->sgv_kobj);
	kobject_put(&pool->sgv_kobj);
	return;
}

struct kobj_attribute sgv_global_stat_attr = 
	__ATTR(global_stats, S_IRUGO, sgv_sysfs_global_stat_show, NULL);

static struct attribute *sgv_default_attrs[] = {
	&sgv_global_stat_attr.attr,
	NULL,
};

static struct kobj_type sgv_ktype = {
	.sysfs_ops = &scst_sysfs_ops,
	.release = scst_sysfs_release,
	.default_attrs = sgv_default_attrs,
};

static ssize_t scst_threads_show(struct kobject *kobj, struct kobj_attribute *attr,
				 char *buf)
{
	int count;

	TRACE_ENTRY();

	count = sprintf(buf, "%d\n", scst_global_threads_count());

	TRACE_EXIT();
	return count;
}

static ssize_t scst_threads_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	int res = count;
	int oldtn, newtn, delta;

	TRACE_ENTRY();

	if (count > SCST_SYSFS_BLOCK_SIZE) {
		res = -EOVERFLOW;
		goto out;
	}

	if (mutex_lock_interruptible(&scst_sysfs_mutex) != 0) {
		res = -EINTR;
		goto out;
	}

	mutex_lock(&scst_global_threads_mutex);

	oldtn = scst_nr_global_threads;
	sscanf(buf, "%du", &newtn);

	if (newtn <= 0) {
		PRINT_ERROR("Illegal threads num value %d", newtn);
		res = -EINVAL;
		goto out_up_thr_free;
	}
	delta = newtn - oldtn;
	if (delta < 0)
		__scst_del_global_threads(-delta);
	else
		__scst_add_global_threads(delta);

	PRINT_INFO("Changed cmd threads num: old %d, new %d", oldtn, newtn);

out_up_thr_free:
	mutex_unlock(&scst_global_threads_mutex);

	mutex_unlock(&scst_sysfs_mutex);

out:
	TRACE_EXIT_RES(res);
	return res;
}

static ssize_t scst_trace_level_show(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	return sprintf(buf, "trace level show!!\n");
}

static ssize_t scst_trace_level_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	return count;
}

static ssize_t scst_version_show(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 char *buf)
{
	TRACE_ENTRY();

	sprintf(buf, "%s\n", SCST_VERSION_STRING);

#ifdef CONFIG_SCST_STRICT_SERIALIZING
	strcat(buf, "Strict serializing enabled\n");
#endif

#ifdef CONFIG_SCST_EXTRACHECKS
	strcat(buf, "EXTRACHECKS\n");
#endif

#ifdef CONFIG_SCST_TRACING
	strcat(buf, "TRACING\n");
#endif

#ifdef CONFIG_SCST_DEBUG
	strcat(buf, "DEBUG\n");
#endif

#ifdef CONFIG_SCST_DEBUG_TM
	strcat(buf, "DEBUG_TM\n");
#endif

#ifdef CONFIG_SCST_DEBUG_RETRY
	strcat(buf, "DEBUG_RETRY\n");
#endif

#ifdef CONFIG_SCST_DEBUG_OOM
	strcat(buf, "DEBUG_OOM\n");
#endif

#ifdef CONFIG_SCST_DEBUG_SN
	strcat(buf, "DEBUG_SN\n");
#endif

#ifdef CONFIG_SCST_USE_EXPECTED_VALUES
	strcat(buf, "USE_EXPECTED_VALUES\n");
#endif

#ifdef CONFIG_SCST_ALLOW_PASSTHROUGH_IO_SUBMIT_IN_SIRQ
	strcat(buf, "ALLOW_PASSTHROUGH_IO_SUBMIT_IN_SIRQ\n");
#endif

#ifdef CONFIG_SCST_STRICT_SECURITY
	strcat(buf, "SCST_STRICT_SECURITY\n");
#endif

	TRACE_EXIT();
	return strlen(buf);
}

struct kobj_attribute scst_threads_attr = 
	__ATTR(threads, S_IRUGO | S_IWUSR, scst_threads_show,
	       scst_threads_store);

struct kobj_attribute scst_trace_level_attr =
	__ATTR(trace_level, S_IRUGO | S_IWUSR, scst_trace_level_show,
	       scst_trace_level_store);

struct kobj_attribute scst_version_attr = 
	__ATTR(version, S_IRUGO, scst_version_show, NULL);
	
static struct attribute *scst_sysfs_root_default_attrs[] = {
	&scst_threads_attr.attr,
	&scst_trace_level_attr.attr,
	&scst_version_attr.attr,
	NULL,
};

static void scst_sysfs_root_release(struct kobject *kobj)
{
	complete_all(&scst_sysfs_root_release_completion);
}

static ssize_t scst_show(struct kobject *kobj, struct attribute *attr,
			 char *buf)
{
	struct kobj_attribute *kobj_attr;
	kobj_attr = container_of(attr, struct kobj_attribute, attr);

	return kobj_attr->show(kobj, kobj_attr, buf);
}

static ssize_t scst_store(struct kobject *kobj, struct attribute *attr,
			  const char *buf, size_t count)
{
	struct kobj_attribute *kobj_attr;
	kobj_attr = container_of(attr, struct kobj_attribute, attr);

	return kobj_attr->store(kobj, kobj_attr, buf, count);
}

static struct sysfs_ops scst_sysfs_ops = {
        .show = scst_show,
        .store = scst_store,
};

static struct kobj_type scst_sysfs_root_ktype = {
	.sysfs_ops = &scst_sysfs_ops,
	.release = scst_sysfs_root_release,
	.default_attrs = scst_sysfs_root_default_attrs,
};


int __init scst_sysfs_init(void)
{
	int retval = 0;

	TRACE_ENTRY();

	retval = kobject_init_and_add(&scst_sysfs_root_kobj,
			&scst_sysfs_root_ktype, kernel_kobj, "%s", "scst_tgt");
	if (retval != 0)
		goto sysfs_root_add_error;

	scst_targets_kobj = kobject_create_and_add("targets",
				&scst_sysfs_root_kobj);
	if (!scst_targets_kobj)
		goto targets_kobj_error;

	scst_devices_kobj = kobject_create_and_add("devices",
				&scst_sysfs_root_kobj);
	if (!scst_devices_kobj)
		goto devices_kobj_error;

	scst_sgv_kobj = kzalloc(sizeof(*scst_sgv_kobj), GFP_KERNEL);
	if (!scst_sgv_kobj)
		goto sgv_kobj_error;

	retval = kobject_init_and_add(scst_sgv_kobj, &sgv_ktype,
			&scst_sysfs_root_kobj, "%s", "sgv");
	if (retval != 0)
		goto sgv_kobj_add_error;

	scst_back_drivers_kobj = kobject_create_and_add("back_drivers",
					&scst_sysfs_root_kobj);
	if (!scst_back_drivers_kobj)
		goto back_drivers_kobj_error;


out:	
	TRACE_EXIT_RES(retval);
	return retval;

back_drivers_kobj_error:
	kobject_del(scst_sgv_kobj);

sgv_kobj_add_error:
	kobject_put(scst_sgv_kobj);

sgv_kobj_error:
	kobject_del(scst_devices_kobj);
	kobject_put(scst_devices_kobj);

devices_kobj_error:
	kobject_del(scst_targets_kobj);
	kobject_put(scst_targets_kobj);

targets_kobj_error:
	kobject_del(&scst_sysfs_root_kobj);

sysfs_root_add_error:
	kobject_put(&scst_sysfs_root_kobj);

	if (retval == 0)
		retval = -EINVAL;
	goto out;
}

void __exit scst_sysfs_cleanup(void)
{
	TRACE_ENTRY();

	PRINT_INFO("%s", "Exiting SCST sysfs hierarchy...");

	kobject_del(scst_sgv_kobj);
	kobject_put(scst_sgv_kobj);

	kobject_del(scst_devices_kobj);
	kobject_put(scst_devices_kobj);

	kobject_del(scst_targets_kobj);
	kobject_put(scst_targets_kobj);

	kobject_del(scst_back_drivers_kobj);
	kobject_put(scst_back_drivers_kobj);

	kobject_del(&scst_sysfs_root_kobj);
	kobject_put(&scst_sysfs_root_kobj);

	wait_for_completion(&scst_sysfs_root_release_completion);

	PRINT_INFO("%s", "Exiting SCST sysfs hierarchy done");

	TRACE_EXIT();
	return;
}
