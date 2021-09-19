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
#include <linux/device.h>
#include <linux/types.h>

#include <net/genetlink.h>
#include <net/sock.h>


static struct ffclock_estimate cest;
static struct radclock_fixedpoint radclock_fp;

static DECLARE_RWSEM(radclock_data_mtx);
static DECLARE_RWSEM(radclock_fixedpoint_mtx);


static struct genl_family radclock_genl = {
	.name = RADCLOCK_NAME,
	.version = 0x1,
	.hdrsize = 0,
	.maxattr = RADCLOCK_ATTR_MAX,
};

/**
 * Fill an skb `message' with the global data as an attribute.
 */
static int radclock_fill_skb(struct genl_info *info, u32 flags, struct sk_buff *skb, u8 cmd)
{
	void * hdr;
	int puterr;

	hdr = genlmsg_put(skb, info->snd_portid, info->snd_seq, &radclock_genl, flags, cmd);
	if (hdr == NULL)
		return -1;

	/* Fill message with each available attribute */
	//printk(KERN_INFO " ** getting RAD attr data \n");
	down_read(&radclock_data_mtx);
	puterr = nla_put(skb, RADCLOCK_ATTR_DATA, sizeof(cest), &cest);
	up_read(&radclock_data_mtx);
	if (puterr<0)
		goto nla_put_failure;

	down_read(&radclock_fixedpoint_mtx);
	//printk(KERN_INFO " ** getting RAD attr fp \n");
	puterr = nla_put(skb, RADCLOCK_ATTR_FIXEDPOINT, sizeof(radclock_fp), &radclock_fp);
	up_read(&radclock_fixedpoint_mtx);
	if (puterr<0)
		goto nla_put_failure;

	genlmsg_end(skb, hdr);
	return 0;

nla_put_failure:
	genlmsg_cancel(skb, hdr);
	return -1;
}

/**
 * Build a reply for a global data request
 */
static struct sk_buff * radclock_build_msg(struct genl_info *info, int cmd)
{
	struct sk_buff *skb;
	int err;

	skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (skb == NULL)
		return ERR_PTR(-ENOBUFS);

	err = radclock_fill_skb(info, 0, skb, cmd);
	if (err < 0) {
		nlmsg_free(skb);
		return ERR_PTR(err);
	}
	return skb;
}

/**
 * Respond to a get request. The request does not specify which attribute is desired.
 * All available will be sent in the message.
 */
static int radclock_getattr(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;

	msg = radclock_build_msg(info, RADCLOCK_CMD_GETATTR);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);
}

static struct nla_policy radclock_policy[RADCLOCK_ATTR_MAX +1] __read_mostly = {
	[RADCLOCK_ATTR_DATA] 		= {  .len = sizeof(struct ffclock_estimate) },
	[RADCLOCK_ATTR_FIXEDPOINT] = {  .len = sizeof(struct radclock_fixedpoint) },
};

/**
 * Set the global data by transferring attributes from the received set message to the global variables
 * Each of the two active kinds of global data acted on, if present.
 * TODO: only let priviledged processes set global data
 */
static int radclock_setattr(struct sk_buff *skb, struct genl_info *info)
{
	//TODO check perms
	if (!info)
		BUG();
	if (!info->attrs)
		BUG();

	//printk(KERN_INFO " ** RADclock_setattr entered \n");

	/* `Loop' over all possible attribute types */
	if (info->attrs[RADCLOCK_ATTR_DATA] != NULL)
	{
		//printk(KERN_INFO " ** setting RAD attr data \n");
		struct ffclock_estimate *value;
		if (nla_len(info->attrs[RADCLOCK_ATTR_DATA]) != sizeof(cest))
			return -EINVAL;

		value = nla_data(info->attrs[RADCLOCK_ATTR_DATA]);
		down_write(&radclock_data_mtx);
		memcpy(&cest, value, sizeof(cest));
		up_write(&radclock_data_mtx);
	}

	if (info->attrs[RADCLOCK_ATTR_FIXEDPOINT] != NULL)
	{
		//printk(KERN_INFO " ** setting RAD attr fp \n");
		struct radclock_fixedpoint *valuefp;
		if (nla_len(info->attrs[RADCLOCK_ATTR_FIXEDPOINT]) != sizeof(radclock_fp))
			return -EINVAL;

		valuefp = nla_data(info->attrs[RADCLOCK_ATTR_FIXEDPOINT]);
		down_write(&radclock_fixedpoint_mtx);
		memcpy(&radclock_fp, valuefp, sizeof(radclock_fp));
		up_write(&radclock_fixedpoint_mtx);
	}

	return 0;
}

