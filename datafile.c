#include <stdio.h>
#include <string.h>

#include "datafile.h"

enum {
    SEEKOK = 0
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
    if (fseek(actfile,(long) (sizeof(action)*recno),0) != SEEKOK) {
        return SEEK;
    }
    if (fread(dest,(recno==0?sizeof(header):sizeof(action)),1,actfile) != 1) {
        return READ;
    }
    return 0;
}

static int rec_write(int recno, void* data)
{
    if (fseek(actfile,(long) sizeof(action)*recno,0) != SEEKOK) {
        return (rem_error_code = SEEK);
    }
    if (fwrite(data,(recno==0?sizeof(header):sizeof(action)),1,actfile) != 1) {
        return (rem_error_code = WRITE);
    }
    return 0;
}

/* Read action record at recno, return pointer to result. Note that
 * pointer refers to static data, so may be overwritten.  Save the
 * data needed. */
ACTREC* act_read(int recno)
{
    if (recno < 0 || recno > header.numrec) {
        rem_error_code = RECNO;
        return NULL;
    }
    if ((rec_read(recno,(void*) &action)) == 0) {
        return &action;
    }
    else {
        return NULL;
    }
}

int act_write(int recno, void* data)
{
    return rec_write(recno,data);
}

int  rem_cls(void)
{
    act_write(0,&header);
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
        strcpy(header.magic,MAGIC);
        if (ucol) rem_set_hilite(ucol);
        rec_write(0,(void*) &header);
    }
    else {
        rem_error_code = CREATE;
    }
    return actfile != NULL;
}

bool rem_open(char* filename)
{
    actfile = fopen(filename,"r+");
    if (actfile) {
        rec_read(0,(void*) &header);
        if (strcmp(header.magic,MAGIC) != 0) {
            rem_error_code = VERSION;
            fclose(actfile);
            actfile = NULL;
        }
    }
    else {
        rem_error_code = OPEN;
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
    ACTREC* act = NULL;

    if (next_rec != 0) act = act_read(next_rec);
    if (act != NULL) {
        actno = next_rec;
        next_rec = act->next;
    }
    return actno;
}


int act_define(ACTREC* newact)
{
    int actrec,last,current,*listhead;
    ACTREC* activerec;

    /* find a free record */
    if (header.fhead == 0)
        actrec = header.numrec++;
    else {
        actrec = header.fhead;
        activerec = act_read(actrec);
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
        activerec = act_read(current);
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
        *listhead = actrec;
    }
    else {
        if (current == 0) { /* inserting at end of list */
            activerec->next = actrec;
            act_write(last,activerec);
            newact->next = 0; /* end of list marker  */
        }
        else { /* inserting in middle of list */
            activerec = act_read(last);
            activerec->next = actrec;
            act_write(last,activerec);
            newact->next = current;
        }
    }
    act_write(actrec,newact);
    return actrec;
}

int act_delete(int del_action)
{
    int actrec,last,current;
    ACTREC* action;

    /* valid record number? */
    if (del_action <= 0 || del_action >= header.numrec) {
        rem_error_code = RECNO;
        return RECNO;
    }

    /* determine action type */
    action = act_read(del_action);
    if (action->type != ACT_PERIODIC && action->type != ACT_STANDARD) {
        rem_error_code = ACTIONTYPE;
        return ACTIONTYPE;
    }

    last = 0;
    /* search correct list */
    if (action->type == ACT_STANDARD)
        current = header.shead;
    else
        current = header.phead;

    while (current) {
        action = act_read(current);
        if (current == del_action) break;
        last = current;
        current = action->next;
    }
    if (last == 0) {  /* list empty or deleting head */
        if (action->type == ACT_STANDARD)
            header.shead = action->next;
        else
            header.phead = action->next;
    }
    else {
        if (current == 0) { /* action not on list! */
            rem_error_code = LIST;
            return LIST;
        }
        else { /* deleting in middle of list */
            actrec = action->next;
            action = act_read(last);
            action->next = actrec;
            act_write(last,action);
        }
    }
    action->next = header.fhead;
    action->type = ACT_FREE;
    action->warning = 0;
    action->urgency = 0;
    action->time = 0;
    action->timeout = 0;
    strcpy(action->msg,".");
    act_write(current,action);
    header.fhead = current;
    return 0;
}

