/*  remind:   reminder program

    NAME
    remind - a reminder program

    SYNOPSIS
    remind  [-a] [-c colour_pairs] [-d date] [-D n[,n] ...] [-e]
    [-f filename] [-h] [-i] [-l] [-L] [-m n[,n] ...] [-p]
    [-P pointer] [-q] [-r repeat] [-s] [-t timeout]
    [-u urgency] [-v] [-w warning] [-X n[,n] ...]
    [message]

    See remind(1) man page for more.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>

#include "datafile.h"
#include "date.h"

#define REMIND_ENV "REMIND_FILE"
#define REMIND_FILE "remind.db"

enum {
    ABORT = 0,
    CONTINUE = 1,
    SECSPERDAY = 86400,
    ERRMSGSIZE = 132
};

enum cmd_type {
    CMD_DEFINE,
    CMD_DELETE,
    CMD_DISPLAY,
    CMD_DUMP,
    CMD_EXPORT,
    CMD_INIT,
    CMD_LIST,
    CMD_LIST_HEADER,
    CMD_MODIFY,
    CMD_MOD_POINTER
};

/* Structure for action number list */
struct st_nlist {
    int n;
    struct st_nlist *next;
};

/* command line parameters */
struct st_params {
    char* filename;
    enum cmd_type cmd;
    int set_type;
    int ucol[URGCOL];
    bool colour_set;
    int urgency;
    bool quiet;
    char* hilite;
    int pointer;
    bool version;
    struct st_nlist* actlist;
};

typedef struct st_params PARAMS;

/* datafile error messages matching error codes */
char* error_msg[] = {
    "unused",
    "no such file: %s",
    "record [%03d]: bad seek",
    "record [%03d]: bad read",
    "record [%03d]: bad write",
    "action [%03d] does not exist",
    "unable to create remind file: %s",
    "that's no remind file: %s",
    "action [%03d] is on free list",
    "action [%03d] can't be found on its list"
};

/* utility functions and procedures */

void error(int abort, char *fmt, ...)
{
    va_list ap;
    char errmsg[ERRMSGSIZE+1];

    va_start(ap,fmt);
    vsnprintf(errmsg,ERRMSGSIZE,fmt,ap);
    va_end(ap);
    fprintf(stderr,"remind: %s\n",errmsg);
    if (abort == ABORT) exit(EXIT_FAILURE);
    return;
}

/* return true if filename exists */
bool exists(char* filename)
{
    struct stat statbuf;

    return (stat(filename,&statbuf) == 0);
}

/* confirm file initialisation */
void ask(char* filename)
{
    char c;

    printf("remind: initialise %s.  are you sure (y/n)? ",filename);
    c = getchar();
    if (c != 'Y' && c != 'y') {
        error(ABORT,"initialisation aborted");
    }
    return;
}

/* strip leading or trailing double quote character */
char *strdq(char* s)
{
    int i;

    i = strlen(s)-1;
    if (*(s+i) == '"') *(s+i) = '\0';
    if (*s == '"') return s+1;
    return s;
}

/* Return string to starting high-lighting, or empty string if
 * hilite_off is empty.  Colour codes are standard ANSI. */
char* hilite_on(int u, char* hilite_off)
{
    static char hl[32];

    hl[0] = '\0';
    if (strlen(hilite_off) != 0 && u >= 0 && u < URGCOL) {
        int* ucol = rem_get_hilite();
        snprintf(hl, sizeof(hl)-1, "\033[%d;%dm",ucol[u*2-2],ucol[u*2-1]);
    }
    return hl;
}

/* Build action number list from comma separated string */
struct st_nlist *parse_int_list(char* s)
{
    struct st_nlist *np,*head;

    head = NULL;
    while (s) {
        np = (struct st_nlist *) calloc(1,sizeof(struct st_nlist));
        if (np == NULL) {
            error(ABORT,"insufficient memory for list");
        }
        np->n = atoi(s);
        np->next = head;
        head = np;
        s = strchr(s,',');
        if (s != NULL) s++;
    }
    return head;
}

