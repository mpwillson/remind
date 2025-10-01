#include <stdlib.h>
#include <stdio.h>

#include "date.h"

static time_t fake_time = 0;

time_t date_now(void)
{
    return (fake_time?fake_time:time((time_t*)NULL));
}

void date_set_time(char *s)
{
    fake_time = date_parse(s,TIME_CURRENT);
    if (fake_time < 0) fake_time = 0;
    return;
}

/* return time_t at end of current day */
time_t date_now_eod(void)
{
    struct tm* tm;
    time_t nowtime;

    nowtime = date_now();
    tm = localtime(&nowtime);
    tm->tm_hour = 23;
    tm->tm_min = 59;
    tm->tm_sec = 59;
    return mktime(tm);
}

/* Parse date in the form dd/mm[/yyyy] If eod is non-zero, returned
 * time is set at end of day (23:59:59), otherwise current time of day
 * is used.
*/
time_t date_parse(char *s, int eod)
{
    int nread, century;
    struct tm* date;
    time_t nowtime;

    nowtime = (eod?date_now_eod():date_now());
    date = localtime(&nowtime);
    century = date->tm_year%100;
    nread = sscanf(s,"%d/%d/%d",&(date->tm_mday),&(date->tm_mon),
                   &(date->tm_year));
    if (nread == EOF || nread < 2 ) return 0; /* not a date */
    if (nread == 3) {
        if (date->tm_year < 100 && date->tm_year != 0) {
            /* two digit year specified */
            date->tm_year += century*100;
        }
        else {
            date->tm_year -= 1900;
        }
    }
    if ((date->tm_mday < 1 || date->tm_mday > 31) ||
        (date->tm_mon < 1 || date->tm_mon > 12)) return -1; /* malformed date */
    date->tm_mon--;
    /* let mktime figure out if date is dst */
    date->tm_isdst = -1;
    return mktime(date);
}

/* Convert periodic action time to current year and, if month is
 * non-zero, current month */
time_t date_make_current(time_t t, int month, time_t base_time)
{
    struct tm* date;
    int cur_year, cur_mon, cur_mday;

    date = localtime(&base_time);
    cur_year = date->tm_year;
    cur_mon = date->tm_mon;
    cur_mday = date->tm_mday;
    date = localtime(&t);
    date->tm_year = cur_year;
    if (month) {
        date->tm_mon = (date->tm_mday>=cur_mday)?cur_mon:cur_mon+1;
        /* new month, different dst? */
        date->tm_isdst = -1;
    }
    else {
        time_t event_time = mktime(date);
        if (difftime(event_time,base_time) < 0) {
            /* event_time in the past; push to next year, to allow for
             * crossing year boundaries */
            date->tm_year++;
        }
    }
    return mktime(date);
}

time_t date_make_days_match(time_t t, int wday)
{
    struct tm* st;
    int day_diff;

    st = localtime(&t);
    day_diff = wday - st->tm_wday;
    if (day_diff == 0) return t;
    if (day_diff < 0) day_diff += 7;
    st->tm_mday += day_diff;
    return mktime(st);

}

enum {
    DATESTRSIZE = 11,
    DATEFULLSTRSIZE = 64
};

char* date_str(time_t t)
{
    struct tm* st;
    static char dstr[DATESTRSIZE];

    st = localtime(&t);
    if (strftime(dstr, DATESTRSIZE, "%d/%m/%Y", st) == 0)
        return "01/01/1900";
    return dstr;
}

char* date_full_str(time_t t)
{
    struct tm* st;
    static char dstr[DATEFULLSTRSIZE];

    st = localtime(&t);
    if (strftime(dstr, DATEFULLSTRSIZE, "%d/%m/%YT%R:%S %Z", st) == 0)
        return "01/01/1900T00:00:00 GMT";
    return dstr;
}
