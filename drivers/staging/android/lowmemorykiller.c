/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_adj values will get killed. Specify the
 * minimum oom_adj values in /sys/module/lowmemorykiller/parameters/adj and the
 * number of free pages in /sys/module/lowmemorykiller/parameters/minfree. Both
 * files take a comma separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill processes
 * with a oom_adj value of 8 or higher when the free memory drops below 4096 pages
 * and kill processes with a oom_adj value of 0 or higher when the free memory
 * drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/notifier.h>

static uint32_t lowmem_debug_level = 2;
static int lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};
static int lowmem_adj_size = 4;
static size_t lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};
static int lowmem_minfree_size = 4;
static size_t lowmem_fork_boost_minfree[6] = {
	0,
	0,
	0,
	0,
	6177,
	6177,
};
static size_t minfree_tmp[6] = {0, 0, 0, 0, 0, 0};

static size_t fork_boost_adj[6] = {
	0,
	2,
	4,
	7,
	9,
	12
};

static struct task_struct *lowmem_deathpending;
static unsigned long lowmem_deathpending_timeout;
static unsigned long lowmem_fork_boost_timeout;
static uint32_t lowmem_fork_boost = 1;
static int last_min_adj = OOM_ADJUST_MAX + 1;;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			printk(x);			\
	} while (0)


static void show_meminfo(void)
{
	printk(" free:%lu \n"
		" active_file:%luK inactive_file:%luK shem:%luK mlock:%luK [cache]\n"
		" anon:%luK mapped:%luK [PSS]\n"
		" slab_reclaimable:%luK slab_unreclaimable:%luK\n"
		" pagetables:%luK\n",
		global_page_state(NR_FREE_PAGES) << 2,
		global_page_state(NR_ACTIVE_FILE) << 2,
		global_page_state(NR_INACTIVE_FILE) << 2,
		global_page_state(NR_SHMEM) << 2,
		global_page_state(NR_MLOCK) << 2,
		(global_page_state(NR_ACTIVE_ANON) + global_page_state(NR_INACTIVE_ANON)) << 2,
		global_page_state(NR_FILE_MAPPED) << 2,
		global_page_state(NR_SLAB_RECLAIMABLE) << 2, global_page_state(NR_SLAB_UNRECLAIMABLE) << 2,
		global_page_state(NR_PAGETABLE) << 2);
}

/**
 * dump_tasks - dump current memory state of all system tasks
 *
 * State information includes task's pid, uid, tgid, vm size, rss, cpu, oom_adj
 * value, oom_score_adj value, and name.
 *
 * Call with tasklist_lock read-locked.
 */
static void dump_tasks(void)
{
	struct task_struct *p;
	struct task_struct *task;

	pr_info("[ pid ]   uid  total_vm      rss cpu oom_adj  name\n");
	for_each_process(p) {
		task = find_lock_task_mm(p);
		if (!task) {
			/*
			 * This is a kthread or all of p's threads have already
			 * detached their mm's.  There's no need to report
			 * them; they can't be oom killed anyway.
			 */
			continue;
		}

		pr_info("[%5d] %5d  %8lu %8lu %3u     %3d  %s\n",
			task->pid, task_uid(task),
			task->mm->total_vm, get_mm_rss(task->mm),
			task_cpu(task), task->signal->oom_adj, task->comm);
		task_unlock(task);
	}
}

static int
task_free_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_free_nb = {
	.notifier_call	= task_free_notify_func,
};

static int
task_free_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	struct task_struct *task = data;

	if (task == lowmem_deathpending)
		lowmem_deathpending = NULL;

	return NOTIFY_OK;
}

static int
task_fork_notify_func(struct notifier_block *self, unsigned long val, void *data);

static struct notifier_block task_fork_nb = {
	.notifier_call  = task_fork_notify_func,
};

static int
task_fork_notify_func(struct notifier_block *self, unsigned long val, void *data)
{
	lowmem_fork_boost_timeout = jiffies + (HZ << 1);

	return NOTIFY_OK;
}