/* return repeat parameter string */
char* repeat_str(struct st_repeat repeat)
{
    static char repstr[MSGSIZ+1];
    char* rtype = "ymwn";

    snprintf(repstr,MSGSIZ,"%c%d,%d",rtype[repeat.type],
             repeat.day, repeat.nday);
    return repstr;
}

bool parse_repeat(char* rstr, enum repeat_type* type, int* day, int* nday)
{
    int ntoks;
    char typech;

    *nday = 1; /* default */
    ntoks = sscanf(rstr,"%c%d,%d",&typech,day,nday);
    switch (typech) {
    case 'y':
        *type = RT_YEAR;
        break;
    case 'm':
        *type = RT_MONTH;
        break;
    case 'w':
        *type = RT_WEEK;
        if (ntoks < 2) return false;
        break;
    case 'n':
        *type = RT_MONTH_WEEK;
        if (ntoks < 2) return false;
        break;
    default:
        return false;
    }
    return true;
}

/* main functions */

bool parse_cmd_args(int argc, char *argv[], PARAMS* params, ACTREC* newact)
{
    int nargs;
    bool first_word = true;
    char *s, *np, *c, *term;
    char *switcharg = "dwuDmxtfcPXr"; /* switches that have arguments */

    /* set effective time? */
    if ((s = getenv("REMIND_TIME"))) date_set_time(s);

    /* set defaults */
    params->cmd = CMD_DISPLAY;
    params->quiet = false;
    params->version = false;
    params->urgency = -1;
    params->colour_set = false;
    params->set_type = ACT_PERIODIC|ACT_STANDARD;
    params->hilite = "";
    params->actlist = NULL;
    params->filename = getenv(REMIND_ENV);
    if (params->filename == NULL) params->filename = REMIND_FILE;
    for (int i=0;i<URGCOL;i++) params->ucol[i] = ((i%2==0)?37:40);

    newact->type = ACT_STANDARD;
    newact->time = 0;
    newact->timeout = -1;
    newact->urgency = -1;
    newact->warning = -1;
    newact->msg[0] = '\0';
    newact->repeat.type = EOF;
    newact->repeat.day = 0;
    newact->repeat.nday = 0;

    while (--argc > 0 && (*++argv)[0] == '-') {
        for (s=argv[0]+1;*s != '\0'; s++) {
            if (strchr(switcharg,*s) && argc == 1) {
                error(ABORT,"switch -%c requires argument",*s);
            }
            switch (*s) {
            case 'a':
                params->set_type = ACT_PERIODIC|ACT_STANDARD;
                break;
            case 'c':
                for (int i=0;i<URGCOL;i+=2) {
                    nargs = sscanf(*++argv,"%d,%d",&(params->ucol[i]),
                                   &(params->ucol[i+1]));
                    --argc;
                    if (nargs == EOF || nargs != 2)
                        error(ABORT,"bad colour pair");
                }
                params->colour_set = true;
                break;
            case 'd':
                if ((newact->time = date_parse(*++argv,TIME_EOD)) <= 0) {
                    error(ABORT,"bad date format");
                }
                if (params->set_type != ACT_STANDARD)
                    newact->type = ACT_PERIODIC;
                --argc;
                break;
            case 'D':
                params->actlist = parse_int_list(*++argv);
                if (!params->actlist) error(ABORT,"bad message list");
                params->cmd = CMD_DELETE;
                --argc;
                break;
            case 'e':
                params->cmd = CMD_EXPORT;
                break;
            case 'f':
                params->filename = *++argv;
                --argc;
                break;
            case 'h':
                /* set highlight off sequence */
                params->hilite = "\033[0m";
                break;
            case 'i':
                params->cmd = CMD_INIT;
                break;
            case 'l':
                params->cmd = CMD_LIST;
                break;
            case 'L':
                params->cmd = CMD_LIST_HEADER;
                break;
            case 'm':
                params->actlist = parse_int_list(*++argv);
                if (!params->actlist) error(ABORT,"bad message list");
                params->cmd = CMD_MODIFY;
                params->quiet = true; /* silence on redefined action */
                --argc;
                break;
            case 'p':
                newact->type = ACT_PERIODIC;
                params->set_type = ACT_PERIODIC;
                break;
            case 'P':
                params->pointer = atoi(*++argv);
                params->cmd = CMD_MOD_POINTER;
                --argc;
                break;
            case 'q':
                params->quiet = true;
                break;
            case 'r':
                if (!parse_repeat(*++argv,&(newact->repeat.type),
                                  &(newact->repeat.day),
                                  &(newact->repeat.nday)))
                    error(ABORT,"bad repeat option");
                --argc;
                newact->type = ACT_PERIODIC;
                break;
            case 's':
                newact->type = ACT_STANDARD;
                params->set_type = ACT_STANDARD;
                break;
            case 't':
                newact->timeout = atoi(*++argv);
                if (newact->timeout < 0) error(ABORT,"bad timeout value");
                --argc;
                break;
            case 'u':
                params->urgency = atoi(*++argv);
                if (params->urgency < 0 || params->urgency > 4) {
                    error(ABORT,"bad urgency value");
                }
                newact->urgency = params->urgency;
                --argc;
                break;
            case 'v':
                params->version = true;
                break;
            case 'w':
                newact->warning = atoi(*++argv);
                if (newact->warning < 0 ) error(ABORT,"bad warning value");
                --argc;
                break;
            case 'X':
                params->actlist = parse_int_list(*++argv);
                if (!params->actlist) error(ABORT,"bad dump list");
                --argc;
                params->cmd = CMD_DUMP;
                break;
            default:
                error(ABORT,"illegal option \"-%c\"\n",*s);
            }
        }
    }
    /* assume non-switch argument is message */
    while (argc-- > 0) {
        s = strdq(*argv++);
        /* Check first msg word for date format, if time not already set */
        if (first_word && newact->time == 0) {
            newact->time = date_parse(s,TIME_EOD);
            if (newact->time < 0) {
                error(ABORT,"bad date format");
            }
            else if (newact->time > 0) {
                if (params->set_type != ACT_STANDARD)
                    newact->type = ACT_PERIODIC;
                s = strchr(s,' ');    /* find next token */
                if (s == NULL) continue; /* wasn't one */
                s++; /* start of next token */
            }
            first_word = false;
        }
        strncat(newact->msg,s,MSGSIZ-strlen(newact->msg));
        if (strlen(newact->msg) == MSGSIZ) {
            error(ABORT,"message too long");
            return false;
        }
        strncat(newact->msg," ",MSGSIZ-strlen(newact->msg));
    }
    *(newact->msg+strlen(newact->msg)-1) = '\0';
    /* presence of a message means define an action, unless
     * something other than DISPLAY requested */
    if (strlen(newact->msg) != 0 && params->cmd == CMD_DISPLAY) {
        params->cmd = CMD_DEFINE;
    }
    /* use defaults for values unset */
    if (params->cmd != CMD_MODIFY) {
        if (newact->timeout < 0) newact->timeout = 0;
        if (newact->urgency < 0 )newact->urgency = 4;
        if (newact->warning < 0 ) newact->warning = 7;
        if (newact->repeat.type == EOF) newact->repeat.type = RT_YEAR;
        if (newact->time == 0) newact->time = date_now_eod();
    }
    return true;
}

