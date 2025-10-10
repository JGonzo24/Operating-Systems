#ifndef SCHEDULERSH
#define SCHEDULERSH

#include <lwp.h>
extern scheduler AlwaysZero;
extern scheduler ChangeOnSIGTSTP;
extern scheduler ChooseHighestColor;
extern scheduler ChooseLowestColor;


void   init(void);            /* initialize any structures     */
void   shutdown (void);        /* tear down any structures      */
void   admit(thread new);     /* add a thread to the pool      */
void   remove(thread victim); /* remove a thread from the pool */
thread next(void);            /* select a thread to schedule   */
int    qlen(void);            /* number of ready threads       */

#endif