static int lowmem_shrink(struct shrinker *s, int nr_to_scan, gfp_t gfp_mask)
{
	struct task_struct *p;
	struct task_struct *selected = NULL;
	int rem = 0;
	int tasksize;
	int i;
	int min_adj = OOM_ADJUST_MAX + 1;
	int selected_tasksize = 0;
	int selected_oom_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES);
	int other_file = global_page_state(NR_FILE_PAGES) -
		global_page_state(NR_SHMEM) - global_page_state(NR_MLOCK);

	int fork_boost = 0;
	int *adj_array;
	size_t *min_array;

	/*
	 * If we already have a death outstanding, then
	 * bail out right away; indicating to vmscan
	 * that we have nothing further to offer on
	 * this pass.
	 *
	 */
	if (lowmem_deathpending &&
	    time_before_eq(jiffies, lowmem_deathpending_timeout))
		return 0;

	if (lowmem_fork_boost &&
	    time_before_eq(jiffies, lowmem_fork_boost_timeout)) {
		for (i = 0; i < lowmem_minfree_size; i++)
			minfree_tmp[i] = lowmem_minfree[i] + lowmem_fork_boost_minfree[i] ;

		adj_array = fork_boost_adj;
		min_array = minfree_tmp;
	}
	else {
		adj_array = lowmem_adj;
		min_array = lowmem_minfree;
	}

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		if (other_free < min_array[i] &&
		    (other_file < min_array[i])) {
			min_adj = adj_array[i];
			fork_boost = lowmem_fork_boost_minfree[i];
			break;
		}
	}
	if (nr_to_scan > 0)
		lowmem_print(3, "lowmem_shrink %d, %x, ofree %d %d, ma %d\n",
			     nr_to_scan, gfp_mask, other_free, other_file,
			     min_adj);
	rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);
	if (nr_to_scan <= 0 || min_adj == OOM_ADJUST_MAX + 1) {
		lowmem_print(5, "lowmem_shrink %d, %x, return %d\n",
			     nr_to_scan, gfp_mask, rem);
		return rem;
	}
	selected_oom_adj = min_adj;

	read_lock(&tasklist_lock);
	for_each_process(p) {
		struct mm_struct *mm;
		struct signal_struct *sig;
		int oom_adj;

		task_lock(p);
		mm = p->mm;
		sig = p->signal;
		if (!mm || !sig) {
			task_unlock(p);
			continue;
		}
		oom_adj = sig->oom_adj;

		if (oom_adj < min_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(mm);
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_adj < selected_oom_adj)
				continue;
			if (oom_adj == selected_oom_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
		selected_oom_adj = oom_adj;
		lowmem_print(2, "select %d (%s), adj %d, size %d, to kill\n",
			     p->pid, p->comm, oom_adj, tasksize);
	}

	if (selected) {
		if (last_min_adj > selected_oom_adj &&
			(selected_oom_adj == 12 || selected_oom_adj == 9 || selected_oom_adj == 7)) {
			last_min_adj = selected_oom_adj;
			lowmem_print(1, "lowmem_shrink: monitor memory status at selected_oom_adj=%d\n", selected_oom_adj);
			show_meminfo();
			dump_tasks();
		}

		lowmem_print(1, "[%s] send sigkill to %d (%s), adj %d, size %dK, min_adj=%d,"
			" free=%dK, file=%dK, fork_boost=%d\n",
			     current->comm, selected->pid, selected->comm,
			     selected_oom_adj, selected_tasksize << 2, min_adj,
			     other_free << 2, other_file << 2, fork_boost << 2);
		lowmem_deathpending = selected;
		lowmem_deathpending_timeout = jiffies + HZ;
		if (selected_oom_adj < 7)
		{
			show_meminfo();
			dump_tasks();
		}
		force_sig(SIGKILL, selected);
		rem -= selected_tasksize;
	}
	lowmem_print(4, "lowmem_shrink %d, %x, return %d\n",
		     nr_to_scan, gfp_mask, rem);
	read_unlock(&tasklist_lock);
	return rem;
}

static struct shrinker lowmem_shrinker = {
	.shrink = lowmem_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	task_free_register(&task_free_nb);
	task_fork_register(&task_fork_nb);
	register_shrinker(&lowmem_shrinker);
	return 0;
}

static void __exit lowmem_exit(void)
{
	unregister_shrinker(&lowmem_shrinker);
	task_fork_unregister(&task_fork_nb);
	task_free_unregister(&task_free_nb);
}

module_param_named(cost, lowmem_shrinker.seeks, int, S_IRUGO | S_IWUSR);
module_param_array_named(adj, lowmem_adj, int, &lowmem_adj_size,
			 S_IRUGO | S_IWUSR);
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);
module_param_named(debug_level, lowmem_debug_level, uint, S_IRUGO | S_IWUSR);
module_param_named(fork_boost, lowmem_fork_boost, uint, S_IRUGO | S_IWUSR);
module_param_array_named(fork_boost_minfree, lowmem_fork_boost_minfree, uint, &lowmem_minfree_size,
			 S_IRUGO | S_IWUSR);

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");