/* return struct tm* to next event date of action where repeat is Mth
 * occurrence of Nth weekday. */
struct tm* mw_ev_time(time_t* now_time, struct st_repeat repeat, int month)
{
    struct tm* ev_tm;
    time_t fd_time;

    ev_tm = localtime(now_time);
    ev_tm->tm_mday = 1;
    ev_tm->tm_mon = month;
    fd_time = mktime(ev_tm);
    ev_tm = localtime(&fd_time);
    /* calculate mday of event */
    ev_tm->tm_mday = (repeat.day - ev_tm->tm_wday) +
        ((repeat.nday-1)*7) + 1 +
        ((repeat.day < ev_tm->tm_wday)?7:0);
    fd_time = mktime(ev_tm);
    return ev_tm;
}


time_t make_active_time(ACTREC* action)
{
    double delta;
    int day_diff, now_wday, now_mon, period, now_mday;
    struct tm* ev_tm, *now;

    time_t event_time, nowtime, fd_time, act_time;

    nowtime = date_now();
    switch (action->repeat.type) {
    case RT_YEAR:
        event_time = date_make_current(action->time,YEAR_ONLY);
        break;
    case RT_MONTH:
        event_time = date_make_current(action->time,YEAR_AND_MONTH);
        break;
    case RT_WEEK:
        delta = difftime(nowtime,action->time);
        if (delta < 0) {
            /* action time is in the future; just return it */
            event_time = action->time;
        }
        else {
            /* compute next event time */
            period = (action->repeat.nday==0?1:action->repeat.nday) * 7 *
                SECSPERDAY;
            delta = ceil(delta / SECSPERDAY) * SECSPERDAY;
            event_time = nowtime + period - ((int) delta)%period;
        }
        break;
    case RT_MONTH_WEEK:
        ev_tm = localtime(&nowtime);
        now_mon = ev_tm->tm_mon;
        now_mday = ev_tm->tm_mday;
        ev_tm = mw_ev_time(&nowtime,action->repeat,now_mon);
        if (ev_tm->tm_mon > now_mon) {
            /* calculated event day next month, look for last wday
             * in current month */
            while (ev_tm->tm_mon > now_mon) {
                ev_tm->tm_mday -= 7;
                event_time = mktime(ev_tm);
            }
        }
        else if (ev_tm->tm_mday < now_mday) {
            /* current day is later than this month's occurrence;
             * check next month */
            ev_tm = mw_ev_time(&nowtime,action->repeat,now_mon+1);
        }
        event_time = mktime(ev_tm);
        break;
    default:
        error(ABORT,"invalid repeat type found: %d",action->repeat.type);
    }
    return event_time;
}

