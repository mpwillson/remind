/*  remind:   reminder program

    NAME
        remind - a reminder program

    SYNOPSIS
        remind  [-a] [-c colour_pairs] [-d date] [-D n[,n] ...] [-e]
                [-f filename] [-h] [-i] [-l] [-L] [-m n[,n] ...] [-p]
                [-P pointer] [-q] [-r repeat] [-s] [-t timeout]
                [-u urgency] [-w warning] [-X n[,n] ...]
                [message]

    See remind(1) man page for more.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "datafile.h"
#include "date.h"

#define ABORT 0
#define CONTINUE 1
#define REMIND_ENV "REMIND_FILE"
#define REMIND_FILE "remind.db"
#define SECSPERDAY 86400

enum cmd_type {
    CMD_ALL,
    CMD_PERIODIC,
    CMD_STANDARD,
    CMD_DELETE,
    CMD_DEFINE,
    CMD_MODIFY,
    CMD_EXPORT,
    CMD_MOD_POINTER,
    CMD_INIT,
    CMD_LIST,
    CMD_LIST_HEADER,
    CMD_DUMP
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
    enum act_type forced;
    int ucol[URGCOL];
    bool colour_set;
    int urgency;
    bool quiet;
    char* hilite;
    int pointer;
    struct st_nlist* actlist;
};

typedef struct st_params PARAMS;

#define ERRMSGSIZE 132

/* datafile error messages matching error codes */
char* error_msg[] = {
    "unused",
    "no such file: %s",
    "bad seek for action: %d",
    "bad read for action: %d",
    "bad write for action: %d",
    "invalid action record number: %d",
    "unable to create remind file: %s",
    "that's no remind file: %s",
    "action has invalid type: %d",
    "action can't be found on list: %d"
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

/* convert string to lower case (not i18n compliant) */
void strlower(char* s)
{
    s--;
    while (*(++s)) if (*s >= 'A' && *s <= 'Z')   *s = *s | 040;
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

bool parse_repeat(char* rstr,enum repeat_type* type, int* day, int* nday)
{
    int ntoks;
    char typech;

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
            if (ntoks != 3) return false;
            break;
        default:
            return false;
    }
    return true;
}

/* main functions */

bool parse_cmd_args(int argc, char *argv[], PARAMS* params, ACTREC* newact)
{
    int k;
    bool first_token = true;
    char *s, *np, *c, *term;
    char *switcharg = "dwuDmxtfcPXr"; /* switches that have arguments */

    /* set defaults */
    params->cmd = CMD_ALL;
    params->quiet = false;
    params->urgency = -1;
    params->colour_set = false;
    params->forced = 0;
    params->hilite = "";
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
    params->actlist = NULL;
    params->filename = getenv(REMIND_ENV);
    if (params->filename == NULL) params->filename = REMIND_FILE;

    while (--argc > 0 && (*++argv)[0] == '-') {
        for (s=argv[0]+1;*s != '\0'; s++) {
            if (strchr(switcharg,*s) && argc == 1) {
                error(ABORT,"switch -%c requires argument",*s);
            }
            switch (*s) {
                case 'a':
                    params->cmd = CMD_ALL;
                    break;
                case 'c':
                    for (int i=0;i<URGCOL;i+=2) {
                        k = sscanf(*++argv,"%d,%d",&(params->ucol[i]),
                                   &(params->ucol[i+1]));
                        --argc;
                        if (k == EOF || k != 2) error(ABORT,"bad colour pair");
                    }
                    params->colour_set = true;
                    break;
                case 'd':
                    if ((newact->time = date_parse(*++argv)) <= 0) {
                        error(ABORT,"bad date format");
                    }
                    if (params->forced != ACT_STANDARD)
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
                    params->cmd = CMD_PERIODIC;
                    newact->type = ACT_PERIODIC;
                    params->forced = ACT_PERIODIC;
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
                    params->cmd = CMD_STANDARD;
                    newact->type = ACT_STANDARD;
                    params->forced = ACT_STANDARD;
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
        /* Check first token for date format, if time not already set */
        if (first_token && newact->time == 0) {
            first_token = false;
            newact->time = date_parse(s);
            if (newact->time < 0) {
                error(ABORT,"bad date format");
            }
            else if (newact->time != 0) {
                if (!params->forced) newact->type = ACT_PERIODIC;
                s = strchr(s,' ');    /* find next token */
                if (s == NULL) continue; /* wasn't one */
                s++; /* start of next token */
            }
        }
        strncat(newact->msg,s,MSGSIZ-strlen(newact->msg));
        if (strlen(newact->msg) == MSGSIZ) {
            error(ABORT,"message too long");
            return false;
        }
        strncat(newact->msg," ",MSGSIZ-strlen(newact->msg));
    }
    *(newact->msg+strlen(newact->msg)-1) = '\0';
    /* presence of a message overrides display type */
    if (strlen(newact->msg) != 0 && params->cmd <= CMD_STANDARD) {
        params->cmd = CMD_DEFINE;
    }
    /* use defaults for values unset */
    if (params->cmd != CMD_MODIFY) {
        if (newact->timeout < 0) newact->timeout = 0;
        if (newact->urgency < 0 )newact->urgency = 4;
        if (newact->warning < 0 ) newact->warning = 5;
        if (newact->time == 0) newact->time = date_now();
        if (newact->repeat.type == EOF) newact->repeat.type = RT_YEAR;
    }
    return true;
}

time_t make_active_time(ACTREC* action)
{
    double delta;
    int day_diff, now_wday, now_mon;
    struct tm* ev_tm, *now;

    time_t event_time, nowtime, fd_time, act_time;

    nowtime = date_now();
    switch (action->repeat.type) {
        case RT_YEAR:
            event_time = date_make_current(action->time,0);
            break;
        case RT_MONTH:
            event_time = date_make_current(action->time,1);
            break;
        case RT_WEEK:
            now = localtime(&nowtime);
            now_wday = now->tm_wday;
            day_diff = action->repeat.day - now_wday;
            if (day_diff <= 0) day_diff += 7;
            event_time = nowtime + (day_diff * SECSPERDAY);
            break;
        case RT_MONTH_WEEK:
            ev_tm = localtime(&nowtime);
            now_mon = ev_tm->tm_mon;
            ev_tm->tm_mday = 1;
            fd_time = mktime(ev_tm);
            ev_tm = localtime(&fd_time);
            /* calculate mday of event (seems to work) */
            ev_tm->tm_mday = (action->repeat.day - ev_tm->tm_wday) +
                ((action->repeat.nday-1)*7) + 1 +
                ((action->repeat.day < ev_tm->tm_wday)?7:0);
            event_time = mktime(ev_tm);
            /* check if date within current month; backtrack if not */
            while (ev_tm->tm_mon > now_mon) {
                ev_tm->tm_mday -= 7;
                event_time = mktime(ev_tm);
            }
            break;
        default:
            error(ABORT,"invalid repeat type found: %d",action->repeat.type);
    }
    return event_time;
}

void define_action(ACTREC* newact, int quiet)
{
    int newrecno;

    newrecno = act_define(newact);
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
            difftime(action->time,date_now()) > action->timeout*SECSPERDAY) {
            if (act_delete(actno) != 0)
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
                        printf("%s[%03d] %-74s%s\n",
                               hilite_on(action->urgency,hilite), actno,
                               action->msg,hilite);
                    }
                    break;
                case ACT_PERIODIC:
                    event_time = make_active_time(action);
                    delta = difftime(event_time,date_now());
                    if (delta > 0 && delta <= action->warning*SECSPERDAY) {
                        delta_days = delta/SECSPERDAY;
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
    if (type == ACT_STANDARD && urgency < 0 && nhidden > 0 && !quiet) {
        if (nhidden == 1)
            printf(">>>>> There is one background action\n");
       else
            printf(">>>>> There are %d background actions\n",nhidden);
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

void dump_action(int act)
{
    ACTREC* action;

    if ((action = act_read(act)) != NULL) {
        printf("--Action: %d--\n",act);
        printf("Type:    %d\n",action->type);
        printf("Next:    %d\n",action->next);
        printf("Urgency: %d\n",action->urgency);
        printf("Warning: %d\n",action->warning);
        printf("Date:    %s\n",date_str(action->time));
        printf("Repeat:  %s\n",repeat_str(action->repeat));
        printf("Timeout: %d\n",action->timeout);
        printf("Msg:     \"%s\"\n",action->msg);
    }
    else {
        error(CONTINUE, error_msg[rem_error()], act);
    }
    return;
}

void delete_action(int actno)
{
    if ((act_delete(actno) != 0)) error(CONTINUE,error_msg[rem_error()],actno);
    return;
}

void list_actions(enum cmd_type option, enum act_type forced)
{
    REMHDR* header;
    ACTREC* action;

    header = rem_header();
    if (option == CMD_LIST_HEADER) {
        printf("P: %d  S: %d  F: %d  Num: %d  ",header->phead, header->shead,
               header->fhead,header->numrec);
        for (int i=0; i<URGCOL; i+=2) {
            printf("(%d,%d) ",header->ucol[i], header->ucol[i+1]);
        }
        printf("\n");
    }
    for (int i=1; i<=header->numrec-1; i++) {
        action = act_read(i);
        if (action == NULL) error(ABORT,error_msg[rem_error()],i);
        if ((action->type == ACT_FREE && option == CMD_LIST_HEADER) ||
            (forced == 0 && action->type != ACT_FREE) ||
            (action->type == forced)) {
            printf("[%03d] %1d %1d %2d %s %2d %s \"%s\"\n",
                   i, action->type,
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

    if ((newact->urgency >= 0 && save.type == ACT_STANDARD) ||
        (newact->time > 0 && save.type == ACT_PERIODIC)) {
        /* need to re-insert action into list; potential sequence change */
        delete_action(actno);
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

void export(char* filename)
{
    ACTREC* action;
    REMHDR* header;
    int actno;


    if ((header = rem_header()) == NULL) error(ABORT,error_msg[rem_error()]);

    printf("remind -iY -f %s -c ",filename);
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
        printf("remind -fruwtdq %s %s %d %d %d %s \"%s\"\n", filename,
               repeat_str(action->repeat),  action->urgency, action->warning,
               action->timeout, date_str(action->time), action->msg);
        actno = act_iter_next();
    }
}

bool perform_cmd(PARAMS* params, ACTREC* newact)
{
    struct st_nlist* actno;

    if (params->cmd != CMD_INIT && !rem_open(params->filename))
        error(ABORT,error_msg[rem_error()],params->filename);
    if (params->colour_set) rem_set_hilite(params->ucol);

    switch (params->cmd) {
        case CMD_ALL:
            display(ACT_STANDARD, params->urgency, params->quiet,
                    params->hilite);
            display(ACT_PERIODIC, params->urgency, params->quiet,
                    params->hilite);
            break;
        case CMD_DEFINE:
            define_action(newact,params->quiet);
            break;
        case CMD_DELETE:
            actno = params->actlist;
            while (actno != NULL) {
                delete_action(actno->n);
                actno = actno->next;
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
            list_actions(params->cmd,params->forced);
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
        case CMD_PERIODIC:
            display(ACT_PERIODIC, params->urgency, params->quiet,
                    params->hilite);
            break;
        case CMD_STANDARD:
            display(ACT_STANDARD, params->urgency, params->quiet,
                    params->hilite);
            break;
        default:
            error(ABORT,"internal command error: %d",params->cmd);
    }
    rem_cls();
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
