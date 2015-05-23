/*
 * Copyright (c) 2015 Kyle Isom <coder@kyleisom.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <libacpi.h>


static int	 battno = -1;
static global_t	*global = NULL;
static char	*progname = NULL;
static FILE	*statfile = NULL;
const char	 DEFAULT_BATTERY[] = "BAT0";
const char	 DEFAULT_STATFILE[] = ".battstat";
const size_t	 MAX_NAME_LEN  = 32;


static void
shutdown(int flag)
{
	flag = 0;
	exit(EXIT_SUCCESS);
}


/*
 * A snapshot stores the current battery and system state, along with
 * a timestamp.
 */
struct snapshot {
	uint32_t	percentage;
	uint32_t	state;
	uint64_t	when;
	struct sysinfo	sys;
};


/*
 * collect_stats creates a new snapshot structure and writes it to the
 * stats file.
 */
static void *
collect_stats(void *args)
{
	struct snapshot	 snap;
	char		*errors;

	args = NULL;

	snap.percentage = batteries[battno].percentage;
	snap.state = batteries[battno].charge_state;
	if  (-1 == sysinfo(&snap.sys)) {
		errors = strerror(errno);
		syslog(LOG_ERR, "failed to read system info: %s", errors);
		errors = NULL;
		return NULL;
	}

	snap.when = time(NULL);

	if (1 != fwrite(&snap, sizeof(struct snapshot), 1, statfile)) {
		syslog(LOG_ERR, "failed to write to snapshot file");
		return NULL;
	}

	if (0 != fflush(statfile)) {
		errors = strerror(errno);
		syslog(LOG_ERR, "failed to flush snapshot file: %s", errors);
		errors = NULL;
		return NULL;
	}

	syslog(LOG_INFO, "[%lu] wrote snapshot", snap.when);
	return NULL;
}


/*
 * usage prints a short usage message.
 */
static void
usage(void)
{
	printf("Usage: %s [-b battery] [-d] [-f statfile] [-h] [-t delay]\n",
	    progname);
	printf("\t-b battery\tSelect the battery name to monitor.\n");
	printf("\t-d\t\tDon't daemonise; run in the foreground.\n");
	printf("\t-f statfile\tSpecify the file to write statistics to.\n");
	printf("\t-h\t\tPrint this help message.\n");
	printf("\t-t delay\tSpecify the delay in seconds between updates.\n");
}


/*
 * battstat is a daemon to collect battery statistics.
 */
int
main(int argc, char *argv[])
{
	char		*bname = (char *)DEFAULT_BATTERY;
	char		*statpath = (char *)DEFAULT_STATFILE;
	pthread_t	 thread;
	int		 c = -1;
	int		 i  = 0;
	int		 nodaemon = 0;
	int		 logopts = LOG_CONS|LOG_PID;
	int		 delay = 60;

	progname = strndup(argv[0], PATH_MAX);
	if (NULL == progname) {
		err(EXIT_FAILURE, "strndup");
	}

	while (-1 != (c = getopt(argc, argv, "b:df:h"))) {
		switch (c) {
		case  'b':
			bname = optarg;
			break;
		case 'd':
			nodaemon  = 1;
			break;
		case 'f':
			statpath = optarg;
			break;
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 't':
			delay = atoi(optarg);
			break;
		default:
			/* NOTREACHED */
			usage();
			return EXIT_FAILURE;
		}
	}
	free(progname);

	if (nodaemon) {
		logopts |= LOG_PERROR;
	}
	openlog(progname, logopts, LOG_LOCAL0);

	global = malloc(sizeof(global_t));
	if (SUCCESS != check_acpi_support()) {
		errx(EXIT_FAILURE, "No ACPI support present.");
	}

	if (SUCCESS != init_acpi_batt(global)) {
		errx(EXIT_FAILURE, "Could not find battery.");
	}

	if (SUCCESS != read_acpi_batt(battno)) {
		errx(EXIT_FAILURE, "Failed to read battery info");
	}

	if (global->batt_count == 0) {
		errx(EXIT_FAILURE, "No batteries found.");
	}

	for (i = 0; i < global->batt_count; i++) {
		if (0  == strncmp(batteries[i].name, bname, MAX_NAME_LEN)) {
			battno = i;
			break;
		}
	}

	if (-1 == battno) {
		errx(EXIT_FAILURE, "Battery %s wasn't found.",
		    bname);
	}

	if (NULL == (statfile = fopen(statpath, "a"))) {
		err(EXIT_FAILURE, "opening stat file failed");
	}

	if (!nodaemon) {
		if (daemon(0, 0)) {
			err(EXIT_FAILURE, "failed to daemonise");
		}
		signal(SIGTTOU, SIG_IGN);
		signal(SIGTTIN, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);
		signal(SIGUSR1, shutdown);
	}

	while (1) {
		if (SUCCESS != init_acpi_batt(global)) {
			syslog(LOG_ERR, "could not find battery");
		}

		if (SUCCESS != read_acpi_batt(battno)) {
			syslog(LOG_ERR, "failed to read battery state");
			return EXIT_FAILURE;
		}

		if (0 != pthread_create(&thread, NULL, collect_stats, NULL)) {
			syslog(LOG_ERR, "failed to start a thread");
			exit(EXIT_FAILURE);
		}

		if (0 != pthread_detach(thread)) {
			syslog(LOG_ERR, "failed to detach thread");
			exit(EXIT_FAILURE);
		}
		sleep(delay);
	}

	return EXIT_SUCCESS;
}

