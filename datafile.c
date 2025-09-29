#include <stdio.h>
#include <string.h>

#include "datafile.h"

enum {
    RE_SEEKOK = 0
};

static FILE* actfile;

static ACTREC action;
static REMHDR header;

static int rem_error_code;

int rem_error(void) {
    return rem_error_code;
}

REMHDR* rem_header(void)
{
    return &header;
}

static int rec_read(int recno, void* dest)
{
    if (fseek(actfile,(long) (sizeof(action)*recno),0) != RE_SEEKOK) {
        return  (rem_error_code = RE_SEEK);
    }
    if (fread(dest,(recno==0?sizeof(header):sizeof(action)),1,actfile) != 1) {
        return (rem_error_code = RE_READ);
    }
    return 0;
}

static int rec_write(int recno, void* data)
{
    if (fseek(actfile,(long) sizeof(action)*recno,0) != RE_SEEKOK) {
        return (rem_error_code = RE_SEEK);
    }
    if (fwrite(data,(recno==0?sizeof(header):sizeof(action)),1,actfile) != 1) {
        return (rem_error_code = RE_WRITE);
    }
    return 0;
}

/* Read action record at recno, return pointer to result. Note that
 * pointer refers to static data, so may be overwritten.  Save the
 * data needed. */
ACTREC* act_read(int recno)
{
    if (recno < 1 || recno > header.numrec) {
        rem_error_code = RE_RECNO;
        return NULL;
    }
    if (rec_read(recno, &action) == 0) {
        return &action;
    }
    else {
        return NULL;
    }
}

int act_write(int recno, ACTREC* data)
{
    if (recno < 1 || recno > header.numrec) {
        rem_error_code = RE_RECNO;
        return RE_RECNO;
    }
    return rec_write(recno,data);
}

int  rem_cls(void)
{
    rem_error_code = 0;
    rec_write(0,&header);
    return fclose(actfile);
}


bool rem_set_hilite(int ucol[])
{
    for (int i=0;i<URGCOL;i++) header.ucol[i] = ucol[i];
    return true;
}

int* rem_get_hilite(void)
{
    return (int*) &(header.ucol);
}

bool rem_create(char* filename, int ucol[]) {
    actfile = fopen(filename,"w");
    if (actfile) {
        header.phead=header.shead=header.fhead = 0;
        header.numrec = 1;
        strncpy(header.magic,MAGIC,sizeof(header.magic)-1);
        if (ucol) rem_set_hilite(ucol);
    }
    else {
        rem_error_code = RE_CREATE;
    }
    return actfile != NULL;
}

bool rem_open(char* filename)
{
    actfile = fopen(filename,"r+");
    if (actfile) {
        rec_read(0,&header);
        if (strcmp(header.magic,MAGIC) != 0) {
            if (strncmp,header.magic, MAGIC, 3) {
                rem_error_code = RE_VERSION;
            }
            else {
                rem_error_code = RE_BADDB;
            }
            fclose(actfile);
            actfile = NULL;
        }
    }
    else {
        rem_error_code = RE_OPEN;
    }
    return actfile != NULL;
}

static int next_rec = 0;

bool act_iter_init(ACTYPE type)
{
    switch (type) {
        case ACT_STANDARD:
            next_rec = header.shead;
            break;
        case  ACT_PERIODIC:
            next_rec = header.phead;
            break;
        case ACT_FREE:
            next_rec = header.fhead;
            break;
        default:
            return false;
    }
    return true;
}

int act_iter_next()
{
    int actno = 0;
    ACTREC* activerec = &action;

    if (next_rec != 0 && rec_read(next_rec,activerec) == 0) {
        actno = next_rec;
        next_rec = activerec->next;
    }
    return actno;
}

/* returns action number defined.  If negative, i/o error ocurred. */
int act_define(ACTREC* newact)
{
    int actno,last,current,*listhead;
    ACTREC* activerec = &action;

    /* find a free record */
    if (header.fhead == 0)
        actno = header.numrec++;
    else {
        actno = header.fhead;
        if (rec_read(actno,activerec) != 0) return -actno;
        header.fhead = activerec->next;
    }

    /* insert action into correct list */
    if (newact->type == ACT_STANDARD) {
        listhead = &header.shead;
        current = header.shead;
    }
    else {
        listhead = &header.phead;
        current = header.phead;
    }

    last = 0;
    while (current) {
        if (rec_read(current, activerec) != 0) return -current;
        if (newact->type == ACT_STANDARD &&
            activerec->urgency > newact->urgency) break; /* urgency
                                                            order */
        else if (newact->type == ACT_PERIODIC) {
            if (newact->time < activerec->time) break; /* date order */
        }
        last = current;
        current = activerec->next;
    }

    if (last == 0) {  /* list empty or inserting at head */
        newact->next = current;
        *listhead = actno;
    }
    else {
        if (current == 0) { /* inserting at end of list */
            activerec->next = actno;
            if (rec_write(last,activerec) != 0) return -last;
            newact->next = 0; /* end of list marker  */
        }
        else { /* inserting in middle of list */
            if (rec_read(last,activerec) != 0) return -last;
            activerec->next = actno;
            if (rec_write(last,activerec) != 0) return -last;
            newact->next = current;
        }
    }
    if (rec_write(actno,newact) != 0) return -actno;
    return actno;
}

int act_delete(int del_actno, bool nullify)
{
    int actno,last,current;
    ACTREC* activerec = &action;
    ACTREC save;

    if (del_actno <= 0) {
        rem_error_code = RE_RECNO;
        return RE_RECNO;
    }

    /* determine action type */
    if (rec_read(del_actno,activerec) != 0) return rem_error_code;
    if (activerec->type == ACT_FREE) {
        rem_error_code = RE_ACTIONTYPE;
        return RE_ACTIONTYPE;
    }

    /* search correct list */
    if (activerec->type == ACT_STANDARD)
        current = header.shead;
    else
        current = header.phead;

    last = 0;
    while (current) {
        if (rec_read(current,activerec) != 0) return rem_error_code;
        if (current == del_actno) break;
        last = current;
        current = activerec->next;
    }
    if (last == 0) {  /* list empty or deleting head */
        if (activerec->type == ACT_STANDARD)
            header.shead = activerec->next;
        else
            header.phead = activerec->next;
    }
    else {
        if (current == 0) { /* action not on list! */
            rem_error_code = RE_LIST;
            return RE_LIST;
        }
        else { /* deleting in middle of list */
            save = *activerec;             /* preserve action data */
            actno = activerec->next;
            if (rec_read(last, activerec) != 0) return rem_error_code;
            activerec->next = actno;
            if (rec_write(last,activerec) != 0) return rem_error_code;
            *activerec = save;
        }
    }
    activerec->next = header.fhead;
    activerec->type = ACT_FREE;
    if (nullify) {
        activerec->warning = 0;
        activerec->urgency = 0;
        activerec->time = 0;
        activerec->timeout = 0;
        activerec->msg[0] = '.';
        memset(activerec->msg+1,'\0',MSGSIZ);
    }
    if (rec_write(current,activerec) != 0) return current;
    header.fhead = current;
    return 0;
}

char* str_act_type(int act_type)
{
    static char* act_str[NACT_TYPES] = {
        "Free","Periodic","Standard"
    };

    return (act_type < 0 || act_type >= NACT_TYPES)? "" : act_str[act_type];
}
