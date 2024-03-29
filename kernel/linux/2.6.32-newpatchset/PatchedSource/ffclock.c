/*
 * Feed Forward Clock (FFClock)
 *
 * Written by Thomas Young <tfyoung@orcon.net.nz>
 * Modified by Julien Ridoux <julien@synclab.org>
 *
 * Store FFClock data in the kernel for the purpose of absolute time
 * timestamping in timeval format. Requires updated synchronization data
 * and "fixed point" data to compute  (vcount * phat + Ca).
 *
 * Use a generic netlink socket to allow user space and kernel to access it.
 * In future other access methods could also be made available such as procfs
 *
 * FFClock data is protected by the ffclock_data_mtx rw mutex. If global
 * data ever needs to be read from the interrupt context, then this will have
 * to change.
 *
 *
 * Things needed:
 *	Bounds checking on input
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/fs.h>
#include <linux/random.h>
#include <linux/bootmem.h>
#include <linux/sysdev.h>
#include <net/genetlink.h>
#include <net/sock.h>
#include <linux/ffclock.h>


MODULE_AUTHOR("Thomas Young, Julien Ridoux");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Feed Fowrard Clock (FFClock) driver support");


#define FFCLOCK_NAME	"FFClock"
#define FFCLOCK_TAG	FFCLOCK_NAME": "
#define FFCLOCK_VERSION	1


/* Concurrency saftey */
static DECLARE_RWSEM(ffclock_data_mtx);


/* Netlink socket functionality */
#define FFCLOCK_CMD_GETATTR 0
#define FFCLOCK_CMD_SETATTR 1
enum {
    FFCLOCK_ATTR_DATA,
    FFCLOCK_ATTR_MAX
};

static struct genl_family ffclock_genl = {
	.id = GENL_ID_GENERATE,
	.name = FFCLOCK_NAME,
	.version = 1,
	.hdrsize = 0,
	.maxattr = FFCLOCK_ATTR_MAX,
};


static struct nla_policy ffclock_policy[FFCLOCK_ATTR_MAX] __read_mostly = {
	[FFCLOCK_ATTR_DATA] = {.len = sizeof(struct ffclock_data)},
};


static int ffclock_getattr(struct sk_buff *skb, struct genl_info *info);
static int ffclock_setattr(struct sk_buff *skb, struct genl_info *info);
static struct  genl_ops ffclock_ops[] = {
	{
		.cmd = FFCLOCK_CMD_GETATTR,
		.doit = ffclock_getattr,
		.policy = ffclock_policy,
	},
	{
		.cmd = FFCLOCK_CMD_SETATTR,
		.doit = ffclock_setattr,
		.policy = ffclock_policy,
	},
};


/* Sysfs functionality */
static ssize_t sysfs_show_ffclock_version(struct sys_device *dev,
					struct sysdev_attribute *attr,
					char *buf);
static ssize_t sysfs_show_ffclock_tsmode(struct sys_device *dev,
					struct sysdev_attribute *attr,
					char *buf);
static ssize_t sysfs_change_ffclock_tsmode(struct sys_device *dev,
					struct sysdev_attribute *attr,
					const char *buf,
					size_t count);


static char sysfs_user_input[32];
static DEFINE_SPINLOCK(ffclock_lock);
static int sysfs_ffclock_version = FFCLOCK_VERSION;
static int sysfs_ffclock_tsmode = FFCLOCK_TSMODE_SYSCLOCK;


static SYSDEV_ATTR(version, 0444, sysfs_show_ffclock_version, NULL);
static SYSDEV_ATTR(timestamping_mode, 0644, sysfs_show_ffclock_tsmode,
		sysfs_change_ffclock_tsmode);


static struct sysdev_class ffclock_sysclass = {
	.name = "ffclock",
};


static struct sys_device device_ffclock = {
	.id	= 0,
	.cls	= &ffclock_sysclass,
};


/*
 * ffclock_fill_skb - Fills a sk_buff for timing information useful for
 * timestamping network packets.
 */
static int ffclock_fill_skb(u32 pid, u32 seq, u32 flags,
			    struct sk_buff *skb, u8 cmd)
{
	void *hdr;

	hdr = genlmsg_put(skb, pid, seq, &ffclock_genl, flags, cmd);
	if (hdr == NULL)
		return -EINVAL;

	down_read(&ffclock_data_mtx);
	NLA_PUT(skb, FFCLOCK_ATTR_DATA, sizeof(struct ffclock_data),
		&(ffclock.cest->cdata));
	up_read(&ffclock_data_mtx);
	return genlmsg_end(skb, hdr);

nla_put_failure:
	up_read(&ffclock_data_mtx);
	genlmsg_cancel(skb, hdr);
	return -EINVAL;
}


/*
 * ffclock_build_msg - Constructs a reply for a global data request
 */
static struct sk_buff *ffclock_build_msg(u32 pid, int seq, int cmd)
{
	int err;
	struct sk_buff *skb;

	skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (skb == NULL)
		return ERR_PTR(-ENOBUFS);

	err = ffclock_fill_skb(pid, seq, 0, skb, cmd);
	if (err < 0) {
		nlmsg_free(skb);
		return ERR_PTR(err);
	}

	return skb;
}


/*
 * ffclock_setattr - Sets ffclock information from a netlink socket, presumably
 * called from userland.
 *
 * TODO: Only let privileged processes set global data?
 */
