#ifndef DATE_H
#define DATE_H
#include <time.h>

extern time_t date_now(void);
extern time_t date_parse(char*);
extern char* date_str(time_t);
extern time_t date_make_current(time_t,int);

#endif