/* Setup the callbacks */
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


/* Read radclock at the passed raw timestamp using the radclock_fp parameters */
void radclock_fill_ktime(ffcounter ffcount, ktime_t *ktime)
{
	ffcounter countdiff;
	struct timespec tspec;
	u64 time_f;
	u64 frac;

	/* Synchronization algorithm (userland) should update the fixed point data
	 * often enough to make sure the timeval does not overflow. If no sync algo
	 * updates the data, we loose precision, but in that case, nobody is tracking
	 * the clock drift anyway ... so send warning and stop worrying.
	 */
	down_read(&radclock_fixedpoint_mtx);

	countdiff = ffcount - radclock_fp.vcount;
//	if (countdiff & ~((1ll << (radclock_fp.countdiff_maxbits +1)) -1))
//		printk(KERN_WARNING "RADclock: warning stamp may overflow timeval at %llu!\n",
//				(long long unsigned) ffcount);

	/* Add the counter delta in second to the recorded fixed point time */
	time_f = radclock_fp.time_int
		  + ((radclock_fp.phat_int * countdiff) >> (radclock_fp.phat_shift - radclock_fp.time_shift)) ;

	tspec.tv_sec = time_f >> radclock_fp.time_shift;

	frac = (time_f - ((u64)tspec.tv_sec << radclock_fp.time_shift));
	tspec.tv_nsec = (frac * 1000000000LL)  >> radclock_fp.time_shift;
	/* tv.nsec truncates at the nano-second digit, so check for next digit rounding */
	if ( ((frac * 10000000000LL) >> radclock_fp.time_shift) >= (tspec.tv_nsec * 10LL + 5) )
		tspec.tv_nsec++;

	/* Push the timespec into the ktime, Ok for 32 and 64 bit arch (see ktime.h) */
	*ktime = timespec_to_ktime(tspec);

	up_read(&radclock_fixedpoint_mtx);
}

EXPORT_SYMBOL_GPL(radclock_fill_ktime);



/* Sysfs entries */

static DEFINE_SPINLOCK(ffclock_lock);
static char user_input[32];

static int ffclock_version = 2;

/**
 * version_ffclock_show -  interface to get ffclock version
 * @dev:	unused
 * @buf:	char buffer to be filled with bypass mode
 *
 * Provides sysfs interface to get ffclock version
 */
static ssize_t version_ffclock_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t count = 0;

	spin_lock_irq(&ffclock_lock);
	count = snprintf(buf,
			 max((ssize_t)PAGE_SIZE - count, (ssize_t)0), "%d", ffclock_version);
	spin_unlock_irq(&ffclock_lock);

	count += snprintf(buf + count,
			max((ssize_t)PAGE_SIZE - count, (ssize_t)0), "\n");

	return count;
}
static DEVICE_ATTR_RO(version_ffclock);


/* FFC tsmode system */
int ffclock_tsmode = RADCLOCK_TSMODE_SYSCLOCK;

/**
 * tsmode_ffclock_show -  interface to get ffclock timestamping mode
 * @dev:	unused
 * @buf:	char buffer to be filled with bypass mode
 *
 * Provides sysfs interface to get ffclock timestamping mode
 */
static ssize_t tsmode_ffclock_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t count = 0;

	spin_lock_irq(&ffclock_lock);
	count = snprintf(buf,
			 max((ssize_t)PAGE_SIZE - count, (ssize_t)0),
			"%d", ffclock_tsmode);

	spin_unlock_irq(&ffclock_lock);

	count += snprintf(buf + count,
			  max((ssize_t)PAGE_SIZE - count, (ssize_t)0), "\n");

	return count;
}


/**
 * tsmode_ffclock_store - interface for manually changing the default timestamping mode
 * @dev:	unused
 * @buf:	new value of bypass mode (0 or 1)
 * @count:	length of buffer
 *
 * Takes input from sysfs interface for manually changing the default
 * timestamping mode.
 */
static ssize_t tsmode_ffclock_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	long val;
	size_t ret = count;

	/* strings from sysfs write are not 0 terminated! */
	if (count >= sizeof(user_input))
		return -EINVAL;

	/* strip off \n: */
	if (buf[count-1] == '\n')
		count--;

	spin_lock_irq(&ffclock_lock);

	if (count > 0)
		memcpy(user_input, buf, count);
	user_input[count] = 0;

	val = simple_strtol(user_input, NULL, 10);

	switch (val) {
		case RADCLOCK_TSMODE_SYSCLOCK:
			ffclock_tsmode = RADCLOCK_TSMODE_SYSCLOCK;
			break;
		case RADCLOCK_TSMODE_RADCLOCK:
			ffclock_tsmode = RADCLOCK_TSMODE_RADCLOCK;
			break;
		default:
			break;
	}
	spin_unlock_irq(&ffclock_lock);

	return ret;
}
static DEVICE_ATTR_RW(tsmode_ffclock);