static int ffclock_setattr(struct sk_buff *skb, struct genl_info *info)
{
	struct ffclock_data *value;

	/* TODO: Check perms */
	if (!info)
		BUG();

	if (!info->attrs)
		BUG();

	if (info->attrs[FFCLOCK_ATTR_DATA] != NULL) {
		if (nla_len(info->attrs[FFCLOCK_ATTR_DATA]) !=
			    sizeof(struct ffclock_data))
			return -EINVAL;

		value = nla_data(info->attrs[FFCLOCK_ATTR_DATA]);

		/* TODO: Sanity check */
		down_write(&ffclock_data_mtx);
		memcpy(&(ffclock.ucest->cdata), value,
			sizeof(struct ffclock_data));
		ffclock.updated = 1;
		up_write(&ffclock_data_mtx);
	}

	return 0;
}


/*
 * ffclock_getattr - Responds to netlink requests for ffclock information.
 * Presumably called from userland.
 *
 * TODO: Handle requests for ffclock_fp. We currently don't need it though,
 * since no one else has a use for the data.
 */
static int ffclock_getattr(struct sk_buff *skb, struct genl_info *info)
{
	/* TODO: Check perms */
	struct sk_buff *msg;

	msg = ffclock_build_msg(info->snd_pid, info->snd_seq,
				FFCLOCK_CMD_GETATTR);

	if (IS_ERR(msg))
		return PTR_ERR(msg);

	return genlmsg_unicast(genl_info_net(info), msg, info->snd_pid);
}


/**
 * sysfs_show_ffclock_version- sysfs interface to get ffclock version
 * @dev:	unused
 * @buf:	char buffer to be filled with passthrough mode
 *
 * Provides sysfs interface to get ffclock version
 */
static ssize_t sysfs_show_ffclock_version(struct sys_device *dev,
					struct sysdev_attribute *attr,
					char *buf)
{
	ssize_t count;

	spin_lock_irq(&ffclock_lock);
	count = snprintf(buf, PAGE_SIZE, "%d", sysfs_ffclock_version);
	spin_unlock_irq(&ffclock_lock);

	count += snprintf(buf + count, PAGE_SIZE, "\n");

	return count;
}


/**
 * sysfs_show_ffclock_tsmode- sysfs interface to get ffclock timestamping mode
 * @dev:	unused
 * @buf:	char buffer to be filled with passthrough mode
 *
 * Provides sysfs interface to get ffclock timestamping mode
 */
static ssize_t sysfs_show_ffclock_tsmode(struct sys_device *dev,
					struct sysdev_attribute *attr,
					char *buf)
{
	ssize_t count;

	spin_lock_irq(&ffclock_lock);
	count = snprintf(buf, PAGE_SIZE, "%d", sysfs_ffclock_tsmode);
	spin_unlock_irq(&ffclock_lock);

	count += snprintf(buf + count, PAGE_SIZE, "\n");

	return count;
}


/**
 * sysfs_change_ffclock_tsmode - interface for manually changing the default
 * timestamping mode
 * @dev:	unused
 * @buf:	new value of passthrough mode (0 or 1)
 * @count:	length of buffer
 *
 * Takes input from sysfs interface for manually changing the default
 * timestamping mode.
 */
static ssize_t sysfs_change_ffclock_tsmode(struct sys_device *dev,
					struct sysdev_attribute *attr,
					const char *buf,
					size_t count)
{
	long val;
	size_t ret = count;

	/* Strings from sysfs write are not 0 terminated! */
	if (count >= sizeof(sysfs_user_input))
		return -EINVAL;

	/* Strip of \n: */
	if (buf[count-1] == '\n')
		count--;

	spin_lock_irq(&ffclock_lock);

	if (count > 0)
		memcpy(sysfs_user_input, buf, count);
	sysfs_user_input[count] = 0;

	val = strict_strtol(sysfs_user_input, 10, NULL);

	switch (val) {
	case FFCLOCK_TSMODE_SYSCLOCK:
		sysfs_ffclock_tsmode = FFCLOCK_TSMODE_SYSCLOCK;
		break;
	case FFCLOCK_TSMODE_FFCLOCK:
		sysfs_ffclock_tsmode = FFCLOCK_TSMODE_FFCLOCK;
		break;
	default:
		break;
	}

	spin_unlock_irq(&ffclock_lock);

	return ret;
}


static int init_ffclock_sysfs(void)
{
	int err = sysdev_class_register(&ffclock_sysclass);

	if (!err)
		err = sysdev_register(&device_ffclock);

	if (!err)
		err = sysdev_create_file(
				&device_ffclock,
				&attr_version);
	if (!err)
		err = sysdev_create_file(
				&device_ffclock,
				&attr_timestamping_mode);
	return err;
}


static int __init ffclock_register(void)
{
	int i;

	if (genl_register_family(&ffclock_genl)) {
		printk(KERN_WARNING FFCLOCK_TAG "netlink socket could not be "
			"created, exiting\n");
		goto errout;
	}

	for (i = 0; i < ARRAY_SIZE(ffclock_ops); i++)
		if (genl_register_ops(&ffclock_genl, &ffclock_ops[i]) < 0)
			goto errout_unregister;

	printk(KERN_INFO FFCLOCK_TAG "netlink socket registered with id %d\n",
		ffclock_genl.id);

	if (init_ffclock_sysfs())
		goto errout_unregister;

	printk(KERN_INFO FFCLOCK_TAG "sysfs initialized\n");
	return 0;

errout_unregister:
	genl_unregister_family(&ffclock_genl);
errout:
	return -EFAULT;
}


static void __exit ffclock_unregister(void)
{
	printk(KERN_INFO FFCLOCK_TAG "netlink socket unregistered\n");
	genl_unregister_family(&ffclock_genl);
}


module_init(ffclock_register);
module_exit(ffclock_unregister);
