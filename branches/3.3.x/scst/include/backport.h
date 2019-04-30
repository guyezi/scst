#ifndef _SCST_BACKPORT_H_
#define _SCST_BACKPORT_H_

/*
 *  Copyright (C) 2015 - 2018 Western Digital Corporation
 *
 *  Backports of functions introduced in recent kernel versions.
 *
 *  Please keep the functions in this file sorted according to the name of the
 *  header file in which these have been defined.
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

#include <linux/bio.h>
#include <linux/blkdev.h>	/* struct request_queue */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 21, 0)
#include <linux/blk-mq.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0)
#include <linux/bsg-lib.h>	/* struct bsg_job */
#endif
#include <linux/eventpoll.h>
#include <linux/iocontext.h>
#include <linux/scatterlist.h>	/* struct scatterlist */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/stddef.h>	/* sizeof_field() */
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/writeback.h>	/* sync_page_range() */
#include <rdma/ib_verbs.h>
#include <scsi/scsi_cmnd.h>	/* struct scsi_cmnd */
#include <scsi/scsi_transport_fc.h> /* struct fc_bsg_job */

/* <asm-generic/barrier.h> */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
#define smp_mb__after_atomic_inc smp_mb__after_atomic
#define smp_mb__after_clear_bit smp_mb__after_atomic
#define smp_mb__before_atomic_dec smp_mb__before_atomic
#define smp_mb__after_atomic_dec smp_mb__after_atomic
#endif

/* <asm-generic/bug.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27) && !defined(WARN)
/* See also commit a8f18b909c0a3f22630846207035c8b84bb252b8 */
#define WARN(condition, format...) do {		\
	if (unlikely(condition)) {		\
		printk(KERN_WARNING format);	\
		WARN_ON(true);			\
	}					\
} while (0)
#endif

/* <asm-generic/fcntl.h> */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 32)
#ifndef O_DSYNC
#define O_DSYNC O_SYNC
#endif
#endif

/* <asm/msr.h> */

#ifdef CONFIG_X86
#include <asm/msr.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0) && !defined(RHEL_RELEASE_CODE)
static __always_inline unsigned long long rdtsc(void)
{
	return native_read_tsc();
}
#endif
#else
static __always_inline unsigned long long rdtsc(void)
{
	return 0;
}
#define tsc_khz 1000
#endif

/* <linux/bio.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
static inline struct bio_set *bioset_create_backport(unsigned int pool_size,
						     unsigned int front_pad,
						     int flags)
{
	WARN_ON_ONCE(flags != 0);
	return bioset_create(pool_size, front_pad);
}
#define bioset_create bioset_create_backport
#define BIOSET_NEED_BVECS 0
#endif

/* See also commit 74d46992e0d9. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static inline void bio_set_dev(struct bio *bio, struct block_device *bdev)
{
	bio->bi_bdev = bdev;
}
#endif

/* <linux/blkdev.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
static inline unsigned int queue_max_hw_sectors(struct request_queue *q)
{
	return q->max_hw_sectors;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
/* See also commit ac481c20ef8f ("block: Topology ioctls") # v2.6.32 */
static inline int bdev_io_opt(struct block_device *bdev)
{
	return 0;
}
#endif

/* <linux/blk-mq.h> */

static inline unsigned int scst_blk_rq_cpu(struct request *rq)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
	/*
	 * See also commit c7c22e4d5c1f ("block: add support for IO CPU
	 * affinity") # v2.6.28.
	 */
	return 0;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 21, 0)
	return rq->cpu;
#else
	return blk_mq_rq_cpu(rq);
#endif
}

/* <linux/bsg-lib.h> */

/*
 * Note: the function bsg_job_sense() exists only in SCST but not in any
 * upstream kernel.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31)
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0) &&	\
     !defined(CONFIG_SUSE_KERNEL)) ||			\
    (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0) &&	\
     defined(CONFIG_SUSE_KERNEL))
static inline void *bsg_job_sense(struct fc_bsg_job *job)
{
	return job->req->sense;
}
#else
static inline void *bsg_job_sense(struct bsg_job *job)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	return job->req->sense;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
	return scsi_req(job->req)->sense;
#else
	return scsi_req(blk_mq_rq_from_pdu(job))->sense;
#endif
}
#endif
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 31) */

