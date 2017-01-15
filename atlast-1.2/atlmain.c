/*			      ATLMAIN.C

                  Main driver program for interactive ATLAST

                  Designed and implemented in January of 1990 by John Walker.

                  This program is in the public domain.

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <signal.h>
#include <stdbool.h>
#include "atldef.h"
#include "atlcfig.h"

#ifdef PUBSUB
#include "Small.h"
#include "linuxParser.h"
#include "tasks.h"

#ifdef PTHREAD
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#endif
#endif

#define FALSE	0
#define TRUE	1

#define V   (void)

/*  Globals imported  */

#ifndef HIGHC

/*  CTRLC  --  Catch a user console break signal.  If your C library
    does not provide this Unix-compatibile facility
    (registered with the call on signal() in main()),
    just turn this code off or, better still, replace it
    with the equivalent on your system.  */

static void ctrlc(int sig) {
    if (sig == SIGINT)
        atl_break();
}
#endif /* HIGHC */


#ifdef PUBSUB
#ifdef PTHREAD
#warning "Define lock"
pthread_mutex_t lock;

pthread_t tid[2];
struct Small *table;

void doSmallCallback(struct nlist *rec, uint8_t idx) {
    struct client *tmp;
    void *fred;

    printf("doSmallCallback\n");

    tmp=(struct client *)nlistGetSubscriber(rec,idx);

    if(tmp != NULL) {
        if( tmp->pipe == NULL ) {
            mqd_t mq = mq_open( tmp->name, O_WRONLY) ;
            if( mq == (mqd_t)-1) {
                perror("doSmallCallback");
            } else {
                tmp->pipe = mq;
            }
        } 
        struct cmdMessage subMessage;

        memset(&subMessage,0,sizeof(struct cmdMessage));
        subMessage.message.fields = 3;
        strncpy(subMessage.message.cmd,"SET",MAX_CMD);
        strncpy(subMessage.message.key,nlistGetName(rec),MAX_KEY);
        strncpy(subMessage.message.value,nlistGetDef(rec),MAX_VALUE);

        int rc=mq_send(tmp->pipe,&subMessage,sizeof(struct cmdMessage),NULL);
    }
}

extern struct linuxParser *newParser();

void *doSmall(void *arg) {
    bool runFlag=true;
    char *queueName ;
    mqd_t mq;
    struct mq_attr attr;
    ssize_t len;
    struct cmdMessage buffer;
    char *res;
    
    struct linuxParser *p;

    bool ff;

    ff=setGlobalCallback(table, doSmallCallback);
    
    // This lock is held by this threads parent.
    // Once the parent has completed its setup it will release the lock, and
    // then we can continue.
    //
    pthread_mutex_lock(&lock);
    fprintf(stderr,"Started\n");

    queueName = dbLookup(table,"QNAME");
    if(!queueName) {
        ff=addRecord(table,"QNAME","/atlast");
        if(ff) {
            fprintf(stderr,"db install faulure, fatal error\n");
            exit(1);
        }
        queueName = dbLookup(table,"QNAME");
    }

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    printf("size %d\n", sizeof(struct cmdMessage));
    attr.mq_msgsize = sizeof(struct cmdMessage);
    attr.mq_curmsgs = 0;

    mq = mq_open(queueName, O_CREAT | O_RDONLY, 0644, &attr);
    mq_setattr(mq, &attr,NULL);

    p=newParser(table);
    
    setIam(p,queueName);

    while(runFlag) {
        len = mq_receive(mq, &buffer, sizeof(buffer), NULL);
        if( len < 0) {
            perror("mq_receive");
        } else {
            res=cmdParse(p,&buffer);
        }
    }

}
#endif
#endif

/*  MAIN  --  Main program.  */

