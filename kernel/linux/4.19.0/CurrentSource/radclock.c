/*
 * RADclock data
 *
 * Written by Thomas Young <tfyoung@orcon.net.nz>
 * Modified by Julien Ridoux <julien@synclab.org>
 *
 * Store RADclock data in the kernel for the purpose of absolute time
 * timestamping in timeval format. Requires updated synchronization data
 * and "fixed point" data to compute  (vcount * phat + Ca).
 *
 * Use a generic netlink socket to allow user space and kernel to access it.
 * In future other access methods could also be made available such as procfs
 *
 * RADclock data is protected by the radclock_data_mtx rw mutex. If global
 * data ever needs to be read from the interupt context, then this will have
 * to change.
 *
 * RADclock fixedpoint data is protected by the radclock_fixedpoint_mtx rw
 * mutex.
 *
 * Since using an old version isn't a complete disaster, it wouldn't be a bad
 * idea to use a wheel and to use lighter locking.
 *
 * Things needed:
 *	Bounds checking on input
 */

#include <linux/bootmem.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/radclock.h>
#include <linux/random.h>
#include <linux/rwsem.h>
#include <linux/sysdev.h>
#include <linux/types.h>

#include <net/genetlink.h>
#include <net/sock.h>




static struct radclock_data radclock_data;
static struct radclock_fixedpoint radclock_fp;

static DECLARE_RWSEM(radclock_data_mtx);
static DECLARE_RWSEM(radclock_fixedpoint_mtx);


static struct genl_family radclock_genl = {
	.id = GENL_ID_GENERATE,
	.name = RADCLOCK_NAME,
	.version = 0x1,
	.hdrsize = 0,
	.maxattr = RADCLOCK_ATTR_MAX,
};

/**
 * Fill an skb with the global data
 */
static int radclock_fill_skb(u32 pid, u32 seq, u32 flags, struct sk_buff *skb, u8 cmd)
{
	void * hdr;
	hdr = genlmsg_put(skb, pid, seq, &radclock_genl, flags, cmd);
	if (hdr == NULL)
		return -1;
	down_read(&radclock_data_mtx);
	NLA_PUT(skb, RADCLOCK_ATTR_DATA, sizeof(radclock_data),&radclock_data);
	up_read(&radclock_data_mtx);
	return genlmsg_end(skb, hdr);

nla_put_failure:
	up_read(&radclock_data_mtx);
	genlmsg_cancel(skb, hdr);
	return -1;
}

/**
 * Build a reply for a global data request
 */
static struct sk_buff * radclock_build_msg(u32 pid, int seq, int cmd)
{
	struct sk_buff *skb;
	int err;
	skb= nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (skb == NULL)
		return ERR_PTR(-ENOBUFS);

	err = radclock_fill_skb(pid, seq, 0, skb, cmd);
	if (err < 0)
	{
		nlmsg_free(skb);
		return ERR_PTR(err);
	}
	return skb;
}


/**
 * Set the global data
 *
 * TODO: only let privilidged processes set global data?
 */
static int radclock_setattr(struct sk_buff *skb, struct genl_info *info)
{
	//TODO check perms
	if (!info)
		BUG();
	if (!info->attrs)
		BUG();
	if (info->attrs[RADCLOCK_ATTR_DATA] != NULL)
	{
		struct radclock_data *value;
		if (nla_len(info->attrs[RADCLOCK_ATTR_DATA]) != sizeof(radclock_data))
			return -EINVAL;

		value = nla_data(info->attrs[RADCLOCK_ATTR_DATA]);
		//TODO sanity check
		//
		down_write(&radclock_data_mtx);
		memcpy(&radclock_data, value, sizeof(radclock_data));
		up_write(&radclock_data_mtx);
	}
	if (info->attrs[RADCLOCK_ATTR_FIXEDPOINT] != NULL)
	{
		struct radclock_fixedpoint *value;
		if (nla_len(info->attrs[RADCLOCK_ATTR_FIXEDPOINT]) != sizeof(radclock_fp))
			return -EINVAL;

		value = nla_data(info->attrs[RADCLOCK_ATTR_FIXEDPOINT]);
		//TODO sanity check
		//
		down_write(&radclock_fixedpoint_mtx);
		memcpy(&radclock_fp, value, sizeof(radclock_fp));
		up_write(&radclock_fixedpoint_mtx);
	}

	return 0;
}

