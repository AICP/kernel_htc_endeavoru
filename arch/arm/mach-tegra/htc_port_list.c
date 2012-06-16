/* arch/arm/mach-msm/htc_port_list.c
 * Copyright (C) 2009 HTC Corporation.
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

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/list.h>
#include <net/tcp.h>

static struct mutex port_lock;
static struct wake_lock port_suspend_lock;
static uint16_t *port_list = NULL;
static int usb_enable = 0;
struct p_list {
	struct list_head list;
	int no;
};
static struct p_list curr_port_list;
static int packet_filter_flag = 1;
struct class *p_class;
static struct miscdevice portlist_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "htc-portlist",
};

struct htc_portlist_info {
	struct kobject portlist_kobj;
};

static struct htc_portlist_info portlist_info;
static struct kset *htc_portlist_kset;

static void htc_portlist_kobject_release(struct kobject *kobj)
{
	printk(KERN_ERR "htc_portlist_kobject_release.\n");
	return;
}

static struct kobj_type htc_portlist_ktype = {
	.release = htc_portlist_kobject_release,
};


static int ril_debug_flag = 0;

#define PF_LOG_DBG(fmt, ...) do {                           \
		if (ril_debug_flag)                                 \
			printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)

#define PF_LOG_INFO(fmt, ...) do {                         \
		if (ril_debug_flag)                                \
			printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)

#define PF_LOG_ERR(fmt, ...) do {                     \
	if (ril_debug_flag)                               \
		printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)

static void portlist_work_func()
{
	size_t i=0;
	char message[128*6+14] = {'\0'};
	char *envp[] = { message, NULL };
	char* p = message;
	p+=sprintf(p, "PORTLIST=");
	if(port_list[0] != 0){
		p+=sprintf(p, "%d", port_list[0]);
		for(i=1; i<128; i++){
			if(port_list[i] != 0)
				p+=sprintf(p, ",%d", port_list[i]);
			else
				break;
		}
	}
	PF_LOG_INFO("[PORT LIST] message=%s", message);
	kobject_uevent_env(&portlist_info.portlist_kobj, KOBJ_CHANGE, envp);
	return;
}

static ssize_t htc_show(struct device *dev,  struct device_attribute *attr,  char *buf)
{
	char *s = buf;
	mutex_lock(&port_lock);
	s += sprintf(s, "%d\n", packet_filter_flag);
	mutex_unlock(&port_lock);
	return s - buf;
}

static ssize_t htc_store(struct device *dev, struct device_attribute *attr,  const char *buf, size_t count)
{
	int ret;

	mutex_lock(&port_lock);
	if (!strncmp(buf, "0", strlen("0"))) {
		packet_filter_flag = 0;
		PF_LOG_INFO("[Port list] Disable Packet filter\n");
		if (port_list != NULL)
			port_list[0] = packet_filter_flag;
		else
			PF_LOG_ERR("[Port list] port_list == NULL\n");
		ret = count;
	} else if (!strncmp(buf, "1", strlen("1"))) {
		packet_filter_flag = 1;
		PF_LOG_INFO("[Port list] Enable Packet filter\n");
		if (port_list != NULL)
			port_list[0] = packet_filter_flag;
		else
			PF_LOG_ERR("[Port list] port_list == NULL\n");
		ret = count;
	} else {
		PF_LOG_ERR("[Port list] flag: invalid argument\n");
		ret = -EINVAL;
	}
	mutex_unlock(&port_lock);

	return ret;
}

static DEVICE_ATTR(flag, 0664, htc_show, htc_store);

static int port_list_enable(int enable)
{
	if (port_list[0] != enable) {
		port_list[0] = enable;
		if (enable)
			PF_LOG_INFO("[Port list] port_list is enabled.\n");
		else
			PF_LOG_INFO("[Port list] port_list is disabled.\n");
	}
	return 0;
}

static void update_port_list(void)
{
	size_t count = 0;
	size_t i = 0;
	struct list_head *listptr;
	struct p_list *entry;

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		count++;
		PF_LOG_INFO("[Port list] [%d] = %d\n", count, entry->no);
		if (count <= 127)
			port_list[count] = entry->no;
	}
	if (count < 127)
		for (i = count + 1; i <= 127; i++)
			port_list[i] = 0;

	if (usb_enable) {
		port_list_enable(0);
	} else {
		if (count <= 127)
			port_list_enable(1);
		else
			port_list_enable(0);
	}
	PF_LOG_INFO("[Port list] called uevent");
	portlist_work_func();
}

static struct p_list *add_list(int no)
{
	struct p_list *ptr = NULL;
	struct list_head *listptr;
	struct p_list *entry;
	int get_list = 0;

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		if (entry->no == no) {
			PF_LOG_INFO("[Port list] TCP port[%d] is already in the list!", entry->no);
			get_list = 1;
			break;
		}
	}
	if (!get_list) {
		ptr = kmalloc(sizeof(struct p_list), GFP_KERNEL);
		if (ptr) {
			ptr->no = no;
			list_add_tail(&ptr->list, &curr_port_list.list);
			PF_LOG_INFO("[Port list] TCP port[%d] added\n", no);
		}
	}
	return (ptr);
}

static void remove_list(int no)
{
	struct list_head *listptr;
	struct p_list *entry;
	int get_list = 0;

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		if (entry->no == no) {
			PF_LOG_INFO("[Port list] TCP port[%d] removed\n", entry->no);
			list_del(&entry->list);
			kfree(entry);
			get_list = 1;
			break;
		}
	}
	if (!get_list)
		PF_LOG_INFO("[Port list] TCP port[%d] failed to remove. Port number is not in list!\n", no);
}

static int allocate_port_list(void)
{
	port_list = kzalloc(sizeof(uint16_t)*128, GFP_KERNEL);

	if (port_list == NULL) {
		PF_LOG_INFO("[Port list] Error: Cannot allocate port_list in SMEM_ID_VENDOR2\n");
		return -1;
	} else {
		PF_LOG_INFO("[Port list] Virtual Address of port_list: [%p]\n", port_list);
		port_list[0] = packet_filter_flag;
		return 0;
	}
}

int add_or_remove_port(struct sock *sk, int add_or_remove)
{
	struct inet_sock *inet = inet_sk(sk);
	__be32 src = inet->inet_rcv_saddr;
	__u16 srcp = ntohs(inet->inet_sport);

	wake_lock(&port_suspend_lock);
	if (!packet_filter_flag) {
		wake_unlock(&port_suspend_lock);
		return 0;
	}

	/* Check port list memory allocation */
	if (port_list == NULL) {
		if(allocate_port_list()!=0) {
			wake_unlock(&port_suspend_lock);
			return 0;
		}
	}

	/* if TCP packet and source IP != 127.0.0.1 */
	if (sk->sk_protocol == IPPROTO_TCP && src != 0x0100007F && srcp != 0) {
		/* Handle TCP_LISTEN only */
		if (sk->sk_state != TCP_LISTEN) {
			wake_unlock(&port_suspend_lock);
			return 0;
		}

		mutex_lock(&port_lock);
		PF_LOG_INFO("[Port list] TCP port#: [%d]\n", srcp);
		if (add_or_remove)
			add_list(srcp);
		else
			remove_list(srcp);
		update_port_list();
		mutex_unlock(&port_lock);
	}

	wake_unlock(&port_suspend_lock);
	return 0;
}
EXPORT_SYMBOL(add_or_remove_port);