int main(int argc, char *argv[]) {
    int i;
    int fname = FALSE, defmode = FALSE;
    FILE *ifp;
    char *include[20];
    int in = 0;

    int *tst;

#define PR(x) (void) fprintf(stderr, x)

#ifdef BANNER
    PR("ATLAST 1.2 (2007-10-07) This program is in the public domain.\n");
    printf("Compiled: %s\n",__DATE__);
#endif

    ifp = stdin;
    for (i = 1; i < argc; i++) {
        char *cp, opt;

        cp = argv[i];
        if (*cp == '-') {
            opt = *(++cp);
            if (islower(opt))
                opt = toupper(opt);
            switch (opt) {

                case 'D':
                    defmode = TRUE;
                    break;

                case 'H':
                    atl_heaplen = atol(cp + 1);
                    break;

                case 'I':
                    include[in++] = cp + 1;
                    break;

                case 'R':
                    atl_rstklen = atol(cp + 1);
                    break;

                case 'S':
                    atl_stklen = atol(cp + 1);
                    break;

                case 'T':
                    atl_trace = TRUE;
                    break;

                case '?':
                case 'U':
                    PR("Usage:  ATLAST [options] [inputfile]\n");
                    PR("        Options:\n");
                    PR("           -D     Treat file as definitions\n");
                    PR("           -Hn    Heap length n\n");
                    PR("           -Ifile Include named definition file\n");
                    PR("           -Rn    Return stack length n\n");
                    PR("           -Sn    Stack length n\n");
                    PR("           -T     Set TRACE mode\n");
                    PR("           -U     Print this message\n");
                    return 0;
            }
        } else {
            char fn[132];

            if (fname) {
                PR("Duplicate file name.\n");
                return 1;
            }
            fname = TRUE;
            V strcpy(fn, cp);
            if (strchr(fn, '.') == NULL)
                V strcat(fn, ".atl");
            ifp = fopen(fn, "r");
            if (ifp == NULL) {
                V fprintf(stderr, "Unable to open file %s\n", fn);
                return 1;
            }
        }
    }

    // OK, so init the system
    //
    atl_init();
    int8_t len=0;
    uint8_t lineBuffer[MAX_LINE];
    char t[132];
#ifdef PUBSUB
    table = newSmall();
#ifdef PTHREAD
#warning Pthreads
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("pthread_mutex_init");
        exit(1);
    }

    pthread_mutex_lock(&lock);
    int err = pthread_create(&(tid[i]), NULL, &doSmall, NULL);
#endif
#ifdef LINUX
    sprintf(t,"0x%x constant TABLE",table);
    atl_eval(t);
    memset(t,0x00,sizeof(t));
#endif


#endif


    /* If any include files were named, load each in turn before
       we execute the program. */

    for (i = 0; i < in; i++) {
        int stat;
        char fn[132];
        FILE *fp;

        V strcpy(fn, include[i]);
        if (strchr(fn, '.') == NULL)
            V strcat(fn, ".atl");
        fp = fopen(fn,
#ifdef FBmode
                "rb"
#else
                "r"
#endif
                );
        if (fp == NULL) {
            V fprintf(stderr, "Unable to open include file %s\n",
                    include[i]);
            return 1;
        }
        stat = atl_load(fp);
        V fclose(fp);
        if (stat != ATL_SNORM) {
            V printf("\nError %d in include file %s\n", stat, include[i]);
        }
    }

    /* Now that all the preliminaries are out of the way, fall into
       the main ATLAST execution loop. */

#ifndef HIGHC
    V signal(SIGINT, ctrlc);
#endif /* HIGHC */

    //    tst = (int *) atl_body(rf);
    while (true) {

        if (!fname)
            V printf(atl_comment ? "(  " :  /* Show pending comment */
                    /* Show compiling state */
                    (((heap != NULL) && state) ? ":> " : "-> "));
        if (fgets(t, 132, ifp) == NULL) {
            if (fname && defmode) {
                fname = defmode = FALSE;
                ifp = stdin;
                continue;
            }
            break;
        }
        V atl_eval(t);
    }
    if (!fname)
        V printf("\n");
    return 0;
}