/**
 * Respond to a request
 *
 * TODO: handle requests for radclock_fp. We currently don't need it though, since
 * no one else has a use for the data.
 */
static int radclock_getattr(struct sk_buff *skb, struct genl_info *info)
{
	//TODO check perms
	struct sk_buff *msg;
	msg = radclock_build_msg(info->snd_pid, info->snd_seq, RADCLOCK_CMD_GETATTR);
	if (IS_ERR(msg))
		return PTR_ERR(msg);
	return genlmsg_unicast(genl_info_net(info), msg, info->snd_pid);
}

static struct nla_policy radclock_policy[RADCLOCK_ATTR_MAX +1] __read_mostly = {
	[RADCLOCK_ATTR_DATA] = {  .len = sizeof(struct radclock_data) },
	[RADCLOCK_ATTR_FIXEDPOINT] = {  .len = sizeof(struct radclock_fixedpoint) },
};

static struct  genl_ops radclock_ops[] = {
	{
		.cmd = RADCLOCK_CMD_GETATTR,
		.doit = radclock_getattr,
		.policy = radclock_policy,
	},
	{
		.cmd = RADCLOCK_CMD_SETATTR,
		.doit = radclock_setattr,
		.policy = radclock_policy,
	},
};


void radclock_fill_ktime(vcounter_t vcounter, ktime_t *ktime)
{
	vcounter_t countdiff;
	struct timespec tspec;
	u64 time_f;
	u64 frac;

	/* Synchronization algorithm (userland) should update the fixed point data
	 * often enough to make sure the timeval does not overflow. If no sync algo
	 * updates the data, we loose precision, but in that case, nobody is tracking
	 * the clock drift anyway ... so send warning and stop worrying.
	 */
	down_read(&radclock_fixedpoint_mtx);

	countdiff = vcounter - radclock_fp.vcount;
	if (countdiff & ~((1ll << (radclock_fp.countdiff_maxbits +1)) -1))
		printk(KERN_WARNING "RADclock: warning stamp may overflow timeval at %llu!\n",
				(long long unsigned) vcounter);

	/* Add the counter delta in second to the recorded fixed point time */
	time_f 	= radclock_fp.time_int
		  + ((radclock_fp.phat_int * countdiff) >> (radclock_fp.phat_shift - radclock_fp.time_shift)) ;

	tspec.tv_sec = time_f >> radclock_fp.time_shift;

	frac = (time_f - ((u64)tspec.tv_sec << radclock_fp.time_shift));
	tspec.tv_nsec = (frac * 1000000000LL)  >> radclock_fp.time_shift;
	/* tv.nsec truncates at the nano-second digit, so check for next digit rounding */
	if ( ((frac * 10000000000LL) >> radclock_fp.time_shift) >= (tspec.tv_nsec * 10LL + 5) )
	{
		tspec.tv_nsec++;
	}

	/* Push the timespec into the ktime, Ok for 32 and 64 bit arch (see ktime.h) */
	*ktime = timespec_to_ktime(tspec);

	up_read(&radclock_fixedpoint_mtx);
}

EXPORT_SYMBOL_GPL(radclock_fill_ktime);



/*
 * Sysfs entries
 */

static DEFINE_SPINLOCK(ffclock_lock);
static char sysfs_user_input[32];

/*
 * Feed-forward clock support version
 */
int sysfs_ffclock_version = FFCLOCK_VERSION;

/**
 * sysfs_show_ffclock_version- sysfs interface to get ffclock version
 * @dev:	unused
 * @buf:	char buffer to be filled with passthrough mode
 *
 * Provides sysfs interface to get ffclock version
 */
static ssize_t
sysfs_show_ffclock_version(struct sys_device *dev,
				  struct sysdev_attribute *attr,
				  char *buf)
{
	ssize_t count = 0;

	spin_lock_irq(&ffclock_lock);
	count = snprintf(buf,
			 max((ssize_t)PAGE_SIZE - count, (ssize_t)0),
			"%d", sysfs_ffclock_version);

	spin_unlock_irq(&ffclock_lock);

	count += snprintf(buf + count,
			  max((ssize_t)PAGE_SIZE - count, (ssize_t)0), "\n");

	return count;
}

static SYSDEV_ATTR(version, 0444, sysfs_show_ffclock_version, NULL);