/* <linux/compiler.h> */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 20)
#ifndef __printf
#define __printf(a, b) __attribute__((format(printf, a, b)))
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 21)
#ifndef __aligned
#define __aligned(x) __attribute__((aligned(x)))
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0) && !defined(READ_ONCE)
/*
 * See also patch "kernel: Provide READ_ONCE and ASSIGN_ONCE" (commit ID
 * 230fa253df6352af12ad0a16128760b5cb3f92df).
 */
#define READ_ONCE(x) (*(volatile typeof(x) *)&(x))

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
#define ACCESS_ONCE(x) READ_ONCE(x)
#endif

#endif

/* <linux/cpumask.h> */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 20) && !defined(BACKPORT_LINUX_CPUMASK_H)
#define nr_cpu_ids NR_CPUS
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28) && defined(__LINUX_CPUMASK_H)
/*
 * See also patch "cpumask: introduce new API, without changing anything"
 * (commit ID 2d3854a37e8b).
 */
typedef cpumask_t cpumask_var_t[1];
#define cpumask_bits(maskp) ((maskp)->bits)
#ifdef CONFIG_CPUMASK_OFFSTACK
/*
 * Assuming NR_CPUS is huge, a runtime limit is more efficient.  Also,
 * not all bits may be allocated.
 */
#define nr_cpumask_bits nr_cpu_ids
#else
#define nr_cpumask_bits NR_CPUS
#endif

#ifdef CONFIG_CPUMASK_OFFSTACK
bool alloc_cpumask_var(cpumask_var_t *mask, gfp_t flags);
void free_cpumask_var(cpumask_var_t mask);
#else
static inline void free_cpumask_var(cpumask_var_t mask)
{
}

static inline bool alloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return true;
}
#endif

/* verify cpu argument to cpumask_* operators */
static inline unsigned int cpumask_check(unsigned int cpu)
{
#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	WARN_ON_ONCE(cpu >= nr_cpumask_bits);
#endif /* CONFIG_DEBUG_PER_CPU_MAPS */
	return cpu;
}

/**
 * cpumask_next - get the next cpu in a cpumask
 * @n: the cpu prior to the place to search (ie. return will be > @n)
 * @srcp: the cpumask pointer
 *
 * Returns >= nr_cpu_ids if no further cpus set.
 */
static inline unsigned int cpumask_next(int n, const cpumask_t *srcp)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_bit(cpumask_bits(srcp), nr_cpumask_bits, n+1);
}

/**
 * for_each_cpu - iterate over every cpu in a mask
 * @cpu: the (optionally unsigned) integer iterator
 * @mask: the cpumask pointer
 *
 * After the loop, cpu is >= nr_cpu_ids.
 */
#define for_each_cpu(cpu, mask)                         \
	for ((cpu) = -1;                                \
		(cpu) = cpumask_next((cpu), (mask)),    \
		(cpu) < nr_cpu_ids;)

/**
 * cpumask_set_cpu - set a cpu in a cpumask
 * @cpu: cpu number (< nr_cpu_ids)
 * @dstp: the cpumask pointer
 */
static inline void cpumask_set_cpu(unsigned int cpu, cpumask_t *dstp)
{
	set_bit(cpu, cpumask_bits(dstp));
}

/**
 * cpumask_copy - *dstp = *srcp
 * @dstp: the result
 * @srcp: the input cpumask
 */
static inline void cpumask_copy(cpumask_t *dstp,
				const cpumask_t *srcp)
{
	bitmap_copy(cpumask_bits(dstp), cpumask_bits(srcp), nr_cpumask_bits);
}

/**
 * cpumask_setall - set all cpus (< nr_cpu_ids) in a cpumask
 * @dstp: the cpumask pointer
 */