void define_action(ACTREC* newact, int quiet)
{
    int newrecno;

    if (newact->repeat.type == RT_WEEK)
        newact->time =  date_make_days_match(newact->time,newact->repeat.day);

    if ((newrecno = act_define(newact)) < 0)
        error(ABORT,error_msg[rem_error()],-newrecno);
    if (!quiet) printf("remind: action [%03d] defined\n",newrecno);
    return;
}

void display(ACTYPE type, int urgency, bool quiet, char* hilite)
{
    int nhidden = 0, actno;
    ACTREC* action;
    time_t event_time;
    double delta;
    int delta_days;

    act_iter_init(type);
    actno = act_iter_next();
    while (actno != 0) {
        action = act_read(actno);
        if (action->timeout != 0 &&
            difftime(date_now(),action->time) >
            (action->timeout-1) * SECSPERDAY) {
            if (act_delete(actno, true) != 0)
                error(ABORT,error_msg[rem_error()], actno);
        }
        else if (urgency < 0 && action->urgency == 0) {
            nhidden++;
        }
        else {
            switch (type) {
            case ACT_STANDARD:
                if ((urgency < 0 ||
                     (urgency >= 0 && urgency == action->urgency)) &&
                    difftime(date_now(),action->time) > -SECSPERDAY) {
                    if (action->urgency == 0) action->urgency = 4;
                    printf("%s[%03d] %s%s\n",
                           hilite_on(action->urgency,hilite), actno,
                           action->msg,hilite);
                }
                break;
            case ACT_PERIODIC:
                event_time = make_active_time(action);
                delta = difftime(event_time,date_now());
                if (delta > 0 && delta <= (action->warning+1)*SECSPERDAY &&
                    (urgency < 0 ||
                     (urgency >= 0 && urgency == action->urgency))) {
                    delta_days = floor(delta/SECSPERDAY);
                    printf("%s[%03d] [%s]",
                           hilite_on(action->urgency, hilite),
                           actno, date_str(event_time));
                    if (delta_days == 1)
                        printf(" (tomorrow) ");
                    else if (delta_days == 0)
                        printf(" (today) ");
                    else
                        printf(" (%2d days) ",delta_days);
                    printf("%s%s\n",action->msg,hilite);
                }
                break;
            default:
                error(ABORT,"bad action type: %d",type);
            }
        }
        actno = act_iter_next();
    }
    if (urgency < 0 && nhidden > 0 && !quiet) {
        char *typestr = (type==ACT_STANDARD?"standard":"periodic");
        if (nhidden == 1)
            printf(">>>>> There is one background %s action\n",typestr);
        else
            printf(">>>>> There are %d background %s actions\n",nhidden,
                   typestr);
    }
    return;
}