/*
 * Feed-forward clock timestamping mode
 */
int sysfs_ffclock_tsmode = RADCLOCK_TSMODE_SYSCLOCK;

/**
 * sysfs_show_ffclock_tsmode- sysfs interface to get ffclock timestamping mode
 * @dev:	unused
 * @buf:	char buffer to be filled with passthrough mode
 *
 * Provides sysfs interface to get ffclock timestamping mode
 */
static ssize_t
sysfs_show_ffclock_tsmode(struct sys_device *dev,
				  struct sysdev_attribute *attr,
				  char *buf)
{
	ssize_t count = 0;

	spin_lock_irq(&ffclock_lock);
	count = snprintf(buf,
			 max((ssize_t)PAGE_SIZE - count, (ssize_t)0),
			"%d", sysfs_ffclock_tsmode);

	spin_unlock_irq(&ffclock_lock);

	count += snprintf(buf + count,
			  max((ssize_t)PAGE_SIZE - count, (ssize_t)0), "\n");

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
					  const char *buf, size_t count)
{
	long val;
	size_t ret = count;

	/* strings from sysfs write are not 0 terminated! */
	if (count >= sizeof(sysfs_user_input))
		return -EINVAL;

	/* strip of \n: */
	if (buf[count-1] == '\n')
		count--;

	spin_lock_irq(&ffclock_lock);

	if (count > 0)
		memcpy(sysfs_user_input, buf, count);
	sysfs_user_input[count] = 0;

	val = simple_strtol(sysfs_user_input, NULL, 10);

	switch (val)
	{
		case RADCLOCK_TSMODE_SYSCLOCK:
			sysfs_ffclock_tsmode = RADCLOCK_TSMODE_SYSCLOCK;
			break;
		case RADCLOCK_TSMODE_RADCLOCK:
			sysfs_ffclock_tsmode = RADCLOCK_TSMODE_RADCLOCK;
			break;
		case RADCLOCK_TSMODE_FAIRCOMPARE:
			sysfs_ffclock_tsmode = RADCLOCK_TSMODE_FAIRCOMPARE;
			break;
		default:
			break;
	}
	spin_unlock_irq(&ffclock_lock);

	return ret;
}


static SYSDEV_ATTR(timestamping_mode, 0644, sysfs_show_ffclock_tsmode, sysfs_change_ffclock_tsmode);




static struct sysdev_class ffclock_sysclass = {
	.name = "ffclock",
};

static struct sys_device device_ffclock = {
	.id	= 0,
	.cls	= &ffclock_sysclass,
};


//static int __init init_ffclock_sysfs(void)
static int init_ffclock_sysfs(void)
{
	int error = sysdev_class_register(&ffclock_sysclass);

	if (!error)
		error = sysdev_register(&device_ffclock);

	if (!error)
		error = sysdev_create_file(
				&device_ffclock,
				&attr_version);
	if (!error)
		error = sysdev_create_file(
				&device_ffclock,
				&attr_timestamping_mode);
	return error;
}
// Pushed into module_init
//device_initcall(init_ffclock_sysfs);





static int __init radclock_register(void)
{
	int i;
	if (genl_register_family(&radclock_genl))
	{
		printk(KERN_WARNING "RADclock netlink socket could not be created, exiting\n");
		goto errout;
	}
	for (i =0; i < ARRAY_SIZE(radclock_ops); i++)
		if (genl_register_ops(&radclock_genl, &radclock_ops[i]) < 0)
			goto errout_unregister;

	/* TODO: more sensible start than 0? */
	memset(&radclock_data, 0, sizeof(radclock_data));
	printk(KERN_INFO "RADclock netlink socket registered with id %d\n", radclock_genl.id);

	if ( init_ffclock_sysfs() )
		goto errout_unregister;
	printk(KERN_INFO "Feed-Forward Clock sysfs initialized\n");

	return 0;

errout_unregister:
	genl_unregister_family(&radclock_genl);
errout:
	return -EFAULT;
}

static void __exit radclock_unregister(void)
{
	printk(KERN_INFO "RADclock netlink socket unregistered\n");
	genl_unregister_family(&radclock_genl);
}



module_init(radclock_register);
module_exit(radclock_unregister);

MODULE_AUTHOR("Thomas Young, Julien Ridoux");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RADclock driver support");