static inline void cpumask_setall(cpumask_t *dstp)
{
	bitmap_fill(cpumask_bits(dstp), nr_cpumask_bits);
}

/**
 * cpumask_equal - *src1p == *src2p
 * @src1p: the first input
 * @src2p: the second input
 */
static inline bool cpumask_equal(const cpumask_t *src1p,
				 const cpumask_t *src2p)
{
	return bitmap_equal(cpumask_bits(src1p), cpumask_bits(src2p),
			    nr_cpumask_bits);
}
#endif

/* <linux/dlm.h> */

/* See also commit 0f8e0d9a317406612700426fad3efab0b7bbc467 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
enum {
	DLM_LSFL_NEWEXCL = 0
};
#endif

/* <linux/eventpoll.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
/*
 * See also commit 65aaf87b3aa2 ("add EPOLLNVAL, annotate EPOLL... and
 * event_poll->event").
 */
typedef unsigned int __poll_t;
#define EPOLLNVAL	POLLNVAL
#endif

/*
 * See also commit 7e040726850a ("eventpoll.h: add missing epoll event masks").
 * Note: this commit got backported to multiple stable kernels, including
 * v3.18.93.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
#if !defined(EPOLLIN)
#define EPOLLIN		POLLIN
#endif
#if !defined(EPOLLPRI)
#define EPOLLPRI	POLLPRI
#endif
#if !defined(EPOLLOUT)
#define EPOLLOUT	POLLOUT
#endif
#if !defined(EPOLLERR)
#define EPOLLERR	POLLERR
#endif
#if !defined(EPOLLHUP)
#define EPOLLHUP	POLLHUP
#endif
#if !defined(EPOLLRDNORM)
#define EPOLLRDNORM	POLLRDNORM
#endif
#if !defined(EPOLLRDBAND)
#define EPOLLRDBAND	POLLRDBAND
#endif
#if !defined(EPOLLWRNORM)
#define EPOLLWRNORM	POLLWRNORM
#endif
#if !defined(EPOLLWRBAND)
#define EPOLLWRBAND	POLLWRBAND
#endif
#if !defined(EPOLLMSG)
#define EPOLLMSG	POLLMSG
#endif
#if !defined(EPOLLRDHUP)
#define EPOLLRDHUP	POLLRDHUP
#endif
#endif

/* <linux/fs.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0) && \
	!defined(CONFIG_COMPAT_KERNEL_3_12)
/*
 * See also patch "new helper: file_inode(file)" (commit ID
 * 496ad9aa8ef448058e36ca7a787c61f2e63f0f54). See also patch
 * "kill f_dentry macro" (commit ID 78d28e651f97).
 */
static inline struct inode *file_inode(const struct file *f)
{
	return f->f_dentry->d_inode;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static inline int vfs_fsync_backport(struct file *file, int datasync)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
	struct inode *inode = file_inode(file);

	return sync_page_range(inode, file->f_mapping, 0, i_size_read(inode));
#else
	return vfs_fsync(file, file->f_path.dentry, datasync);
#endif
}

#define vfs_fsync vfs_fsync_backport
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
/* See also commit dde0c2e79848 ("fs: add IOCB_SYNC and IOCB_DSYNC") */
#define IOCB_DSYNC 0
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0) && \
	LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
/*
 * See also commit bb7462b6fd64 ("vfs: use helpers for calling
 * f_op->{read,write}_iter()").
 */
static inline ssize_t call_read_iter(struct file *file, struct kiocb *kio,
				     struct iov_iter *iter)
{
	return file->f_op->read_iter(kio, iter);
}

static inline ssize_t call_write_iter(struct file *file, struct kiocb *kio,
				      struct iov_iter *iter)
{
	return file->f_op->write_iter(kio, iter);
}
#endif

/* See also commit bdd1d2d3d251 ("fs: fix kernel_read prototype") # v4.14 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static inline ssize_t
kernel_read_backport(struct file *file, void *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, *pos, buf, count);
}

#define kernel_read(file, buf, count, pos)			\
	kernel_read_backport((file), (buf), (count), (pos))
#endif

/* <linux/iocontext.h> */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25) || \
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 21, 0)

