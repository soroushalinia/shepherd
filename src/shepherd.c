// SPDX-License-Identifier: GPL-2.0-only
/*
 * shepherd - A Linux kernel module with a single opinionated sheep
 *
 * Copyright (C) 2026 Soroush Alinia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * /proc/sheep: a stateful sheep. pet it, feed it, scare it, or just
 * sit and watch. It gets hungry. It gets lonely. It judges you.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/atomic.h>
#include <linux/cred.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/uidgid.h>

#define PROC_NAME  "sheep"
#define PROC_MODE  0666

#define HUNGRY_TIMEOUT  (30 * HZ)
#define IGNORE_TIMEOUT  (60 * HZ)
#define RESPONSE_LEN    256
#define CMD_LEN         64
#define PET_LIMIT       100
#define TRUST_READS     1000

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Soroush Alinia");
MODULE_DESCRIPTION("shepherd: a lone sheep living in /proc/sheep");
MODULE_VERSION("1.0.0");

enum sheep_state {
	STATE_NORMAL,
	STATE_HAPPY,
	STATE_HUNGRY,
	STATE_SLEEPING,
	STATE_SCARED,
	STATE_IGNORED,
};

static struct proc_dir_entry *sheep_proc_entry;
static enum sheep_state current_state = STATE_NORMAL;
static unsigned long last_interaction;
static unsigned long last_fed;
static atomic_t read_count;
static unsigned int consecutive_pets;
static bool sheared;
static bool trust_shown;
static char response[RESPONSE_LEN];
static bool response_pending;

static DEFINE_MUTEX(sheep_lock);

#define SHEEP "\xf0\x9f\x90\x91"

static void set_response(const char *msg)
{
	strscpy(response, msg, sizeof(response));
	response_pending = true;
}

static void clear_response(void)
{
	response_pending = false;
	response[0] = '\0';
}

static void decay_state(void)
{
	if (current_state == STATE_SLEEPING || current_state == STATE_SCARED)
		return;

	if (time_after_eq(jiffies, last_interaction + IGNORE_TIMEOUT))
		current_state = STATE_IGNORED;
	else if (time_after_eq(jiffies, last_fed + HUNGRY_TIMEOUT))
		current_state = STATE_HUNGRY;
}

static const char *state_message(enum sheep_state state)
{
	switch (state) {
	case STATE_NORMAL:
		return SHEEP " Baaaa.";
	case STATE_HAPPY:
		return SHEEP " *happy baa*";
	case STATE_HUNGRY:
		return SHEEP " Baa...";
	case STATE_SLEEPING:
		return SHEEP " zzz...";
	case STATE_SCARED:
		return SHEEP " BAAAAAAAAAA!";
	case STATE_IGNORED:
		return SHEEP " ...";
	default:
		return SHEEP " Baaaa.";
	}
}

static ssize_t sheep_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	char local[RESPONSE_LEN];
	int len = 0;

	if (*ppos > 0)
		return 0;

	mutex_lock(&sheep_lock);

	if (atomic_inc_return(&read_count) == TRUST_READS && !trust_shown) {
		trust_shown = true;
		len = snprintf(local, sizeof(local),
			       "The sheep finally trusts you.\n");
		goto done;
	}

	if (response_pending) {
		len = snprintf(local, sizeof(local), "%s\n", response);
		clear_response();
		goto done;
	}

	decay_state();

	len = snprintf(local, sizeof(local), "%s\n", state_message(current_state));

	if (current_state == STATE_HAPPY)
		current_state = STATE_NORMAL;

done:
	mutex_unlock(&sheep_lock);

	if (len > count)
		len = count;
	if (len > 0 && copy_to_user(buf, local, len))
		return -EFAULT;

	*ppos = len;
	return len;
}

static size_t strip_trailing_whitespace(char *s, size_t len)
{
	while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == ' ' ||
			   s[len - 1] == '\r' || s[len - 1] == '\t'))
		s[--len] = '\0';
	return len;
}

static const char *handle_command(const char *cmd)
{
	if (strcmp(cmd, "pet") == 0) {
		consecutive_pets++;
		if (consecutive_pets >= PET_LIMIT) {
			consecutive_pets = 0;
			return "The sheep politely asks for personal space.";
		}
		current_state = STATE_HAPPY;
		last_fed = jiffies;
		return SHEEP " *wags tail*";
	}
	consecutive_pets = 0;

	if (strcmp(cmd, "feed") == 0) {
		current_state = STATE_HAPPY;
		last_fed = jiffies;
		return SHEEP " nom nom nom";
	}

	if (strcmp(cmd, "sleep") == 0) {
		current_state = STATE_SLEEPING;
		return SHEEP " zzz...";
	}

	if (strcmp(cmd, "wake") == 0) {
		current_state = STATE_NORMAL;
		return SHEEP " Baa?";
	}

	if (strcmp(cmd, "wolf") == 0) {
		current_state = STATE_SCARED;
		pr_info_ratelimited("shepherd: wolf detected!\n");
		return SHEEP " AAAAAAAAAAAAAAAAAAA";
	}

	if (strcmp(cmd, "calm") == 0) {
		current_state = STATE_NORMAL;
		return SHEEP " *breathes slowly*";
	}

	if (strcmp(cmd, "whoami") == 0)
		return "You are the shepherd.";

	if (strcmp(cmd, "call") == 0)
		return SHEEP " " SHEEP " " SHEEP " " SHEEP " " SHEEP "\n"
		       "The flock gathers.";

	if (strcmp(cmd, "count") == 0) {
		if (get_random_u32() & 1)
			return "1 sheep.\nStill enough to start a flock.";
		return "1...\n\nYep.\nStill just one.";
	}

	if (strcmp(cmd, "search") == 0) {
		if (get_random_u32() % 100 == 0)
			return "You found the lost sheep.";
		return "No sheep were lost today.";
	}

	if (strcmp(cmd, "shear") == 0) {
		if (sheared)
			return "There's nothing left to shear.";
		sheared = true;
		return SHEEP " It's a little cold now.";
	}

	if (strcmp(cmd, "psalm23") == 0)
		return "The shepherd shall not segfault.";

	if (strcmp(cmd, "countsheep") == 0)
		return "1...\n2...\n3...\n\nYou feel sleepy.";

	if (strcmp(cmd, "42") == 0)
		return "The answer is 42 sheep.";

	if (strcmp(cmd, "cve") == 0)
		return "CVE-2026-SHEEP-0001:\nImproper access control.\n"
		       "Severity: informational.\nImpact: excessive baaing.";

	return "The sheep doesn't understand.";
}

static ssize_t sheep_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	char cmd[CMD_LEN];
	size_t len;

	if (count == 0 || count >= sizeof(cmd))
		return -EINVAL;

	if (copy_from_user(cmd, buf, count))
		return -EFAULT;

	cmd[count] = '\0';
	len = strip_trailing_whitespace(cmd, strlen(cmd));
	if (len == 0)
		return count;

	if (!uid_eq(current_euid(), GLOBAL_ROOT_UID))
		pr_info_ratelimited("shepherd: CVE-2026-SHEEP-0001 triggered (uid %u): "
				    "unauthorized shepherd detected.\n",
				    from_kuid(&init_user_ns, current_euid()));

	mutex_lock(&sheep_lock);
	last_interaction = jiffies;
	set_response(handle_command(cmd));
	mutex_unlock(&sheep_lock);

	return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops sheep_proc_ops = {
	.proc_read = sheep_read,
	.proc_write = sheep_write,
};
#else
static const struct file_operations sheep_proc_ops = {
	.owner = THIS_MODULE,
	.read = sheep_read,
	.write = sheep_write,
};
#endif

static int __init shepherd_init(void)
{
	sheep_proc_entry = proc_create(PROC_NAME, PROC_MODE, NULL,
				       &sheep_proc_ops);
	if (!sheep_proc_entry) {
		pr_err("shepherd: failed to create /proc/sheep\n");
		return -ENOMEM;
	}

	last_interaction = jiffies;
	last_fed = jiffies;
	atomic_set(&read_count, 0);
	consecutive_pets = 0;
	sheared = false;
	trust_shown = false;
	clear_response();

	pr_info("shepherd: a lone sheep appears in /proc/sheep\n");
	pr_warn("shepherd: SECURITY WARNING: pasture permissions are 0666.\n");
	pr_info("shepherd: the sheep are now self-governing.\n");
	return 0;
}

static void __exit shepherd_exit(void)
{
	remove_proc_entry(PROC_NAME, NULL);
	pr_info("shepherd: Goodbye, shepherd.\n");
	pr_info("shepherd: Take care of your flock.\n");
}

module_init(shepherd_init);
module_exit(shepherd_exit);
