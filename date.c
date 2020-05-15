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
    fake_time = date_parse(s);
    if (fake_time < 0) fake_time = 0;
    return;
}

/* Parse date in the form dd/mm[/yyyy] */
time_t date_parse(char *s)
{
    int nread, now_year, century;
    struct tm* date;
    time_t nowtime;

    nowtime = date_now();
    date = localtime(&nowtime);
    date->tm_sec = 59;
    date->tm_min = 59;
    date->tm_hour = 23;
    now_year = date->tm_year;
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
        (date->tm_mon < 1 || date->tm_mon > 12) ||
        (date->tm_year < now_year)) return -1; /* malformed date */
    date->tm_mon--;
    return mktime(date);
}

/* Convert periodic action time to current year and, if month is
 * non-zero, current month */
time_t date_make_current(time_t t, int month)
{
    struct tm* date;
    time_t nowtime;
    int cur_year, cur_mon, cur_mday;

    nowtime = date_now();
    date = localtime(&nowtime);
    cur_year = date->tm_year;
    cur_mon = date->tm_mon;
    cur_mday = date->tm_mday;
    date = localtime(&t);
    date->tm_year = cur_year;
    if (month) {
        date->tm_mon = (date->tm_mday>=cur_mday)?cur_mon:cur_mon+1;
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

#define DATESTRSIZE 11

char* date_str(time_t t)
{
    struct tm* st;
    static char dstr[DATESTRSIZE+1];

    st = localtime(&t);
    snprintf(dstr,DATESTRSIZE,"%02d/%02d/%02d", st->tm_mday, st->tm_mon+1,
             st->tm_year+1900);
    return dstr;
}
