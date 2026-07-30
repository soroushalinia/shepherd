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
#include <linux/printk.h>

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

	last_interaction = jiffies;

	if (strcmp(cmd, "pet") == 0) {
		consecutive_pets++;
		if (consecutive_pets >= PET_LIMIT) {
			set_response("The sheep politely asks for personal space.");
			consecutive_pets = 0;
			return count;
		}
		current_state = STATE_HAPPY;
		last_fed = jiffies;
		set_response(SHEEP " *wags tail*");
		return count;
	}
	consecutive_pets = 0;

	if (strcmp(cmd, "feed") == 0) {
		current_state = STATE_HAPPY;
		last_fed = jiffies;
		set_response(SHEEP " nom nom nom");
		return count;
	}

	if (strcmp(cmd, "sleep") == 0) {
		current_state = STATE_SLEEPING;
		set_response(SHEEP " zzz...");
		return count;
	}

	if (strcmp(cmd, "wake") == 0) {
		current_state = STATE_NORMAL;
		set_response(SHEEP " Baa?");
		return count;
	}

	if (strcmp(cmd, "wolf") == 0) {
		current_state = STATE_SCARED;
		pr_info("shepherd: wolf detected!\n");
		set_response(SHEEP " AAAAAAAAAAAAAAAAAAA");
		return count;
	}

	if (strcmp(cmd, "calm") == 0) {
		current_state = STATE_NORMAL;
		set_response(SHEEP " *breathes slowly*");
		return count;
	}

	if (strcmp(cmd, "whoami") == 0) {
		set_response("You are the shepherd.");
		return count;
	}

	if (strcmp(cmd, "call") == 0) {
		set_response(SHEEP " " SHEEP " " SHEEP " " SHEEP " " SHEEP "\n"
			     "The flock gathers.");
		return count;
	}

	if (strcmp(cmd, "count") == 0) {
		if (get_random_u32() & 1)
			set_response("1 sheep.\nStill enough to start a flock.");
		else
			set_response("1...\n\nYep.\nStill just one.");
		return count;
	}

	if (strcmp(cmd, "search") == 0) {
		if (get_random_u32() % 100 == 0)
			set_response("You found the lost sheep.");
		else
			set_response("No sheep were lost today.");
		return count;
	}

	if (strcmp(cmd, "shear") == 0) {
		if (!sheared) {
			sheared = true;
			set_response(SHEEP " It's a little cold now.");
		} else {
			set_response("There's nothing left to shear.");
		}
		return count;
	}

	if (strcmp(cmd, "psalm23") == 0) {
		set_response("The shepherd shall not segfault.");
		return count;
	}

	if (strcmp(cmd, "countsheep") == 0) {
		set_response("1...\n2...\n3...\n\nYou feel sleepy.");
		return count;
	}

	if (strcmp(cmd, "42") == 0) {
		set_response("The answer is 42 sheep.");
		return count;
	}

	set_response("The sheep doesn't understand.");
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