void create_file(char* filename, int ucol[], int quiet)
{
    if (!quiet && exists(filename)) {
        ask(filename);
    }
    if (!rem_create(filename,ucol)) {
        error(ABORT, error_msg[rem_error()], filename);
    }
    return;
}

void dump_action(int actno)
{
    ACTREC* action;

    if ((action = act_read(actno)) != NULL) {
        printf("--Action: %d--\n",actno);
        printf("Type:    %s\n",str_act_type(action->type));
        printf("Next:    %d\n",action->next);
        printf("Urgency: %d\n",action->urgency);
        printf("Warning: %d\n",action->warning);
        printf("Date:    %s\n",date_str(action->time));
        printf("Repeat:  %s\n",repeat_str(action->repeat));
        printf("Timeout: %d\n",action->timeout);
        printf("Msg:     \"%s\"\n",action->msg);
    }
    else {
        error(CONTINUE, error_msg[rem_error()], actno);
    }
    return;
}

void delete_action(int actno, bool nullify)
{
    if ((act_delete(actno, nullify) != 0))
        error(CONTINUE, error_msg[rem_error()], actno);
    return;
}

void list_actions(enum cmd_type option, int set_type)
{
    REMHDR* header;
    ACTREC* action;

    header = rem_header();
    if (option == CMD_LIST_HEADER) {
        printf("P: %d  S: %d  F: %d  Num: %d [",header->phead, header->shead,
               header->fhead,header->numrec);
        for (int i=0; i<URGCOL; i+=2) {
            printf("%d,%d%s",header->ucol[i], header->ucol[i+1],
                   (i==URGCOL-2?"":" "));
        }
        printf("]\n");
    }
    for (int actno=1; actno < header->numrec; actno++) {
        if ((action = act_read(actno)) == NULL)
            error(ABORT, error_msg[rem_error()], actno);
        if ((option == CMD_LIST_HEADER && action->type == ACT_FREE) ||
            (action->type & set_type)) {
            printf("[%03d] %1d %1d %2d %s %2d %s \"%s\"\n",
                   actno, action->type,
                   action->urgency, action->warning, date_str(action->time),
                   action->timeout, repeat_str(action->repeat), action->msg);
        }
    }
}

void modify_action(int actno,ACTREC* newact)
{
    ACTREC* action;
    char *p, buf[MSGSIZ+1];
    ACTREC save;

    action = act_read(actno);
    if (action == NULL) {
        error(CONTINUE,error_msg[rem_error()],actno);
        return;
    }
    if (action->type == ACT_FREE) {
        error(CONTINUE,error_msg[RE_ACTIONTYPE],actno);
        return;
    }

    /* action points to static storage */
    save = *action;
    if (newact->warning >= 0) save.warning = newact->warning;
    if (newact->urgency >= 0) save.urgency = newact->urgency;
    if (newact->timeout >= 0) save.timeout = newact->timeout;
    if (newact->time > 0) save.time = newact->time;
    if (newact->repeat.type != EOF) save.repeat = newact->repeat;

    if (strlen(newact->msg) != 0) {
        p = strchr(newact->msg,'&');
        if (p != NULL) {
            *p++ = '\0';
            buf[0]='\0';
            strncat(buf,newact->msg,MSGSIZ);
            strncat(buf,action->msg,MSGSIZ-strlen(buf));
            strncat(buf,p,MSGSIZ-strlen(buf));
            strncpy(newact->msg,buf,MSGSIZ);
        }
        strncpy(save.msg,newact->msg,MSGSIZ);
    }
    /* force action time to match day of week of WEEK repeat */
    if (save.repeat.type == RT_WEEK)
        save.time = date_make_days_match(save.time,save.repeat.day);

    if ((newact->urgency >= 0 && save.type == ACT_STANDARD) ||
        (newact->time > 0 && save.type == ACT_PERIODIC)) {
        /* need to re-insert action into list; potential sequence change */
        delete_action(actno, false);
        define_action(&save,true);
    }
    else {
        if (act_write(actno,&save) != 0)
            error(CONTINUE, error_msg[rem_error()], actno);
    }
}