int update_port_list_charging_state(int enable)
{
	size_t count = 0;

	wake_lock(&port_suspend_lock);
	if (!packet_filter_flag) {
		wake_unlock(&port_suspend_lock);
		return 0;
	}

	if (port_list == NULL) {
		PF_LOG_INFO("[Port list] port_list is NULL.\n");
		wake_unlock(&port_suspend_lock);
		return 0;
	}

	usb_enable = enable;
	mutex_lock(&port_lock);
	if (usb_enable) {
		port_list_enable(0);
	} else {
		for (count = 1; count <= 127; count++) {
			if (!port_list[count])
				break;
		}
		if (count <= 127)
			port_list_enable(1);
		else
			port_list_enable(0);
	}
	mutex_unlock(&port_lock);
	wake_unlock(&port_suspend_lock);
	return 0;
}
EXPORT_SYMBOL(update_port_list_charging_state);


static int __init port_list_init(void)
{
	int ret;
	wake_lock_init(&port_suspend_lock, WAKE_LOCK_SUSPEND, "port_list");
	mutex_init(&port_lock);

	PF_LOG_INFO("[Port list] init()\n");

	/* initial TCP port list linked-list struct */
	memset(&curr_port_list, 0, sizeof(curr_port_list));
	INIT_LIST_HEAD(&curr_port_list.list);

	/* Check port list memory allocation */
	allocate_port_list();

	ret = misc_register(&portlist_misc);
	if (ret < 0) {
		PF_LOG_ERR("[Port list] failed to register misc device!\n");
		goto err_misc_register;
	}

	/* +++ init kobject +++ */
	htc_portlist_kset = kset_create_and_add("event", NULL,
			kobject_get(&portlist_misc.this_device->kobj));
	if (!htc_portlist_kset) {
		ret = -ENOMEM;
		goto err_misc_register;
	}

	portlist_info.portlist_kobj.kset = htc_portlist_kset;

	ret = kobject_init_and_add(&portlist_info.portlist_kobj,
			&htc_portlist_ktype, NULL, "portlist");
	if (ret) {
		kobject_put(&portlist_info.portlist_kobj);
		goto err_misc_register;
	}
	/* --- init kobject --- */

	p_class = class_create(THIS_MODULE, "htc_portlist");
	if (IS_ERR(p_class)) {
		ret = PTR_ERR(p_class);
		p_class = NULL;
		PF_LOG_ERR("[Port list] class_create failed!\n");
		goto err_class_create;
	}

	portlist_misc.this_device = device_create(p_class, NULL, 0 , NULL, "packet_filter");
	if (IS_ERR(portlist_misc.this_device)) {
		ret = PTR_ERR(portlist_misc.this_device);
		portlist_misc.this_device = NULL;
		PF_LOG_ERR("[Port list] device_create failed!\n");
		goto err_device_create;
	}

	ret = device_create_file(portlist_misc.this_device, &dev_attr_flag);
	if (ret < 0) {
		PF_LOG_ERR("[Port list] devices_create_file failed!\n");
		goto err_device_create_file;
	}

	return 0;

err_device_create_file:
	device_destroy(p_class, 0);
err_device_create:
	class_destroy(p_class);
err_class_create:
	misc_deregister(&portlist_misc);
err_misc_register:
	return ret;
}

static void __exit port_list_exit(void)
{
	int ret;
	struct list_head *listptr;
	struct p_list *entry;

	device_remove_file(portlist_misc.this_device, &dev_attr_flag);
	device_destroy(p_class, 0);
	class_destroy(p_class);

	ret = misc_deregister(&portlist_misc);
	if (ret < 0)
		PF_LOG_ERR("[Port list] failed to unregister misc device!\n");

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		kfree(entry);
	}
}

late_initcall(port_list_init);
module_exit(port_list_exit);

MODULE_AUTHOR("Mio Su <Mio_Su@htc.com>");
MODULE_DESCRIPTION("HTC port list driver");
MODULE_LICENSE("GPL");