static inline struct io_context *
scst_get_task_io_context(struct task_struct *task,
			 gfp_t gfp_flags, int node)
{
	return NULL;
}

static inline void scst_put_io_context(struct io_context *ioc)
{
}

static inline void scst_ioc_task_link(struct io_context *ioc)
{
}

#define get_task_io_context scst_get_task_io_context
#define put_io_context scst_put_io_context
#define ioc_task_link scst_ioc_task_link

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25) && \
	LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
static inline struct io_context *get_task_io_context(struct task_struct *task,
						     gfp_t gfp_flags, int node)
{
	WARN_ON_ONCE(task != current);
	return get_io_context(gfp_flags, node);
}
#endif

/* <linux/kernel.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
#ifndef RHEL_RELEASE_CODE
typedef _Bool bool;
#endif
#define true  1
#define false 0
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
#ifndef swap
#define swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35) &&	\
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 6 ||	\
	 RHEL_MAJOR -0 == 6 && RHEL_MINOR -0 < 1)
extern int hex_to_bin(char ch);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 38)
/*
 * See also "lib: hex2bin converts ascii hexadecimal string to binary" (commit
 * dc88e46029486ed475c71fe1bb696d39511ac8fe).
 */
static inline void hex2bin(u8 *dst, const char *src, size_t count)
{
	while (count--) {
		*dst = hex_to_bin(*src++) << 4;
		*dst += hex_to_bin(*src++);
		dst++;
	}
}
#endif

/*
 * See also commit 33ee3b2e2eb9. That commit was introduced in kernel v2.6.39
 * and later backported to kernel v2.6.38.4.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39) &&		\
	LINUX_VERSION_CODE != KERNEL_VERSION(2, 6, 38) &&	\
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 6)
static inline int __must_check kstrtoull(const char *s, unsigned int base,
					 unsigned long long *res)
{
	return strict_strtoull(s, base, res);
}

static inline int __must_check kstrtoll(const char *s, unsigned int base,
					long long *res)
{
	return strict_strtoll(s, base, res);
}

static inline int __must_check kstrtoul(const char *s, unsigned int base,
					unsigned long *res)
{
	return strict_strtoul(s, base, res);
}

static inline int __must_check kstrtol(const char *s, unsigned int base,
				       long *res)
{
	return strict_strtol(s, base, res);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
/* Suboptimal algorithm for computing the square root of a 64-bit number */
static inline u32 int_sqrt64(u64 x)
{
	u32 r = 0, s;
	int i;

	for (i = 8 * sizeof(r) - 2; i >= 0; i--) {
		s = r + (1 << i);
		if (1ull * s * s <= x)
			r = s;
	}

	return r;
}
#endif

/* <linux/kmod.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
enum umh_wait {
	UMH_NO_WAIT = -1,       /* don't wait at all */
	UMH_WAIT_EXEC = 0,      /* wait for the exec, but not the process */
	UMH_WAIT_PROC = 1,      /* wait for the process to complete */
};
#endif

/* <linux/kref.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0) &&		      \
	!(LINUX_VERSION_CODE >> 8 == KERNEL_VERSION(3, 4, 0) >> 8 &&  \
	  LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 41)) &&	      \
	!(LINUX_VERSION_CODE >> 8 == KERNEL_VERSION(3, 2, 0) >> 8 &&  \
	  LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 44)) &&	      \
	(!defined(CONFIG_SUSE_KERNEL) ||			      \
	 LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 101)) &&	      \
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 6 ||		      \
	 (RHEL_MAJOR -0 == 6 && RHEL_MINOR -0 < 6))
/*
 * See also commit 4b20db3 (kref: Implement kref_get_unless_zero v3 -- v3.8).
 * See also commit e3a5505 in branch stable/linux-3.4.y (v3.4.41).
 * See also commit 3fa8ee5 in branch stable/linux-3.2.y (v3.2.44).
 * See also commit 6b9508d in the SuSE kernel tree.
 */