void modify_action_pointer(int actno, int pointer)
{
    ACTREC* action;

    if ((action = act_read(actno)) == NULL)
        error(ABORT,error_msg[rem_error()],actno);
    action->next = pointer;
    act_write(actno, action);
}

enum {
    DAYMONSTRLEN = 5
};

void export(char* filename)
{
    ACTREC* action;
    REMHDR* header;
    int actno;
    char daymon[DAYMONSTRLEN+1];

    if ((header = rem_header()) == NULL) error(ABORT,error_msg[rem_error()]);

    printf("remind -iq -f %s -c ",filename);
    for (int i=0;i<URGCOL;i+=2)
        printf("%d,%d ",header->ucol[i],header->ucol[i+1]);
    printf("\n");

    if (!act_iter_init(ACT_STANDARD)) error(ABORT,"action interator failed");
    actno = act_iter_next();
    while (actno) {
        if ((action = act_read(actno)) == NULL)
            error(ABORT,error_msg[rem_error()],actno);
        printf("remind -fuwtqsd %s %d %d %d %s \"%s\"\n", filename,
               action->urgency, action->warning, action->timeout,
               date_str(action->time), action->msg);
        actno = act_iter_next();
    }

    if (!act_iter_init(ACT_PERIODIC)) error(ABORT,"action interator failed");
    actno = act_iter_next();
    while (actno) {
        if ((action = act_read(actno)) == NULL)
            error(ABORT,error_msg[rem_error()],actno);
        strncpy(daymon, date_str(action->time), DAYMONSTRLEN);
        daymon[DAYMONSTRLEN] = '\0';
        printf("remind -fruwtdq %s %s %d %d %d %s \"%s\"\n", filename,
               repeat_str(action->repeat),  action->urgency, action->warning,
               action->timeout, daymon, action->msg);
        actno = act_iter_next();
    }
}

bool perform_cmd(PARAMS* params, ACTREC* newact)
{
    struct st_nlist* actno;
    int errno;

    if (params->cmd != CMD_INIT && !rem_open(params->filename))
        error(ABORT,error_msg[rem_error()],params->filename);
    if (params->colour_set) rem_set_hilite(params->ucol);

    if (params->version) {
        printf("remind %s\n",GIT_VERSION);
    }

    switch (params->cmd) {
    case CMD_DEFINE:
        define_action(newact,params->quiet);
        break;
    case CMD_DELETE:
        actno = params->actlist;
        while (actno != NULL) {
            delete_action(actno->n, params->quiet);
            actno = actno->next;
        }
        break;
    case CMD_DISPLAY:
        if (params->set_type & ACT_PERIODIC) {
            display(ACT_PERIODIC, params->urgency, params->quiet,
                    params->hilite);
        }
        if (params->set_type & ACT_STANDARD) {
            display(ACT_STANDARD, params->urgency, params->quiet,
                    params->hilite);
        }
        break;
    case CMD_DUMP:
        actno = params->actlist;
        while (actno != NULL) {
            dump_action(actno->n);
            actno = actno->next;
        }
        break;
    case CMD_EXPORT:
        export(params->filename);
        break;
    case CMD_INIT:
        create_file(params->filename, params->ucol,
                    params->quiet);
        break;
    case CMD_LIST:
    case CMD_LIST_HEADER:
        list_actions(params->cmd,params->set_type);
        break;
    case CMD_MODIFY:
        actno = params->actlist;
        while (actno != NULL) {
            modify_action(actno->n,newact);
            actno = actno->next;
        }
        break;
    case CMD_MOD_POINTER:
        actno = params->actlist;
        while (actno != NULL) {
            modify_action_pointer(actno->n,params->pointer);
            actno = actno->next;
        }
        break;
    default:
        error(ABORT,"internal command error: %d",params->cmd);
    }
    if (rem_cls() == EOF) error(ABORT,"close failed: %s",params->filename);
    if ((errno = rem_error()) != 0) error(ABORT,error_msg[errno],0);
    return true;
}

int main(int argc,char *argv[])
{
    ACTREC newact;
    PARAMS params;

    if (parse_cmd_args(argc, argv, &params, &newact) &&
        perform_cmd(&params, &newact)) {
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
