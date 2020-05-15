#ifndef DATE_H
#define DATE_H
#include <time.h>
#include <stdbool.h>

extern time_t date_now(void);
extern time_t date_parse(char*);
extern char* date_str(time_t);
extern time_t date_make_current(time_t,int);
extern time_t date_make_days_match(time_t,int);
extern void date_set_time(char*);

#endif