static inline int __must_check kref_get_unless_zero(struct kref *kref)
{
	return atomic_add_unless(&kref->refcount, 1, 0);
}
#endif

/* See also commit 2c935bc57221 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
#define kref_read(kref) (atomic_read(&(kref)->refcount))
#endif

/* <linux/kthread.h> */

/* See also commit 207205a2ba26 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39) && \
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 6 || \
	 RHEL_MAJOR -0 == 6 && RHEL_MINOR -0 < 9)
#define kthread_create_on_node(threadfn, data, node, namefmt, arg...)\
	kthread_create((threadfn), (data), (namefmt), ##arg)
#endif

/* <linux/ktime.h> */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0) &&		\
	LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)) &&	\
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 7)
/**
 * ktime_before - Compare if a ktime_t value is smaller than another one.
 * @cmp1:	comparable1
 * @cmp2:	comparable2
 *
 * Return: true if cmp1 happened before cmp2.
 */
static inline bool ktime_before(const ktime_t cmp1, const ktime_t cmp2)
{
	return ktime_compare(cmp1, cmp2) < 0;
}
#endif

/* <linux/list.h> */

#ifndef __list_for_each
/* ToDo: cleanup when both are the same for all relevant kernels */
#define __list_for_each list_for_each
#endif

/*
 * Returns true if entry is in its list. Entry must be deleted from the
 * list by using list_del_init()!
 */
static inline bool list_entry_in_list(const struct list_head *entry)
{
	return !list_empty(entry);
}

/* <linux/lockdep.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#define lockdep_assert_held(l) (void)(l)
#endif

/* <linux/kernel.h> */

#if LINUX_VERSION_CODE >> 8 == KERNEL_VERSION(4, 4, 0) >> 8 &&	\
	LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 168)
/*
 * See also commit 8e50b8b07f46 ("mm: replace get_user_pages() write/force
 * parameters with gup_flags") # v4.4.168.
 */
static inline long get_user_pages_backport(unsigned long start,
					   unsigned long nr_pages,
					   unsigned int gup_flags,
					   struct page **pages,
					   struct vm_area_struct **vmas)
{
	return get_user_pages(current, current->mm, start, nr_pages, gup_flags,
			      pages, vmas);
}
#define get_user_pages get_user_pages_backport
#elif !defined(CONFIG_SUSE_KERNEL) &&				\
	LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0)
/*
 * See also commit cde70140fed8 ("mm/gup: Overload get_user_pages() functions")
 * # v4.6.
 */
static inline long get_user_pages_backport(unsigned long start,
					   unsigned long nr_pages,
					   unsigned int gup_flags,
					   struct page **pages,
					   struct vm_area_struct **vmas)
{
	const bool write = gup_flags & FOLL_WRITE;
	const bool force = 0;

	WARN_ON_ONCE(gup_flags & ~FOLL_WRITE);
	return get_user_pages(current, current->mm, start, nr_pages, write,
			      force, pages, vmas);
}
#define get_user_pages get_user_pages_backport
#elif (!defined(CONFIG_SUSE_KERNEL) &&				\
	LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)) ||	\
	LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
/*
 * See also commit 768ae309a961 ("mm: replace get_user_pages() write/force
 * parameters with gup_flags") # v4.9.
 */
static inline long get_user_pages_backport(unsigned long start,
					   unsigned long nr_pages,
					   unsigned int gup_flags,
					   struct page **pages,
					   struct vm_area_struct **vmas)
{
	const bool write = gup_flags & FOLL_WRITE;
	const bool force = 0;

	WARN_ON_ONCE(gup_flags & ~FOLL_WRITE);
	return get_user_pages(start, nr_pages, write, force, pages, vmas);
}
#define get_user_pages get_user_pages_backport
#endif

/* <linux/preempt.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37)
/*
 * See also patch "sched: Fix softirq time accounting" (commit ID
 * 75e1056f5c57050415b64cb761a3acc35d91f013).
 */
#ifndef in_serving_softirq
#define in_serving_softirq() in_softirq()
#endif
#endif

