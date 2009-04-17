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

/* struct kobject *targets; */
/* struct kobject *devices; */
/* struct kobject *sgv; */
/* struct kobject *drivers; */

struct scst_sysfs_root {
	struct kobject kobj;
};

struct scst_sysfs_root *scst_sysfs_root;

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


static ssize_t scst_trace_level_show(struct kobject *kobj, struct kobj_attribute *attr,
				     char *buf)
{
	return sprintf(buf, "stgt show!!\n");
}


static ssize_t scst_trace_level_store(struct kobject *kobj, struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	return count;
}


static ssize_t scst_version_show(struct kobject *kobj, struct kobj_attribute *attr,
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
	__ATTR(threads, S_IRUGO | S_IWUSR, scst_threads_show, scst_threads_store);

struct kobj_attribute scst_trace_level_attr = 
	__ATTR(trace_level, S_IRUGO | S_IWUSR, scst_trace_level_show, scst_trace_level_store);


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
	kfree(scst_sysfs_root);
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

struct sysfs_ops scst_sysfs_root_kobj_ops = {
        .show = scst_show,
        .store = scst_store,
};


static struct kobj_type scst_sysfs_root_ktype = {
	.sysfs_ops = &scst_sysfs_root_kobj_ops,
	.release = scst_sysfs_root_release,
	.default_attrs = scst_sysfs_root_default_attrs,
};

int __init scst_sysfs_init(void)
{
	int retval = 0;

	TRACE_ENTRY();

	scst_sysfs_root = kzalloc(sizeof(*scst_sysfs_root), GFP_KERNEL);
	if(!scst_sysfs_root)
		goto sysfs_root_error;

	retval = kobject_init_and_add(&scst_sysfs_root->kobj,
			&scst_sysfs_root_ktype, NULL, "%s", "scsi_tgt");
	if (retval) 
		goto sysfs_root_kobj_error;

out:	
	TRACE_EXIT_RES(retval);
	return retval;

sysfs_root_kobj_error:
	kfree(scst_sysfs_root);

sysfs_root_error:
	retval = -EINVAL;
	goto out;
}

void __exit scst_sysfs_cleanup(void)
{
	TRACE_ENTRY();

	kobject_put(&scst_sysfs_root->kobj);

	TRACE_EXIT();
	return;
}