/* FFC Bypass system */
static char override_bypass[8];
uint8_t ffcounter_bypass;

/**
 * bypass_ffclock_show - sysfs interface for showing ffcount reading mode
 * @dev:	unused
 * @buf:	char buffer to be filled with bypass mode
 *
 * Provides sysfs interface for showing ffcount reading mode
 */
static ssize_t bypass_ffclock_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	ssize_t count = 0;

	spin_lock_irq(&ffclock_lock);
	if (ffcounter_bypass == 1)		// activate bypass
		count = snprintf(buf,
				 max((ssize_t)PAGE_SIZE - count, (ssize_t)0),
				"1");
	else
		count = snprintf(buf,
				 max((ssize_t)PAGE_SIZE - count, (ssize_t)0),
				"0");

	spin_unlock_irq(&ffclock_lock);

	count += snprintf(buf + count,
			  max((ssize_t)PAGE_SIZE - count, (ssize_t)0), "\n");

	return count;
}

/**
 * bypass_ffclock_store - interface for manually overriding the ffcount bypass mode
 * @dev:	unused
 * @buf:	new value of bypass mode (0 or 1)
 * @count:	length of buffer
 *
 * Takes input from sysfs interface for manually overriding the ffcount bypass mode.
 */
static ssize_t bypass_ffclock_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	size_t ret = count;

	/* strings from sysfs write are not 0 terminated! */
	if (count >= sizeof(override_bypass))
		return -EINVAL;

	/* strip off \n: */
	if (buf[count-1] == '\n')
		count--;

	spin_lock_irq(&ffclock_lock);

	if (count > 0)
		memcpy(override_bypass, buf, count);
	override_bypass[count] = 0;

	if ( !strcmp(override_bypass, "0"))
		ffcounter_bypass = 0;
	if ( !strcmp(override_bypass, "1"))
		ffcounter_bypass = 1;

	spin_unlock_irq(&ffclock_lock);

	return ret;
}
static DEVICE_ATTR_RW(bypass_ffclock);


static struct attribute *ffclock_attrs[] = {
	&dev_attr_version_ffclock.attr,
	&dev_attr_tsmode_ffclock.attr,
	&dev_attr_bypass_ffclock.attr,
	NULL
};
ATTRIBUTE_GROUPS(ffclock);

static struct bus_type ffclock_subsys = {
	.name = "ffclock",
	.dev_name = "ffclock",
};

static struct device device_ffclock = {
	.id	= 0,
	.bus	= &ffclock_subsys,
	.groups	= ffclock_groups,
};


static int __init init_ffclock_sysfs(void)
{
	int error = subsys_system_register(&ffclock_subsys, NULL);

	if (!error)
		error = device_register(&device_ffclock);

	return error;
}


/* RADclock module definition */

/* Register radclock with netlink and sysfs, and initialize. */
static int __init radclock_register(void)
{
	/* Register family and operations with netlink */
	radclock_genl.ops = radclock_ops;
	radclock_genl.n_ops = 2;
	if (genl_register_family(&radclock_genl)) {
		printk(KERN_WARNING "RADclock netlink socket could not be created, exiting\n");
		goto errout;
	} else
		printk(KERN_INFO "%s netlink family registered with id %d\n",
							radclock_genl.name, radclock_genl.id);

	/* Register ffclock sysfs subsystem */
	if ( init_ffclock_sysfs() )
		goto errout_unregister;
	printk(KERN_INFO "Feed-Forward Clock sysfs initialized\n");

	/* Initialize global data [ both KV1 and KV2 ] */
	memset(&cest, 0, sizeof(cest));
	memset(&radclock_fp, 0, sizeof(radclock_fp));

	return 0;

errout_unregister:
	genl_unregister_family(&radclock_genl);
errout:
	return -EFAULT;
}

static void __exit radclock_unregister(void)
{
	printk(KERN_INFO "RADclock netlink family unregistered\n");
	genl_unregister_family(&radclock_genl);
}



module_init(radclock_register);
module_exit(radclock_unregister);

MODULE_AUTHOR("Thomas Young, Julien Ridoux, Darryl Veitch");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RADclock driver support");
