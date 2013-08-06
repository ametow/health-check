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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>

#include "list.h"
#include "json.h"
#include "cpustat.h"
#include "health-check.h"

/*
 *  cpustat_loading()
 *	map CPU loading to some human understandable form
 */
static const char *cpustat_loading(const double cpu_percent)
{
	if (cpu_percent == 0.0)
		return "idle";
	if (cpu_percent > 99.0)
		return "1 CPU fully loaded";
	if (cpu_percent > 95.0)
		return "nearly 1 CPU fully loaded";
	if (cpu_percent > 85.0)
		return "excessive load";
	if (cpu_percent > 70.0)
		return "very high load";
	if (cpu_percent > 40.0)
		return "high load";
	if (cpu_percent > 20.0)
		return "medium load";
	if (cpu_percent > 10.0)
		return "slight load";
	if (cpu_percent > 2.5)
		return "light load";

	return "very light load";
}

/*
 *  cpustat_cmp()
 *	cpu total time list sort comparitor
 */
static int cpustat_cmp(const void *data1, const void *data2)
{
	cpustat_info_t	*cpustat1 = (cpustat_info_t *)data1;
	cpustat_info_t	*cpustat2 = (cpustat_info_t *)data2;

	return cpustat2->ttime - cpustat1->ttime;
}

/*
 *  cpustat_dump_diff()
 *	dump difference in CPU loading between two snapshots in time
 */
