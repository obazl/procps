/*
 * uptime - uptime related functions - part of procps
 *
 * Copyright (C) 1992-1998 Michael K. Johnson <johnsonm@redhat.com>
 * Copyright (C) ???? Larry Greenfield <greenfie@gauss.rutgers.edu>
 * Copyright (C) 1993 J. Cowley
 * Copyright (C) 1998-2003 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@enc.com.au>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include <proc/uptime.h>
#include <proc/sysinfo.h>
#include "procps-private.h"

#define UPTIME_FILE "/proc/uptime"

static __thread char upbuf[128];
static __thread char shortbuf[128];

static int count_users(void)
{
    int numuser = 0;
    struct utmp *ut;

    setutent();
    while ((ut = getutent())) {
	if ((ut->ut_type == USER_PROCESS) && (ut->ut_name[0] != '\0'))
	    numuser++;
    }
    endutent();

    return numuser;
}
/*
 * uptime:
 *
 * Find the uptime and idle time of the system.
 * These numbers are found in /proc/uptime
 * Unlike other procps functions this closes the file each time
 * Either uptime_secs or idle_secs can be null
 *
 * Returns: uptime_secs on success and <0 on failure
 */
PROCPS_EXPORT int uptime(double *restrict uptime_secs, double *restrict idle_secs)
{
    double up=0, idle=0;
    char *savelocale;
    char buf[256];
    FILE *fp;

    if ((fp = fopen(UPTIME_FILE, "r")) == NULL)
	return -errno;
    savelocale = strdup(setlocale(LC_NUMERIC, NULL));
    setlocale(LC_NUMERIC, "C");
    if (fscanf(fp, "%lf %lf", &up, &idle) < 2) {
	setlocale(LC_NUMERIC, savelocale);
	free(savelocale);
	fclose(fp);
	return -ERANGE;
    }
    fclose(fp);
    setlocale(LC_NUMERIC, savelocale);
    free(savelocale);
    if (uptime_secs)
	*uptime_secs = up;
    if (idle_secs)
	*idle_secs = idle;
    return up;
}

/*
 * sprint_uptime: 
 *
 * Print current time in nice format
 *
 * Returns a statically allocated upbuf or NULL on error
 */
PROCPS_EXPORT char *sprint_uptime(void)
{
    int upminutes, uphours, updays, users;
    int pos;
    time_t realseconds;
    struct tm *realtime;
    double uptime_secs, idle_secs;
    double av1, av5, av15;

    upbuf[0] = '\0';
    if (time(&realseconds) < 0)
	return upbuf;
    realtime = localtime(&realseconds);
    if (uptime(&uptime_secs, &idle_secs) < 0)
	return upbuf;

    updays  =   ((int) uptime_secs / (60*60*24));
    uphours =   ((int) uptime_secs / (60*60)) % 24;
    upminutes = ((int) uptime_secs / (60)) % 60;

    pos = sprintf(upbuf, " %02d:%02d:%02d up %d %s, ",
	    realtime->tm_hour, realtime->tm_min, realtime->tm_sec,
	    updays, (updays != 1) ? "days" : "day");
    if (uphours)
	pos += sprintf(upbuf + pos, "%2d:%02d, ", uphours, upminutes);
    else
	pos += sprintf(upbuf + pos, "%d min, ", uphours, upminutes);

    users = count_users();
    loadavg(&av1, &av5, &av15);

    pos += sprintf(upbuf + pos, "%2d user%s, load average: %.2f, %.2f, %.2f",
	    users, users == 1 ? "" : "s",
	    av1, av5, av15);

    return upbuf;
}

PROCPS_EXPORT char *sprint_uptime_short(void)
{
    int updecades, upyears, upweeks, updays, uphours, upminutes;
    int pos = 3;
    int comma = 0;
    time_t realseconds;
    struct tm *realtime;
    double uptime_secs, idle_secs;

    shortbuf[0] = '\0';
    if (uptime(&uptime_secs, &idle_secs) < 0)
	return shortbuf;

    updecades =  (int) uptime_secs / (60*60*24*365*10);
    upyears =   ((int) uptime_secs / (60*60*24*365)) % 10;
    upweeks =   ((int) uptime_secs / (60*60*24*7)) % 52;
    updays  =   ((int) uptime_secs / (60*60*24)) % 7;
    uphours =   ((int) uptime_secs / (60*24)) % 24;
    upminutes = ((int) uptime_secs / (60)) % 60;

    strcat(shortbuf, "up ");

    if (updecades) {
	pos += sprintf(shortbuf + pos, "%d %s",
		updecades, updecades > 1 ? "decades" : "decade");
	comma +1;
    }

    if (upyears) {
	pos += sprintf(shortbuf + pos, "%s%d %s",
		comma > 0 ? ", " : "", upyears,
		upyears > 1 ? "years" : "year");
	comma += 1;
    }

    if (upweeks) {
	pos += sprintf(shortbuf + pos, "%s%d %s",
		comma  > 0 ? ", " : "", upweeks,
		upweeks > 1 ? "weeks" : "week");
	comma += 1;
    }

    if (updays) {
	pos += sprintf(shortbuf + pos, "%s%d %s",
		comma  > 0 ? ", " : "", updays,
		updays > 1 ? "days" : "day");
	comma += 1;
    }

    if (uphours) {
	pos += sprintf(shortbuf + pos, "%s%d %s",
		comma  > 0 ? ", " : "", uphours,
		uphours > 1 ? "hours" : "hour");
	comma += 1;
    }

    if (upminutes) {
	pos += sprintf(shortbuf + pos, "%s%d %s",
		comma  > 0 ? ", " : "", upminutes,
		upminutes > 1 ? "minutes" : "minute");
	comma += 1;
    }
    return shortbuf;
}

