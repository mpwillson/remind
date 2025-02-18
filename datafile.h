#ifndef DATAFILE_H
#define DATAFILE_H

#include <stdbool.h>
#include <time.h>

#define MAGIC "rmd3"

enum {
    MSGSIZ = 80,
    URGCOL = 8
};

enum rem_errors {
    RE_OPEN = 1,
    RE_SEEK,
    RE_READ,
    RE_WRITE,
    RE_RECNO,
    RE_CREATE,
    RE_VERSION,
    RE_ACTIONTYPE,
    RE_LIST
};

enum act_type {
    ACT_FREE,
    ACT_PERIODIC,
    ACT_STANDARD,
    NACT_TYPES
};

enum repeat_type {
    RT_YEAR,
    RT_MONTH,
    RT_WEEK,
    RT_MONTH_WEEK
};

struct st_repeat {
    enum repeat_type type;
    int day;   /* day of week */
    int nday;  /* nth day of week */
};

struct st_remfile_hdr {
    char magic[8];  /* remind file identifier */
    int phead;      /* periodic action list pointer */
    int shead;      /* standard action list pointer */
    int fhead;      /* free list pointer */
    int numrec;     /* number of records in file */
    int ucol[URGCOL];/* urgency colour pairs */
};

struct st_action_rec {
    enum act_type type;      /* action type */
    int next;                /* pointer to next action on list */
    int urgency;             /* urgency of this action */
    int warning;             /* warning required for this action (periodic
                              * only) */
    time_t time;             /* action date/time */
    struct st_repeat repeat; /* repeat parameters */
    int timeout;            /* timeout value in days; zero for no timeout */
    int workday;            /* unused */
    char msg[MSGSIZ+1];     /* the action message */
};

typedef struct st_remfile_hdr REMHDR;
typedef struct st_action_rec ACTREC;
typedef enum act_type ACTYPE;

/* public function prototypes */
extern int rem_error(void);
extern ACTREC* act_read(int);
extern int act_write(int,ACTREC*);
extern int rem_cls(void);
extern bool rem_create(char*, int[]);
extern bool rem_open(char*);
extern bool act_iter_init(ACTYPE);
extern int act_iter_next(void);
extern bool rem_set_hilite(int[]);
extern int* rem_get_hilite(void);
extern int act_define(ACTREC*);
extern int act_delete(int, bool);
extern REMHDR* rem_header(void);
extern char* str_act_type(int);

#endif