void cpustat_dump_diff(
	json_object *j_tests,
	const double duration,
	const list_t *cpustat_old,
	const list_t *cpustat_new)
{
	double nr_ticks =
		/* (double)sysconf(_SC_NPROCESSORS_CONF) * */
		(double)sysconf(_SC_CLK_TCK) *
		duration;
	uint64_t utime_total = 0, stime_total = 0, ttime_total = 0;
	int count = 0;
	link_t *lo, *ln;
	list_t	sorted;
	cpustat_info_t *cio, *cin;
#ifndef JSON_OUTPUT
	(void)j_tests;
#endif

	list_init(&sorted);

	for (ln = cpustat_new->head; ln; ln = ln->next) {
		cin = (cpustat_info_t*)ln->data;

		for (lo = cpustat_old->head; lo; lo = lo->next) {
			cio = (cpustat_info_t*)lo->data;

			if (cin->proc->pid == cio->proc->pid) {
				cpustat_info_t *cpustat;

				if ((cpustat = calloc(1, sizeof(*cpustat))) == NULL) {
					fprintf(stderr, "Out of memory\n");
					health_check_exit(EXIT_FAILURE);
				}
				cpustat->proc  = cio->proc;
				cpustat->utime = cin->utime - cio->utime;
				cpustat->stime = cin->stime - cio->stime;
				cpustat->ttime = cin->ttime - cio->ttime;
				list_add_ordered(&sorted, cpustat, cpustat_cmp);

				utime_total += cpustat->utime;
				stime_total += cpustat->stime;
				ttime_total += cpustat->ttime;
				count++;
			}
		}
	}

	printf("CPU usage:\n");
	if (opt_flags & OPT_BRIEF) {
		printf(" User: %6.2f%%, System: %6.2f%%, Total: %6.2f%% (%s)\n",
			100.0 * (double)utime_total / (double)nr_ticks,
			100.0 * (double)stime_total / (double)nr_ticks,
			100.0 * (double)ttime_total / (double)nr_ticks,
			cpustat_loading(100.0 * (double)ttime_total / (double)nr_ticks));
	} else {
		printf("  PID  Process                USR%%   SYS%%  TOTAL%%\n");
		for (ln = sorted.head; ln; ln = ln->next) {
			cin = (cpustat_info_t*)ln->data;
			printf(" %5d %-20.20s %6.2f %6.2f %6.2f (%s)\n",
				cin->proc->pid,
				cin->proc->cmdline,
				100.0 * (double)cin->utime / (double)nr_ticks,
				100.0 * (double)cin->stime / (double)nr_ticks,
				100.0 * (double)cin->ttime / (double)nr_ticks,
				cpustat_loading(100.0 * (double)cin->ttime / (double)nr_ticks));
		}
		if (count > 1)
			printf(" %-26.26s %6.2f %6.2f %6.2f (%s)\n",
				"Total",
				100.0 * (double)utime_total / (double)nr_ticks,
				100.0 * (double)stime_total / (double)nr_ticks,
				100.0 * (double)ttime_total / (double)nr_ticks,
				cpustat_loading(100.0 * (double)ttime_total / (double)nr_ticks));
	}

#ifdef JSON_OUTPUT
	if (j_tests) {
		json_object *j_cpustat, *j_cpuload, *j_cpu;

		j_obj_obj_add(j_tests, "cpu-load", (j_cpustat = j_obj_new_obj()));
		j_obj_obj_add(j_cpustat, "cpu-load-per-process", (j_cpuload = j_obj_new_array()));
		
		for (ln = sorted.head; ln; ln = ln->next) {
			cin = (cpustat_info_t*)ln->data;

			j_cpu = j_obj_new_obj();
			j_obj_new_int32_add(j_cpu, "pid", cin->proc->pid);
			j_obj_new_int32_add(j_cpu, "ppid", cin->proc->ppid);
			j_obj_new_int32_add(j_cpu, "is-thread", cin->proc->is_thread);
			j_obj_new_string_add(j_cpu, "name", cin->proc->cmdline);
			j_obj_new_int64_add(j_cpu, "user-cpu-ticks", cin->utime);
			j_obj_new_int64_add(j_cpu, "system-cpu-ticks", cin->stime);
			j_obj_new_int64_add(j_cpu, "total-cpu-ticks", cin->ttime);
			j_obj_new_double_add(j_cpu, "user-cpu-percent",
				100.0 * (double)cin->utime / (double)nr_ticks);
			j_obj_new_double_add(j_cpu, "system-cpu-percent", 
				100.0 * (double)cin->stime / (double)nr_ticks);
			j_obj_new_double_add(j_cpu, "total-cpu-percent",
				100.0 * (double)cin->ttime / (double)nr_ticks);
			j_obj_new_string_add(j_cpu, "load-hint",
				cpustat_loading(100.0 * (double)cin->ttime / (double)nr_ticks));
			j_obj_array_add(j_cpuload, j_cpu);
		}

		j_obj_obj_add(j_cpustat, "cpu-load-total", (j_cpu = j_obj_new_obj()));
		j_obj_new_double_add(j_cpu, "cpu-load-total",
			100.0 * (double)utime_total / (double)nr_ticks);
		j_obj_new_double_add(j_cpu, "user-cpu-percent",
			100.0 * (double)stime_total / (double)nr_ticks);
		j_obj_new_double_add(j_cpu, "system-cpu-percent",
			100.0 * (double)ttime_total / (double)nr_ticks);
	}
#endif

	list_free(&sorted, free);

	printf("\n");
}

/*
 *  cpustat_get()
 *
 */
int cpustat_get(const list_t *pids, list_t *cpustat)
{
	char filename[PATH_MAX];
	FILE *fp;
	link_t *l;

	for (l = pids->head; l; l = l->next) {
		proc_info_t *p = (proc_info_t *)l->data;

		if (p->is_thread)
			continue;

		snprintf(filename, sizeof(filename), "/proc/%d/stat", p->pid);
		if ((fp = fopen(filename, "r")) != NULL) {
			char comm[20];
			uint64_t utime, stime;
			pid_t pid;

			/* 3173 (a.out) R 3093 3173 3093 34818 3173 4202496 165 0 0 0 3194 0 */
			if (fscanf(fp, "%d (%[^)]) %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %" SCNu64 " %" SCNu64,
				&pid, comm, &utime, &stime) == 4) {
				cpustat_info_t *info;

				info = calloc(1, sizeof(*info));
				if (info == NULL) {
					fprintf(stderr, "Out of memory\n");
					health_check_exit(EXIT_FAILURE);
				}
				info->proc  = p;
				info->utime = utime;
				info->stime = stime;
				info->ttime = utime + stime;
				list_append(cpustat, info);
			}
			fclose(fp);
		}
	}

	return 0;
}
