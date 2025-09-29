#ifndef DATE_H
#define DATE_H
#include <time.h>
#include <stdbool.h>

enum {
    TIME_CURRENT = 0,
    TIME_EOD = 1,
    YEAR_ONLY = 0,
    YEAR_AND_MONTH =1
};

extern time_t date_now(void);
extern time_t date_parse(char*,int);
extern char* date_str(time_t);
extern char* date_full_str(time_t);
extern time_t date_make_current(time_t,int,time_t);
extern time_t date_make_days_match(time_t,int);
extern void date_set_time(char*);
extern time_t date_now_eod(void);

#endif