/* <linux/printk.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24) && !defined(RHEL_MAJOR)
#define KERN_CONT       ""
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
/*
 * See also the following commits:
 * d091c2f5 - Introduction of pr_info() etc. in <linux/kernel.h>.
 * 311d0761 - Introduction of pr_cont() in <linux/kernel.h>.
 * 968ab183 - Moved pr_info() etc. from <linux/kernel.h> to <linux/printk.h>
 */
#ifndef pr_emerg

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define pr_emerg(fmt, ...)	printk(KERN_EMERG pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...)	printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...)	printk(KERN_CRIT pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...)	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...)	printk(KERN_WARNING pr_fmt(fmt), ##__VA_ARGS__)
#define pr_notice(fmt, ...)	printk(KERN_NOTICE pr_fmt(fmt), ##__VA_ARGS__)

#endif /* pr_emerg */

#ifndef pr_info
#define pr_info(fmt, ...)	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
#ifndef pr_cont
#define pr_cont(fmt, ...)	printk(KERN_CONT fmt, ##__VA_ARGS__)
#endif
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) */

/* See also commit f036be96dd9c ("printk: introduce printk_once()") */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
#define printk_once(fmt, ...)					\
({								\
	static bool __print_once __read_mostly;			\
	bool __ret_print_once = !__print_once;			\
								\
	if (!__print_once) {					\
		__print_once = true;				\
		printk(fmt, ##__VA_ARGS__);			\
	}							\
	unlikely(__ret_print_once);				\
})
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
/*
 * See also patch "kernel.h: add pr_warn for symmetry to dev_warn,
 * netdev_warn" (commit fc62f2f19edf46c9bdbd1a54725b56b18c43e94f).
 */
#ifndef pr_warn
#define pr_warn pr_warning
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36) && \
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 6)
/*
 * See also patch "Add a dummy printk function for the maintenance of unused
 * printks" (commit 12fdff3fc2483f906ae6404a6e8dcf2550310b6f).
 */
static inline __attribute__ ((format (printf, 1, 2)))
int no_printk(const char *s, ...) { return 0; }
#endif

/* <linux/ratelimit.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
/* See also commit 717115e1a585 */

#define DEFAULT_RATELIMIT_INTERVAL (5 * HZ)
#define DEFAULT_RATELIMIT_BURST 10

struct ratelimit_state {
	int interval;
	int burst;
};

#define DEFINE_RATELIMIT_STATE(name, interval, burst)	\
	struct ratelimit_state name = {interval, burst,}

static inline int __ratelimit(struct ratelimit_state *rs)
{
	return 1;
}
#endif

/* <linux/rcupdate.h> */

/* See also commit b62730baea32 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34)
#define rcu_dereference_protected(p, c) rcu_dereference(p)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0) && !defined(kfree_rcu)
typedef void (*rcu_callback_t)(struct rcu_head *);
#define __is_kfree_rcu_offset(offset) ((offset) < 4096)
#define kfree_call_rcu(head, rcb) call_rcu(head, rcb)
#define __kfree_rcu(head, offset)				\
	do {							\
		BUILD_BUG_ON(!__is_kfree_rcu_offset(offset));	\
		kfree_call_rcu(head, (rcu_callback_t)(unsigned long)(offset)); \
	} while (0)
#define kfree_rcu(ptr, rcu_head)				\
	__kfree_rcu(&((ptr)->rcu_head), offsetof(typeof(*(ptr)), rcu_head))
#endif

/* <rdma/ib_verbs.h> */
/* commit ed082d36 */
#ifndef ib_alloc_pd
static inline struct ib_pd *ib_alloc_pd_backport(struct ib_device *device)
{
	return ib_alloc_pd(device);
}
#define ib_alloc_pd(device, flags)				\
	({							\
		(void)(flags), ib_alloc_pd_backport(device);	\
	})
#endif

