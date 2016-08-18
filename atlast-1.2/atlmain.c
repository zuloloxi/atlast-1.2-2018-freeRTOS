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
#include "atldef.h"

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

static void ctrlc( int sig) {
    if (sig == SIGINT)
        atl_break();
}
#endif /* HIGHC */


/*  MAIN  --  Main program.  */
#define DEFER     ": defer create ['] abort , does> @ execute ;"
#define DEFER_AT  ": defer@ >body @ ;"
#define DEFER_PUT ": defer!  >body ! ;"
#define DEFER_IS  ": is ' defer!  ; immediate"



void install_defer() {
    int rc = atl_eval( (char *)DEFER );
}

void install_defer_at() {
    int rc = atl_eval( (char *)DEFER_AT );
}

void install_defer_put() {
    int rc = atl_eval( (char *)DEFER_PUT );
}

void install_defer_is() {
    int rc = atl_eval( (char *)DEFER_IS );
}

void install_secondaries() {
    install_defer();
    install_defer_at();
    install_defer_put();
    install_defer_is();
}

void atlast_init() {
    int rc;

    install_secondaries();

    rc = atl_eval((char *)": to ' >body ! ; immediate");
    rc = atl_eval((char *)"defer emit");
    rc = atl_eval((char *)"' (emit) is  emit");

    rc = atl_eval((char *)"defer ?emit");
    rc = atl_eval((char *)"' (?emit) is ?emit");

    rc = atl_eval((char *)"defer key");
    rc = atl_eval((char *)"' (key) is key");

    rc = atl_eval((char *)"defer ?key");
    rc = atl_eval((char *)"' (?key) is ?key");
}

int main(int argc, char *argv[]) {
    int i;
    int fname = FALSE, defmode = FALSE;
    FILE *ifp;
    char *include[20];
    int in = 0;
    char t[132];  // command line buffer
    extern bool initIO();

    memset(t,0,sizeof(t));

#define PR(x) (void) fprintf(stderr, x)

    PR("ATLAST 1.2 (2007-10-07) This program is in the public domain.\n");
    printf("Compiled: %s\n",__DATE__);

    ifp = stdin;
    for (i = 1; i < argc; i++) {
        char *cp, opt;

        cp = argv[i];
        if (*cp == '-') {
            opt = *(++cp);
            if (islower(opt))
                opt = toupper(opt);
            switch (opt) {

                case 'C':
                    strcpy(t,(cp+1));
                    break;
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
                    PR("           -Ccmd  Execute 'cmd' once started up.");
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

    atlast_init();
    if (false == initIO()) {
        fprintf(stderr,"Fatel error initialising IO\n");
    }


    bool cmdRun = false;
    while (TRUE) {
        //        char t[132];

        if (!fname)
            V printf(atl_comment ? "(  " :  /* Show pending comment */
                    /* Show compiling state */
                    (((heap != NULL) && state) ? ":> " : "-> "));

        if( cmdRun == false) {
            if (strlen(t) > 0) {

                V atl_eval(t);
                memset(t,0,sizeof(t));
            }
            cmdRun=true;
        }

        // TODO: Replace fgets with a common, forth like word
        // to allow simple redirection.
        //
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
