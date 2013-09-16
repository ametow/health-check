/*
 * Copyright (C) 2013 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#define _GNU_SOURCE

#include <sys/time.h>

#include "proc.h"
#include "list.h"
#include "json.h"

#define MAX_BUCKET			(9)
#define BUCKET_START			(0.00001)

#define SYSCALL(n) \
	[SYS_ ## n] = { #n, SYS_ ## n, 0, NULL, NULL, NULL, true }

#define SYSCALL_TIMEOUT(n, arg, func_check, func_ret) \
	[SYS_ ## n] = { #n, SYS_ ## n, arg, &syscall_timeout[SYS_ ## n], func_check, func_ret, true }

#define SYSCALL_CHKARGS(n, arg, func_check, func_ret) \
	[SYS_ ## n] = { #n, SYS_ ## n, arg, &syscall_timeout[SYS_ ## n], func_check, func_ret, false }

#define TIMEOUT(n, timeout) \
	[SYS_ ## n] = timeout

/* Stash timeout related syscall return value */
typedef struct {
	double		timeout;	/* syscall timeout in seconds */
	int		ret;		/* syscall return */
} syscall_return_info_t;

/* Syscall polling stats for a particular process */
typedef struct syscall_info {
	proc_info_t	*proc;		/* process info */
	int 		syscall;	/* system call number */
	uint64_t	count;		/* number times call has been made */
	double 		poll_min;	/* minimum poll time */
	double		poll_max;	/* maximum poll time */
	double		poll_total;	/* sum of non zero or negative poll times */
	uint64_t	poll_count;	/* number of polls */
	uint64_t	poll_too_low;	/* number of poll times below a threshold */
	uint64_t	poll_infinite;	/* number of -ve (infinite) poll times */
	uint64_t	poll_zero;	/* number of zero poll times */
	uint64_t	bucket[MAX_BUCKET]; /* bucket count of poll times */
	list_t		return_history;	/* history system call returns */
	struct syscall_info *next;
} syscall_info_t;

typedef struct syscall syscall_t;

typedef void (*check_timeout_func_t)(const syscall_t *sc, syscall_info_t *s, const pid_t pid, const double threshold, double *timeout);
typedef void (*check_return_func_t)(json_object *j_tests, const syscall_t *sc, const syscall_info_t *s);

/* syscall specific information */
typedef struct syscall {
	char 		*name;		/* name of the syscall */
	int  		syscall;	/* system call number */
	int		arg;		/* nth arg to check for timeout value (1st arg is zero) */
	double		*threshold;	/* threshold - points to timeout array items indexed by syscall */
	check_timeout_func_t check_func;/* timeout checking function, NULL means don't check */
	check_return_func_t  check_ret; /* return checking function, NULL means don't check */
	bool		do_poll_accounting;/* true if we should do polling accounting stats for this */
} syscall_t;

/* fd cache */
typedef struct fd_cache {
	pid_t		pid;		/* process */
	int		fd;		/* file descriptor */
	char		*filename;	/* filename, NULL if closed */
	pthread_mutex_t	mutex;		/* mutex on filename */
	struct fd_cache	*next;		/* next one in cache */
} fd_cache_t;

typedef struct syscall_wakelock_info {
        pid_t		pid;		/* process */
	bool		locked;		/* true = lock, false = unlock */
	struct timeval	tv;		/* when locked/unlocked */
	char		*lockname;	/* wake lock name */
	struct syscall_wakelock_info *paired;	/* ptr to lock/unlock pair */
} syscall_wakelock_info_t;

typedef struct {
	pid_t		pid;		/* process */
	uint64_t	sync_count;	/* sync syscall count */
	uint64_t	fsync_count;	/* fsync syscall count */
	uint64_t	fdatasync_count;/* fdatasync syscall count */
	uint64_t	syncfs_count;	/* syncfs syscall count */
	uint64_t	total_count;	/* total count */
	list_t		sync_file;	/* list of files that were sync'd */
} syscall_sync_info_t;

typedef struct {
	char		*filename;
	uint64_t	count;
	int		syscall;
} syscall_sync_file_t;

typedef struct syscall_context {
	pid_t		pid;		/* process */
	proc_info_t	*proc;		/* proc info */
	int		syscall;	/* syscall detected */
	double		timeout;	/* timeout on poll syscalls */
	syscall_info_t	*syscall_info;	/* syscall accounting */
	bool		alive;		/* is traced thread alive */
	struct syscall_context *next;
} syscall_context_t;

extern syscall_t syscalls[];
extern size_t syscalls_len;
extern int procs_traced;

extern void *syscall_trace(void *arg);
extern void syscall_dump_hashtable(json_object *j_tests, const double duration);
extern void syscall_dump_pollers(json_object *j_tests, const double duration);
extern void syscall_init(void);
extern void syscall_cleanup(void);
extern void syscall_dump_wakelocks(json_object *j_tests, const double duration, list_t *pids);
extern void syscall_dump_sync(json_object *j_tests, double duration);
extern int syscall_trace_proc(list_t *pids);

#endif