/* <linux/sched.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26) && \
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 6)
#define set_cpus_allowed_ptr(p, new_mask) set_cpus_allowed((p), *(new_mask))
#endif

/* <linux/scatterlist.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
/*
 * The macro's sg_page(), sg_virt(), sg_init_table(), sg_assign_page() and
 * sg_set_page() have been introduced in the 2.6.24 kernel. The definitions
 * below are backports of the 2.6.24 macro's for older kernels. There is one
 * exception however: when compiling SCST on a system with a pre-2.6.24 kernel
 * (e.g. RHEL 5.x) where the OFED kernel headers have been installed, do not
 * define the backported macro's because OFED has already defined these.
 */

static inline bool sg_is_chain(struct scatterlist *sg)
{
	return false;
}

static inline struct scatterlist *sg_chain_ptr(struct scatterlist *sg)
{
	return NULL;
}

#define sg_is_last(sg) false

#ifndef sg_page
static inline struct page *sg_page(struct scatterlist *sg)
{
	return sg->page;
}
#endif

static inline void *sg_virt(struct scatterlist *sg)
{
	return page_address(sg_page(sg)) + sg->offset;
}

static inline void sg_mark_end(struct scatterlist *sg)
{
}

#ifndef __BACKPORT_LINUX_SCATTERLIST_H_TO_2_6_23__

static inline void sg_init_table(struct scatterlist *sgl, unsigned int nents)
{
	memset(sgl, 0, sizeof(*sgl) * nents);
}

static inline void sg_assign_page(struct scatterlist *sg, struct page *page)
{
	sg->page = page;
}

static inline void sg_set_page(struct scatterlist *sg, struct page *page,
			       unsigned int len, unsigned int offset)
{
	sg_assign_page(sg, page);
	sg->offset = offset;
	sg->length = len;
}

#ifndef for_each_sg
/* See also commit 96b418c960af0d5c7185ff5c4af9376eb37ac9d3 */
#define for_each_sg(sglist, sg, nr, __i)       \
	for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next_inline(sg))
#endif /* for_each_sg */

#endif /* __BACKPORT_LINUX_SCATTERLIST_H_TO_2_6_23__ */
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24) */

/* <linux/slab.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)
#define KMEM_CACHE(__struct, __flags) kmem_cache_create(#__struct,\
	sizeof(struct __struct), __alignof__(struct __struct),\
	(__flags), NULL, NULL)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0) &&	    \
	!(LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 52) && \
	  LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)) &&  \
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 6)
static inline void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	if (size != 0 && n > ULONG_MAX / size)
		return NULL;
	return kmalloc(n * size, flags);
}
#endif

/*
 * See also commit 8eb8284b4129 ("usercopy: Prepare for usercopy
 * whitelisting").
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23)
static inline struct kmem_cache *kmem_cache_create_usercopy(const char *name,
			unsigned int size, unsigned int align,
			unsigned long flags,
			unsigned int useroffset, unsigned int usersize,
			void (*ctor)(void *))
{
	return kmem_cache_create(name, size, align, flags, ctor, NULL);
}
/* UEKR5 is based on kernel v4.14.35 but has a backport of the v4.16 API. */
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0) && !defined(UEK_KABI_RENAME)
static inline struct kmem_cache *kmem_cache_create_usercopy(const char *name,
			unsigned int size, unsigned int align,
			unsigned long flags,
			unsigned int useroffset, unsigned int usersize,
			void (*ctor)(void *))
{
	return kmem_cache_create(name, size, align, flags, ctor);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)
#define KMEM_CACHE_USERCOPY(__struct, __flags, __field)			\
		kmem_cache_create_usercopy(#__struct,			\
			sizeof(struct __struct),			\
			__alignof__(struct __struct), (__flags),	\
			offsetof(struct __struct, __field),		\
			sizeof_field(struct __struct, __field), NULL)
#endif

/* <linux/t10-pi.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)
struct t10_pi_tuple {
	__be16 guard_tag;
	__be16 app_tag;
	__be32 ref_tag;
};
#endif

/* <linux/stddef.h> */

#ifndef sizeof_field
#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))
#endif

/* <linux/timer.h> */

/*
 * See also commit 686fef928bba ("timer: Prepare to change timer callback
 * argument type"). See also commit 0eeda71bc30d ("timer: Replace timer base
 * by a cpu index").
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
#define timer_setup(_timer, _fn, _flags) do {			\
	init_timer(_timer);					\
	(_timer)->function = (void *)(_fn);	\
	(_timer)->data = (unsigned long)(_timer);		\
	WARN_ON_ONCE((_flags) != 0);				\
} while (0)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#define timer_setup(_timer, _fn, _flags) do {			\
	init_timer(_timer);					\
	(_timer)->function = (void *)(_fn);\
	(_timer)->data = (unsigned long)(_timer);		\
	(_timer)->flags = (_flags);				\
} while (0)
#endif

/*
 * See also commit 1d27e3e2252b ("timer: Remove expires and data arguments
 * from DEFINE_TIMER").
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#undef DEFINE_TIMER
#define DEFINE_TIMER(_name, _function)					\
	struct timer_list _name = TIMER_INITIALIZER(			\
		(void (*)(unsigned long))(unsigned long)(_function), 0,	\
		(unsigned long)&(_name))
#endif

/* <linux/types.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
/*
 * See also patch "fix abuses of ptrdiff_t" (commit ID
 * 142956af525002c5378e7d91d81a01189841a785).
 */
typedef unsigned long uintptr_t;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22)
char *kvasprintf(gfp_t gfp, const char *fmt, va_list ap);
#endif

/* <linux/unaligned.h> */

#if defined(RHEL_MAJOR) && RHEL_MAJOR -0 <= 5
static inline uint16_t get_unaligned_be16(const void *p)
{
	return be16_to_cpu(get_unaligned((__be16 *)p));
}

static inline void put_unaligned_be16(uint16_t i, void *p)
{
	put_unaligned(cpu_to_be16(i), (__be16 *)p);
}

static inline uint32_t get_unaligned_be32(const void *p)
{
	return be32_to_cpu(get_unaligned((__be32 *)p));
}

static inline void put_unaligned_be32(uint32_t i, void *p)
{
	put_unaligned(cpu_to_be32(i), (__be32 *)p);
}

static inline uint64_t get_unaligned_be64(const void *p)
{
	return be64_to_cpu(get_unaligned((__be64 *)p));
}

static inline void put_unaligned_be64(uint64_t i, void *p)
{
	put_unaligned(cpu_to_be64(i), (__be64 *)p);
}
#endif

/* <linux/vmalloc.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 37) && \
	(!defined(RHEL_MAJOR) || RHEL_MAJOR -0 < 5 || \
	 RHEL_MAJOR -0 == 5 && RHEL_MINOR -0 < 10 || \
	 RHEL_MAJOR -0 == 6 && RHEL_MINOR -0 < 1)
/*
 * See also patch "mm: add vzalloc() and vzalloc_node() helpers" (commit
 * e1ca7788dec6773b1a2bce51b7141948f2b8bccf).
 */
static inline void *vzalloc(unsigned long size)
{
	return __vmalloc(size, GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
			 PAGE_KERNEL);
}
#endif

/* <scsi/scsi_cmnd.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
/*
 * See also patch "[SCSI] bidirectional command support"
 * (commit ID 6f9a35e2dafa).
 */
static inline int scsi_bidi_cmnd(struct scsi_cmnd *cmd)
{
	return false;
}
#endif

/* <scsi/scsi_request.h> */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
static inline struct request *scsi_req(struct request *rq)
{
	return rq;
}

static inline void scsi_req_init(struct request *rq)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)
	rq->cmd_type = REQ_TYPE_BLOCK_PC;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 31)
	rq->data_len = 0;
	rq->sector = (sector_t) -1;
#else
	rq->__data_len = 0;
	rq->__sector = (sector_t) -1;
#endif
	rq->bio = rq->biotail = NULL;
	memset(rq->__cmd, 0, sizeof(rq->__cmd));
	rq->cmd = rq->__cmd;
#else
	return blk_rq_set_block_pc(rq);
#endif
}
#endif

#endif /* _SCST_BACKPORT_H_ */
