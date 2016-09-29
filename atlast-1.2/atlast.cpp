/** @file atlast.cpp
 *

   A T L A S T

   Autodesk Threaded Language Application System Toolkit

   Main Interpreter and Compiler

   Designed and implemented in January of 1990 by John Walker.

   This program is in the public domain.

*/
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <iostream>
using namespace std;
// #include <message.h>
#include "Console.h"

// #include <unistd.h>

#ifdef ALIGNMENT
#ifdef __TURBOC__
#include <mem.h>
#else
#include <memory.h>
#endif
#endif

#ifdef FILEIO
#include <unistd.h>
#endif

extern Console *sysConsole;

#ifdef Macintosh
/* Macintoshes need 32K segments, else barfage ensues */
#pragma segment seg2a
#endif /*Macintosh*/

/*  Custom configuration.  If the tag CUSTOM has been defined (usually on
    the compiler call line), we include the file "atlcfig.h", which may
    then define INDIVIDUALLY and select the subpackages needed for its
    application.  */

#ifdef CUSTOM
#include "atlcfig.h"
#endif

/*  Subpackage configuration.  If INDIVIDUALLY is defined, the inclusion
    of subpackages is based on whether their compile-time tags are
    defined.  Otherwise, we automatically enable all the subpackages.  */

#ifndef INDIVIDUALLY
#define ARRAY			      /* Array subscripting words */
#define BREAK			      /* Asynchronous break facility */
#define COMPILERW		      /* Compiler-writing words */
#define CONIO			      /* Interactive console I/O */
#define DEFFIELDS		      /* Definition field access for words */
#define DOUBLE			      /* Double word primitives (2DUP) */
#define EVALUATE		      /* The EVALUATE primitive */
#define FILEIO			      /* File I/O primitives */
#define MATH			      /* Math functions */
#define MEMMESSAGE		      /* Print message for stack/heap errors */
#define PROLOGUE		      /* Prologue processing and auto-init */
#define REAL			      /* Floating point numbers */
#define SHORTCUTA		      /* Shortcut integer arithmetic words */
#define SHORTCUTC		      /* Shortcut integer comparison */
#define STRING			      /* String functions */
#define SYSTEM			      /* System command function */
#ifndef NOMEMCHECK
#define TRACE			      /* Execution tracing */
#define WALKBACK		      /* Walkback trace */
#define WORDSUSED		      /* Logging of words used and unused */
#endif /* NOMEMCHECK */
#endif /* !INDIVIDUALLY */


#include "atldef.h"

#ifdef MATH
#include <math.h>
#endif

/* LINTLIBRARY */

/* Implicit functions (work for all numeric types). */

#ifdef abs
#undef abs
#endif
#define abs(x)	 ((x) < 0    ? -(x) : (x))
#define max(a,b) ((a) >  (b) ?	(a) : (b))
#define min(a,b) ((a) <= (b) ?	(a) : (b))

/*  Globals imported  */

/*  Data types	*/

typedef enum {False = 0, True = 1} Boolean;

#define EOS     '\0'                  /* End of string characters */

#define V	(void)		      /* Force result to void */

#define Truth	-1L		      /* Stack value for truth */
#define Falsity 0L		      /* Stack value for falsity */

/* Utility definition to get an  array's  element  count  (at  compile
   time).   For  example:

   int  arr[] = {1,2,3,4,5};
   ...
   printf("%d", ELEMENTS(arr));

   would print a five.  ELEMENTS("abc") can also be used to  tell  how
   many  bytes are in a string constant INCLUDING THE TRAILING NULL. */

#define ELEMENTS(array) (sizeof(array)/sizeof((array)[0]))

/*  Globals visible to calling programs  */

#ifdef LINUX
#include <termios.h>
static int      ttyfd = 0;   /* STDIN_FILENO is 0 by default */
struct termios orig_termios; /* Terminal IO Structure */

void fatal(char *message) {
    fprintf(stderr, "fatal error: %s\n", message);
    exit(1);
}
#endif

atl_int atl_stklen = 100;	      /* Evaluation stack length */
atl_int atl_rstklen = 100;	      /* Return stack length */
atl_int atl_heaplen = 1000;	      /* Heap length */
atl_int atl_ltempstr = 256;	      /* Temporary string buffer length */
atl_int atl_ntempstr = 4;	      /* Number of temporary string buffers */

atl_int atl_trace = Falsity;	      /* Tracing if true */
atl_int atl_walkback = Truth;	      /* Walkback enabled if true */
atl_int atl_comment = Falsity;	      /* Currently ignoring a comment */
atl_int atl_redef = Truth;	      /* Allow redefinition without issuing
                                     the "not unique" message. */
atl_int atl_errline = 0;	      /* Line where last atl_load failed */

//ATH
atl_int ath_safe_memory = Truth;
/*  Local variables  */

/* The evaluation stack */

Exported stackitem *stack = NULL;     /* Evaluation stack */
Exported stackitem *stk;	      /* Stack pointer */
Exported stackitem *stackbot;	      /* Stack bottom */
Exported stackitem *stacktop;	      /* Stack top */

/* The return stack */

Exported dictword ***rstack = NULL;   /* Return stack */
Exported dictword ***rstk;	      /* Return stack pointer */
Exported dictword ***rstackbot;       /* Return stack bottom */
Exported dictword ***rstacktop;       /* Return stack top */

/* The heap */

Exported stackitem *heap = NULL;      /* Allocation heap */
Exported stackitem *hptr;	      /* Heap allocation pointer */
Exported stackitem *heapbot;	      /* Bottom of heap (temp string buffer) */
Exported stackitem *heaptop;	      /* Top of heap */

/* The dictionary */

Exported dictword *dict = NULL;       /* Dictionary chain head */
Exported dictword *dictprot = NULL;   /* First protected item in dictionary */

/* The temporary string buffers */

Exported char **strbuf = NULL;	      /* Table of pointers to temp strings */
Exported int cstrbuf = 0;	      /* Current temp string */

/* The walkback trace stack */

#ifdef WALKBACK
static dictword **wback = NULL;       /* Walkback trace buffer */
static dictword **wbptr;	      /* Walkback trace pointer */
#endif /* WALKBACK */

#ifdef MEMSTAT
Exported stackitem *stackmax;	      /* Stack maximum excursion */
Exported dictword ***rstackmax;       /* Return stack maximum excursion */
Exported stackitem *heapmax;	      /* Heap maximum excursion */
#endif

#ifdef FILEIO
static char *fopenmodes[] = {
#ifdef FBmode
#define FMspecial
    /* Fopen() mode table for systems that require a "b" in the
       mode string for binary files. */
    "", "r",  "",   "r+",
    "", "rb", "",   "r+b",
    "", "",   "w",  "w+",
    "", "",   "wb", "w+b"
#endif
#ifndef FMspecial
        /* Default fopen() mode table for SVID-compatible systems not
           overridden by a special table above. */
        (char *)"", (char *)"r", (char *) "", (char *)  "r+",
    (char *)"", (char *)"r", (char *) "", (char *)  "r+",
    (char *)"", (char *)"", (char *)  "w", (char *) "w+",
    (char *)"", (char *)"", (char *)  "w", (char *) "w+"
#endif
};

#endif /* FILEIO */

static char tokbuf[128];	      /* Token buffer */
static char *instream = NULL;	      /* Current input stream line */
static long tokint;		      /* Scanned integer */
#ifdef REAL
static atl_real tokreal;	      /* Scanned real number */
#ifdef ALIGNMENT
Exported atl_real rbuf0, rbuf1, rbuf2; /* Real temporary buffers */
#endif
#endif
static long base = 10;		      /* Number base */
Exported dictword **ip = NULL;	      /* Instruction pointer */
Exported dictword *curword = NULL;    /* Current word being executed */
static int evalstat = ATL_SNORM;      /* Evaluator status */
static Boolean defpend = False;       /* Token definition pending */
static Boolean forgetpend = False;    /* Forget pending */
static Boolean tickpend = False;      /* Take address of next word */
static Boolean ctickpend = False;     /* Compile-time tick ['] pending */
static Boolean cbrackpend = False;    /* [COMPILE] pending */
Exported dictword *createword = NULL; /* Address of word pending creation */
static Boolean stringlit = False;     /* String literal anticipated */
#ifdef BREAK
static Boolean broken = False;	      /* Asynchronous break received */
#endif

#ifdef COPYRIGHT
#ifndef HIGHC
#ifndef lint
static
#endif
#endif
char copyright[] = "ATLAST: This program is in the public domain.";
#endif

/* The following static cells save the compile addresses of words
   generated by the compiler.  They are looked up immediately after
   the dictionary is created.  This makes the compiler much faster
   since it doesn't have to look up internally-reference words, and,
   far more importantly, keeps it from being spoofed if a user redefines
   one of the words generated by the compiler.	*/

static stackitem s_exit, s_lit, s_strlit, s_dotparen,
                 s_qbranch, s_branch, s_xdo, s_xqdo, s_xloop,
                 s_pxloop, s_abortq;
#ifdef REAL
static stackitem s_flit;
#endif
/*  Forward functions  */

STATIC void exword(dictword *), trouble(char *);
#ifndef NOMEMCHECK
STATIC void notcomp(), divzero();
#endif
#ifdef WALKBACK
STATIC void pwalkback();
#endif

bool initIO() {
    return true;
}

#ifdef ATH
void ATH_Features() {
#ifdef ARRAY
    printf("    ARRAY\n");
#else
    printf("NOT ARRAY\n");
#endif

#ifdef BREAK
    printf("    BREAK\n");
#else
    printf("NOT BREAK\n");
#endif

#ifdef COMPILERW
    printf("    COMPILERW\n");
#else
    printf("NOT COMPILERW\n");
#endif

#ifdef CONIO
    printf("    CONIO\n");
#else
    printf("NOT CONIO\n");
#endif

#ifdef DEFFIELDS
    printf("    DEFFIELDS\n");
#else
    printf("NOT DEFFIELDS\n");
#endif

#ifdef DOUBLE
    printf("    DOUBLE\n");
#else
    printf("NOT DOUBLE\n");
#endif

#ifdef EVALUATE
    printf("    EVALUATE\n");
#else
    printf("NOT EVALUATE\n");
#endif

#ifdef FILEIO
    printf("    FILEIO\n");
#else
    printf("NOT FILEIO\n");
#endif

#ifdef MATH
    printf("    MATH\n");
#else
    printf("NOT MATH\n");
#endif

#ifdef MEMMESSAGE
    printf("    MEMMESSAGE\n");
#else
    printf("NOT MEMMESSAGE\n");
#endif

#ifdef PROLOGUE
    printf("    PROLOGUE\n");
#else
    printf("NOT PROLOGUE\n");
#endif

#ifdef REAL
    printf("    REAL\n");
#else
    printf("NOT REAL\n");
#endif

#ifdef SHORTCUTA
    printf("    SHORTCUTA\n");
#else
    printf("NOT SHORTCUTA\n");
#endif

#ifdef SHORTCUTC
    printf("    SHORTCUTC\n");
#else
    printf("NOT SHORTCUTC\n");
#endif

#ifdef STRING
    printf("    STRING\n");
#else
    printf("NOT STRING\n");
#endif

#ifdef SYSTEM
    printf("    SYSTEM\n");
#else
    printf("NOT SYSTEM\n");
#endif

#ifdef TRACE
    printf("    TRACE\n");
#else
    printf("NOT TRACE\n");
#endif

#ifdef WALKBACK
    printf("    WALKBACK\n");
#else
    printf("NOT WALKBACK\n");
#endif

#ifdef WORDSUSED
    printf("    WORDSUSED\n");
#else
    printf("NOT WORDSUSED\n");
#endif

#ifdef ATH
    printf("    ATH CUSTOM\n");
#else
    printf("NOT ATH CUSTOM\n");
#endif
}
// If ath_safe_memory is non zero then ensure memeory access
// is within the heap.
//
/* @brief MAkes memeory access 'safe'
 *
 * Normally maemory access is restricted to a pool.  Access words validate that the addres is one within the bounds of the pool and return an exception.  This, however, restricts access to hardware such as registers.  memsafe turns thi on and off.  By default access is safe.
 *
 * @param flag true | false
 * @return None.
 *
 */
prim ATH_memsafe() {
    Sl(1);
    ath_safe_memory = (S0 == 0) ? Falsity : Truth;
    Pop;
}

prim ATH_qmemsafe() {
    So(1);
    Push = ath_safe_memory;
}

prim ATH_realq() {
    So(1);
#ifdef REAL
    Push=-1;
#else
    Push=0;
#endif
}

prim ATH_mathq() {
    So(1);
#ifdef MATH
    Push=-1;
#else
    Push=0;
#endif
}

prim ATH_fileioq() {
    So(1);
#ifdef FILEIO
    Push=-1;
#else
    Push=0;
#endif
}

// Time to leave.
prim ATH_bye() {
    exit(0);
}

void displayLineHex(uint8_t *a) {
    int i;

    for(i=0;i<16;i++) {
        printf(" %02x",*(a++));
    }
}

void displayLineAscii(uint8_t *a) {
    int i;

    printf(":");

    for(i=0;i<16;i++) {
        if( (*a < 0x20 ) || (*a > 128 )) {
            printf(".");
        } else {
            printf("%c",*(a++));
        }
    }
    printf("\n");
}

static int token( char **);

prim ATH_dump() {
    Sl(2); // address len

    int length = S0;
    uint8_t *address = (uint8_t *) S1;
    int lines=length/16;

    if(lines ==0 ) {
        lines=1;
    }

    for(int i = 0; i<length;i+=16) {
        printf("%08x:", address);
        displayLineHex( address );
        displayLineAscii( address );
        address +=16;
    }

    Pop2;
}

prim ATH_erase() {
    Sl(2);

    int length = S0;
    uint8_t *address = (uint8_t *) S1;

    memset(address,0,length);

    Pop2;
}

prim ATH_fill() {
    Sl(3);

    uint8_t d = S0;
    int length = S1;
    uint8_t *address = (uint8_t *) S2 ;

    memset(address,d,length);

    Pop2;
    Pop;
}

/*
#include "message.h"
#include "Boiler.h"
*/

prim ATH_test() {
    /*
    int i;
    int len;

    printf("Test\n");

    i=token(&instream);

    printf("%d=%s\n",i,tokbuf);
    printf("here=%08x\n", hptr);

    len=strlen( tokbuf );
    strncpy( (char *)hptr, tokbuf, len);

    Push = (stackitem)tokbuf;
    Push = (stackitem)len;
    */
    /*
    Message *tst;
    tst = new Boiler();
    Push=(stackitem) tst;
    */
    Push = (stackitem )sysConsole;

}

prim ATH_objdump() {

    ((Console *)S0)->dump();
    Pop;

}


#endif // ATH

/*  ALLOC  --  Allocate memory and error upon exhaustion.  */

static char *alloc(unsigned int size) {
    char buffer[132];
    char *cp = (char *)malloc(size);

    /* printf("\nAlloc %u", size); */
    if (cp == NULL) {

#ifdef EMBEDDED
        sprintf(buffer, "\n\nOut of memory!  %u bytes requested.\n", size); // EMBEDDED
        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
        V fprintf(stderr, "\n\nOut of memory!  %u bytes requested.\n", size); // NOT EMBEDDED
#endif

        abort();
    }
    return cp;
}

/*  UCASE  --  Force letters in string to upper case.  */

static void ucase(char *c) {
    char ch;

    while ((ch = *c) != EOS) {
        if (islower(ch))
            *c = toupper(ch);
        c++;
    }
}

/*  TOKEN  --  Scan a token and return its type.  */

static int token( char **cp) {
    char *sp = *cp;

    while (True) {
        char *tp = tokbuf;
        unsigned int tl = 0;
        Boolean istring = False, rstring = False;

        if (atl_comment) {
            while (*sp != ')') {
                if (*sp == EOS) {
                    *cp = sp;
                    return TokNull;
                }
                sp++;
            }
            sp++;
            atl_comment = Falsity;
        }

        while (isspace(*sp))		  /* Skip leading blanks */
            sp++;

        if (*sp == '"') {                 /* Is this a string ? */

            /* Assemble string token. */

            sp++;
            while (True) {
                char c = *sp++;

                if (c == '"') {
                    sp++;
                    *tp++ = EOS;
                    break;
                } else if (c == EOS) {
                    rstring = True;
                    *tp++ = EOS;
                    break;
                }
                if (c == '\\') {
                    c = *sp++;
                    if (c == EOS) {
                        rstring = True;
                        break;
                    }
                    switch (c) {
                        case 'b':
                            c = '\b';
                            break;
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        default:
                            break;
                    }
                }
                if (tl < (sizeof tokbuf) - 1) {
                    *tp++ = c;
                    tl++;
                } else {
                    rstring = True;
                }
            }
            istring = True;
        } else {

            /* Scan the next raw token */

            while (True) {
                char c = *sp++;

                if (c == EOS || isspace(c)) {
                    *tp++ = EOS;
                    break;
                }
                if (tl < (sizeof tokbuf) - 1) {
                    *tp++ = c;
                    tl++;
                }
            }
        }
        *cp = --sp;			  /* Store end of scan pointer */

        if (istring) {
            if (rstring) {
#ifdef MEMMESSAGE
#ifdef EMBEDDED
                char buffer[132];
                sprintf(buffer,"\nRunaway string: %s\n", tokbuf); // EMBEDDED
                sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                V printf("\nRunaway string: %s\n", tokbuf); // NOT EMBEDDED
#endif
#endif
                evalstat = ATL_RUNSTRING;
                return TokNull;
            }
            return TokString;
        }

        if (tokbuf[0] == EOS)
            return TokNull;

        /* See if token is a comment to end of line character.	If so, discard
           the rest of the line and return null for this token request. */

        if (strcmp(tokbuf, "\\") == 0) {
            while (*sp != EOS)
                sp++;
            *cp = sp;
            return TokNull;
        }

        /* See if this token is a comment open delimiter.  If so, set to
           ignore all characters until the matching comment close delimiter. */

        if (strcmp(tokbuf, "(") == 0) {
            while (*sp != EOS) {
                if (*sp == ')')
                    break;
                sp++;
            }
            if (*sp == ')') {
                sp++;
                continue;
            }
            atl_comment = Truth;
            *cp = sp;
            return TokNull;
        }

        /* See if the token is a number. */

        if (isdigit(tokbuf[0]) || tokbuf[0] == '-') {
            //            char tc;
            char *tcp;

#ifdef OS2
            /* Compensate for error in OS/2 sscanf() library function */
            if ((tokbuf[0] == '-') &&
                    !(isdigit(tokbuf[1]) ||
                        (((tokbuf[1] == 'x') || (tokbuf[1] == 'X')) &&
                         isxdigit(tokbuf[2])))) {
                return TokWord;
            }
#endif /* OS2 */
#ifdef USE_SSCANF
            if (sscanf(tokbuf, "%li%c", &tokint, &tc) == 1)
                return TokInt;
#else
            tokint = strtoul(tokbuf, &tcp, 0);
            if (*tcp == 0) {
                return TokInt;
            }
#endif
#ifdef REAL
            if (sscanf(tokbuf, "%lf%c", &tokreal, &tc) == 1)
                return TokReal;
#endif
        }
        return TokWord;
    }
}

/*  LOOKUP  --	Look up token in the dictionary.  */

static dictword *lookup(char *tkname) {
    dictword *dw = dict;

    ucase(tkname);		      /* Force name to upper case */
    while (dw != NULL) {
        if (!(dw->wname[0] & WORDHIDDEN) &&
                (strcmp(dw->wname + 1, tkname) == 0)) {
#ifdef WORDSUSED
            *(dw->wname) |= WORDUSED; /* Mark this word used */
#endif
            break;
        }
        dw = dw->wnext;
    }
    return dw;
}

/* Gag me with a spoon!  Does no compiler but Turbo support
#if defined(x) || defined(y) ?? */
#ifdef EXPORT
#define FgetspNeeded
#endif
#ifdef FILEIO
#ifndef FgetspNeeded
#define FgetspNeeded
#endif
#endif

#ifdef FgetspNeeded

/*  ATL_FGETSP	--  Portable database version of FGETS.  This reads the
    next line into a buffer a la fgets().  A line is
    delimited by either a carriage return or a line
    feed, optionally followed by the other character
    of the pair.  The string is always null
    terminated, and limited to the length specified - 1
    (excess characters on the line are discarded.
    The string is returned, or NULL if end of file is
    encountered and no characters were stored.	No end
    of line character is stored in the string buffer.
    */

// Exported char *atl_fgetsp(s, n, stream)
Exported char *atl_fgetsp( char *s, int n, FILE *stream) {
    int i = 0, ch;

    while (True) {
        ch = getc(stream);
        if (ch == EOF) {
            if (i == 0)
                return NULL;
            break;
        }
        if (ch == '\r') {
            ch = getc(stream);
            if (ch != '\n')
                V ungetc(ch, stream);
            break;
        }
        if (ch == '\n') {
            ch = getc(stream);
            if (ch != '\r')
                V ungetc(ch, stream);
            break;
        }
        if (i < (n - 1))
            s[i++] = ch;
    }
    s[i] = EOS;
    return s;
}
#endif /* FgetspNeeded */

/*  ATL_MEMSTAT  --  Print memory usage summary.  */

#ifdef MEMSTAT
// TODO Comments reduce size to below 255 bytes, now make it print into
// outBuffer
void atl_memstat() {
    static char fmt[] = "   %-12s %6ld    %6ld    %6ld       %3ld\n";

    //    V printf("\n             Memory Usage Summary\n\n");
    //    V printf("                 Current   Maximum    Items     Percent\n");
    V printf("  Memory Area     usage     used    allocated   in use \n");

    V printf(fmt, "Stack",
            ((long) (stackmax - stack)),
            atl_stklen,
            (100L * (stk - stack)) / atl_stklen);
    V printf(fmt, "Return stack",
            ((long) (rstk - rstack)),
            ((long) (rstackmax - rstack)),
            atl_rstklen,
            (100L * (rstk - rstack)) / atl_rstklen);
    V printf(fmt, "Heap",
            ((long) (hptr - heap)),
            ((long) (heapmax - heap)),
            atl_heaplen,
            (100L * (hptr - heap)) / atl_heaplen);
}
#endif /* MEMSTAT */

/*  Primitive implementing functions.  */

/*  ENTER  --  Enter word in dictionary.  Given token for word's
    name and initial values for its attributes, returns
    the newly-allocated dictionary item. */

static void enter( char *tkname) {
    /* Allocate name buffer */
    createword->wname = alloc(((unsigned int) strlen(tkname) + 2));
    createword->wname[0] = 0;	      /* Clear flags */
    V strcpy(createword->wname + 1, tkname); /* Copy token to name buffer */
    createword->wnext = dict;	      /* Chain rest of dictionary to word */
    dict = createword;		      /* Put word at head of dictionary */
}

#ifdef Keyhit

/*  KBQUIT  --	If this system allows detecting key presses, handle
    the pause, resume, and quit protocol for the word
    listing facilities.  */

// TODO, not sure what to do here
static Boolean kbquit()
{
    int key;

    if ((key = Keyhit()) != 0) {
        V printf("\nPress RETURN to stop, any other key to continue: ");
        while ((key = Keyhit()) == 0) ;
        if (key == '\r' || (key == '\n'))
            return True;
    }
    return False;
}
#endif /* Keyhit */

/*  Primitive word definitions.  */

#ifdef NOMEMCHECK
#define Compiling
#else
#define Compiling if (state == Falsity) {notcomp(); return;}
#endif
#define Compconst(x) Ho(1); Hstore = (stackitem) (x)
#define Skipstring ip += *((char *) ip)

prim P_plus()			      /* Add two numbers */
{
    Sl(2);
    /* printf("PLUS %lx + %lx = %lx\n", S1, S0, (S1 + S0)); */
    S1 += S0;
    Pop;
}

prim P_minus()			      /* Subtract two numbers */
{
    Sl(2);
    S1 -= S0;
    Pop;
}

prim P_times()			      /* Multiply two numbers */
{
    Sl(2);
    S1 *= S0;
    Pop;
}

prim P_div()			      /* Divide two numbers */
{
    Sl(2);
#ifndef NOMEMCHECK
    if (S0 == 0) {
        divzero();
        return;
    }
#endif /* NOMEMCHECK */
    S1 /= S0;
    Pop;
}

prim P_mod()			      /* Take remainder */
{
    Sl(2);
#ifndef NOMEMCHECK
    if (S0 == 0) {
        divzero();
        return;
    }
#endif /* NOMEMCHECK */
    S1 %= S0;
    Pop;
}

prim P_divmod() 		      /* Compute quotient and remainder */
{
    stackitem quot;

    Sl(2);
#ifndef NOMEMCHECK
    if (S0 == 0) {
        divzero();
        return;
    }
#endif /* NOMEMCHECK */
    quot = S1 / S0;
    S1 %= S0;
    S0 = quot;
}

prim P_min()			      /* Take minimum of stack top */
{
    Sl(2);
    S1 = min(S1, S0);
    Pop;
}

prim P_max()			      /* Take maximum of stack top */
{
    Sl(2);
    S1 = max(S1, S0);
    Pop;
}

prim P_neg()			      /* Negate top of stack */
{
    Sl(1);
    S0 = - S0;
}

prim P_abs()			      /* Take absolute value of top of stack */
{
    Sl(1);
    S0 = abs(S0);
}

prim P_equal()			      /* Test equality */
{
    Sl(2);
    S1 = (S1 == S0) ? Truth : Falsity;
    Pop;
}

prim P_unequal()		      /* Test inequality */
{
    Sl(2);
    S1 = (S1 != S0) ? Truth : Falsity;
    Pop;
}

prim P_gtr()			      /* Test greater than */
{
    Sl(2);
    S1 = (S1 > S0) ? Truth : Falsity;
    Pop;
}

prim P_lss()			      /* Test less than */
{
    Sl(2);
    S1 = (S1 < S0) ? Truth : Falsity;
    Pop;
}

prim P_geq()			      /* Test greater than or equal */
{
    Sl(2);
    S1 = (S1 >= S0) ? Truth : Falsity;
    Pop;
}

prim P_leq()			      /* Test less than or equal */
{
    Sl(2);
    S1 = (S1 <= S0) ? Truth : Falsity;
    Pop;
}

prim P_and()			      /* Logical and */
{
    Sl(2);
    /* printf("AND %lx & %lx = %lx\n", S1, S0, (S1 & S0)); */
    S1 &= S0;
    Pop;
}

prim P_or()			      /* Logical or */
{
    Sl(2);
    S1 |= S0;
    Pop;
}

prim P_xor()			      /* Logical xor */
{
    Sl(2);
    S1 ^= S0;
    Pop;
}

prim P_not()			      /* Logical negation */
{
    Sl(1);
    S0 = ~S0;
}

prim P_shift()			      /* Shift:  value nbits -- value */
{
    Sl(1);
    S1 = (S0 < 0) ? (((unsigned long) S1) >> (-S0)) :
        (((unsigned long) S1) <<   S0);
    Pop;
}

#ifdef SHORTCUTA

prim P_1plus()			      /* Add one */
{
    Sl(1);
    S0++;
}

prim P_2plus()			      /* Add two */
{
    Sl(1);
    S0 += 2;
}

prim P_1minus() 		      /* Subtract one */
{
    Sl(1);
    S0--;
}

prim P_2minus() 		      /* Subtract two */
{
    Sl(1);
    S0 -= 2;
}

prim P_2times() 		      /* Multiply by two */
{
    Sl(1);
    S0 *= 2;
}

prim P_2div()			      /* Divide by two */
{
    Sl(1);
    S0 /= 2;
}

#endif /* SHORTCUTA */

#ifdef SHORTCUTC

prim P_0equal() 		      /* Equal to zero ? */
{
    Sl(1);
    S0 = (S0 == 0) ? Truth : Falsity;
}

prim P_0notequal()		      /* Not equal to zero ? */
{
    Sl(1);
    S0 = (S0 != 0) ? Truth : Falsity;
}

prim P_0gtr()			      /* Greater than zero ? */
{
    Sl(1);
    S0 = (S0 > 0) ? Truth : Falsity;
}

prim P_0lss()			      /* Less than zero ? */
{
    Sl(1);
    S0 = (S0 < 0) ? Truth : Falsity;
}

#endif /* SHORTCUTC */

/*  Storage allocation (heap) primitives  */

prim P_here()			      /* Push current heap address */
{
    So(1);
    Push = (stackitem) hptr;
}

// TODO To allow acces to memeory outside of heap, such as
// Shared memeory or IO device add a value to test around the
// Hpc call, to switch off memory bounds checking.
//
prim P_bang()			      /* Store value into address */
{
    Sl(2);
    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }
    *((stackitem *) S0) = S1;
    Pop2;
}

/* Fetch value from address */
prim P_at() {
    Sl(1);

    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }
    S0 = *((stackitem *) S0);
}

prim P_plusbang()		      /* Add value at specified address */
{
    Sl(2);
    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }
    *((stackitem *) S0) += S1;
    Pop2;
}

prim P_allot()			      /* Allocate heap bytes */
{
    stackitem n;

    Sl(1);
    n = (S0 + (sizeof(stackitem) - 1)) / sizeof(stackitem);
    Pop;
    Ho(n);
    hptr += n;
}

prim P_comma()			      /* Store one item on heap */
{
    Sl(1);
    Ho(1);
    Hstore = S0;
    Pop;
}

prim P_cbang()			      /* Store byte value into address */
{
    Sl(2);
    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }
    *((unsigned char *) S0) = S1;
    Pop2;
}

prim P_cat()			      /* Fetch byte value from address */
{
    Sl(1);
    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }
    S0 = *((unsigned char *) S0);
}

#ifdef ATH
prim ATH_wbang() {
    Sl(2);

    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }

    uint16_t *ptr=(uint16_t *)S0;
    uint16_t data=S1;

    *ptr=(uint16_t)(data & 0xffff);

    Pop2;
}

/* Fetch byte value from address */
prim ATH_wat() {

    Sl(1);
    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }
    S0 = *((uint16_t *) S0);
}
#endif

prim P_ccomma() 		      /* Store one byte on heap */
{
    unsigned char *chp;

    Sl(1);
    Ho(1);
    chp = ((unsigned char *) hptr);
    *chp++ = S0;
    hptr = (stackitem *) chp;
    Pop;
}

prim P_cequal() 		      /* Align heap pointer after storing */
{				      /* a series of bytes. */
    stackitem n = (((stackitem) hptr) - ((stackitem) heap)) %
        (sizeof(stackitem));

    if (n != 0) {
        char *chp = ((char *) hptr);

        chp += sizeof(stackitem) - n;
        hptr = ((stackitem *) chp);
    }
}

/*  Variable and constant primitives  */

prim P_var()			      /* Push body address of current word */
{
    So(1);
    Push = (stackitem) (((stackitem *) curword) + Dictwordl);
}

Exported void P_create()	      /* Create new word */
{
    defpend = True;		      /* Set definition pending */
    Ho(Dictwordl);
    createword = (dictword *) hptr;   /* Develop address of word */
    createword->wname = NULL;	      /* Clear pointer to name string */
    createword->wcode = P_var;	      /* Store default code */
    hptr += Dictwordl;		      /* Allocate heap space for word */
}

prim P_forget() 		      /* Forget word */
{
    forgetpend = True;		      /* Mark forget pending */
}

prim P_variable()		      /* Declare variable */
{
    P_create(); 		      /* Create dictionary item */
    Ho(1);
    Hstore = 0; 		      /* Initial value = 0 */
}

prim P_con()			      /* Push value in body */
{
    So(1);
    Push = *(((stackitem *) curword) + Dictwordl);
}

prim P_constant()		      /* Declare constant */
{
    Sl(1);
    P_create(); 		      /* Create dictionary item */
    createword->wcode = P_con;	      /* Set code to constant push */
    Ho(1);
    Hstore = S0;		      /* Store constant value in body */
    Pop;
}

/*  Array primitives  */

#ifdef ARRAY
prim P_arraysub()		      /* Array subscript calculation */
{				      /* sub1 sub2 ... subn -- addr */
    int i, offset, esize, nsubs;
    stackitem *array;
    stackitem *isp;

    Sl(1);
    array = (((stackitem *) curword) + Dictwordl);
    Hpc(array);
    nsubs = *array++;		      /* Load number of subscripts */
    esize = *array++;		      /* Load element size */
#ifndef NOMEMCHECK
    isp = &S0;
    for (i = 0; i < nsubs; i++) {
        stackitem subn = *isp--;

        if (subn < 0 || subn >= array[i])
            trouble((char *)"Subscript out of range");
    }
#endif /* NOMEMCHECK */
    isp = &S0;
    offset = *isp;		      /* Load initial offset */
    for (i = 1; i < nsubs; i++)
        offset = (offset * (*(++array))) + *(--isp);
    Npop(nsubs - 1);
    /* Calculate subscripted address.  We start at the current word,
       advance to the body, skip two more words for the subscript count
       and the fundamental element size, then skip the subscript bounds
       words (as many as there are subscripts).  Then, finally, we
       can add the calculated offset into the array. */
    S0 = (stackitem) (((char *) (((stackitem *) curword) +
                    Dictwordl + 2 + nsubs)) + (esize * offset));
}

prim P_array()			      /* Declare array */
{				      /* sub1 sub2 ... subn n esize -- array */
    int i, nsubs, asize = 1;
    stackitem *isp;

    Sl(2);
#ifndef NOMEMCHECK
    if (S0 <= 0)
        trouble((char *)"Bad array element size");
    if (S1 <= 0)
        trouble((char *)"Bad array subscript count");
#endif /* NOMEMCHECK */

    nsubs = S1; 		      /* Number of subscripts */
    Sl(nsubs + 2);		      /* Verify that dimensions are present */

    /* Calculate size of array as the product of the subscripts */

    asize = S0; 		      /* Fundamental element size */
    isp = &S2;
    for (i = 0; i < nsubs; i++) {
#ifndef NOMEMCHECK
        if (*isp <= 0)
            trouble((char *)"Bad array dimension");
#endif /* NOMEMCHECK */
        asize *= *isp--;
    }

    asize = (asize + (sizeof(stackitem) - 1)) / sizeof(stackitem);
    Ho(asize + nsubs + 2);	      /* Reserve space for array and header */
    P_create(); 		      /* Create variable */
    createword->wcode = P_arraysub;   /* Set method to subscript calculate */
    Hstore = nsubs;		      /* Header <- Number of subscripts */
    Hstore = S0;		      /* Header <- Fundamental element size */
    isp = &S2;
    for (i = 0; i < nsubs; i++) {     /* Header <- Store subscripts */
        Hstore = *isp--;
    }
    while (asize-- > 0) 	      /* Clear the array to zero */
        Hstore = 0;
    Npop(nsubs + 2);
}
#endif /* ARRAY */

/*  String primitives  */

#ifdef STRING

/* Push address of string literal */
prim P_strlit() {
    char buffer[132];
    So(1);
    Push = (stackitem) (((char *) ip) + 1);
#ifdef TRACE
    if (atl_trace) {
#ifdef EMBEDDED
        sprintf(buffer,"\"%s\" ", (((char *) ip) + 1)); // EMBEDDED
        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
        V printf("\"%s\" ", (((char *) ip) + 1)); // NOT EMBEDDED
#endif
    }
#endif /* TRACE */
    Skipstring; 		      /* Advance IP past it */
}

prim P_string() 		      /* Create string buffer */
{
    Sl(1);
    Ho((S0 + 1 + sizeof(stackitem)) / sizeof(stackitem));
    P_create(); 		      /* Create variable */
    /* Allocate storage for string */
    hptr += (S0 + 1 + sizeof(stackitem)) / sizeof(stackitem);
    Pop;
}

prim P_strcpy() 		      /* Copy string to address on stack */
{
    Sl(2);
    Hpc(S0);
    Hpc(S1);
    V strcpy((char *) S0, (char *) S1);
    Pop2;
}

prim P_strcat() 		      /* Append string to address on stack */
{
    Sl(2);
    Hpc(S0);
    Hpc(S1);
    V strcat((char *) S0, (char *) S1);
    Pop2;
}

prim P_strlen() 		      /* Take length of string on stack top */
{
    Sl(1);
    Hpc(S0);
    S0 = strlen((char *) S0);
}

prim P_strcmp() 		      /* Compare top two strings on stack */
{
    int i;

    Sl(2);
    Hpc(S0);
    Hpc(S1);
    i = strcmp((char *) S1, (char *) S0);
    S1 = (i == 0) ? 0L : ((i > 0) ? 1L : -1L);
    Pop;
}

prim P_strchar()		      /* Find character in string */
{
    Sl(2);
    Hpc(S0);
    Hpc(S1);
    S1 = (stackitem) strchr((char *) S1, *((char *) S0));
    Pop;
}

prim P_substr() 		      /* Extract and store substring */
{				      /* source start length/-1 dest -- */
    long sl, sn;
    char *ss, *sp, *se, *ds;

    Sl(4);
    Hpc(S0);
    Hpc(S3);
    sl = strlen(ss = ((char *) S3));
    se = ss + sl;
    sp = ((char *) S3) + S2;
    if ((sn = S1) < 0)
        sn = 999999L;
    ds = (char *) S0;
    while (sn-- && (sp < se))
        *ds++ = *sp++;
    *ds++ = EOS;
    Npop(4);
}

prim P_strform()		      /* Format integer using sprintf() */
{                                     /* value "%ld" str -- */
    Sl(2);
    Hpc(S0);
    Hpc(S1);

    V sprintf((char *) S0, (char *) S1, S2);  // NOT EMBEDDED

    Npop(3);
}

#ifdef REAL
prim P_fstrform()		      /* Format real using sprintf() */
{                                     /* rvalue "%6.2f" str -- */
    Sl(4);
    Hpc(S0);
    Hpc(S1);
    V sprintf((char *) S0, (char *) S1, REAL1);
    Npop(4);
}
#endif /* REAL */

prim P_strint() 		      /* String to integer */
{				      /* str -- endptr value */
    stackitem is;
    char *eptr;

    Sl(1);
    So(1);
    Hpc(S0);
    is = strtoul((char *) S0, &eptr, 0);
    S0 = (stackitem) eptr;
    Push = is;
}

#ifdef REAL
prim P_strreal()		      /* String to real */
{				      /* str -- endptr value */
    int i;
    union {
        atl_real fs;
        stackitem fss[Realsize];
    } fsu;
    char *eptr;

    Sl(1);
    So(2);
    Hpc(S0);
    fsu.fs = strtod((char *) S0, &eptr);
    S0 = (stackitem) eptr;
    for (i = 0; i < Realsize; i++) {
        Push = fsu.fss[i];
    }
}
#endif /* REAL */
#endif /* STRING */

/*  Floating point primitives  */

#ifdef REAL

prim P_flit()			      /* Push floating point literal */
{
    int i;

    So(Realsize);
#ifdef TRACE
    if (atl_trace) {
        atl_real tr;

        V memcpy((char *) &tr, (char *) ip, sizeof(atl_real));
        V printf("%g ", tr);
    }
#endif /* TRACE */
    for (i = 0; i < Realsize; i++) {
        Push = (stackitem) *ip++;
    }
}

prim P_fplus()			      /* Add floating point numbers */
{
    Sl(2 * Realsize);
    SREAL1(REAL1 + REAL0);
    Realpop;
}

prim P_fminus() 		      /* Subtract floating point numbers */
{
    Sl(2 * Realsize);
    SREAL1(REAL1 - REAL0);
    Realpop;
}

prim P_ftimes() 		      /* Multiply floating point numbers */
{
    Sl(2 * Realsize);
    SREAL1(REAL1 * REAL0);
    Realpop;
}

prim P_fdiv()			      /* Divide floating point numbers */
{
    Sl(2 * Realsize);
#ifndef NOMEMCHECK
    if (REAL0 == 0.0) {
        divzero();
        return;
    }
#endif /* NOMEMCHECK */
    SREAL1(REAL1 / REAL0);
    Realpop;
}

prim P_fmin()			      /* Minimum of top two floats */
{
    Sl(2 * Realsize);
    SREAL1(min(REAL1, REAL0));
    Realpop;
}

prim P_fmax()			      /* Maximum of top two floats */
{
    Sl(2 * Realsize);
    SREAL1(max(REAL1, REAL0));
    Realpop;
}

prim P_fneg()			      /* Negate top of stack */
{
    Sl(Realsize);
    SREAL0(- REAL0);
}

prim P_fabs()			      /* Absolute value of top of stack */
{
    Sl(Realsize);
    SREAL0(abs(REAL0));
}

prim P_fequal() 		      /* Test equality of top of stack */
{
    stackitem t;

    Sl(2 * Realsize);
    t = (REAL1 == REAL0) ? Truth : Falsity;
    Realpop2;
    Push = t;
}

prim P_funequal()		      /* Test inequality of top of stack */
{
    stackitem t;

    Sl(2 * Realsize);
    t = (REAL1 != REAL0) ? Truth : Falsity;
    Realpop2;
    Push = t;
}

prim P_fgtr()			      /* Test greater than */
{
    stackitem t;

    Sl(2 * Realsize);
    t = (REAL1 > REAL0) ? Truth : Falsity;
    Realpop2;
    Push = t;
}

prim P_flss()			      /* Test less than */
{
    stackitem t;

    Sl(2 * Realsize);
    t = (REAL1 < REAL0) ? Truth : Falsity;
    Realpop2;
    Push = t;
}

prim P_fgeq()			      /* Test greater than or equal */
{
    stackitem t;

    Sl(2 * Realsize);
    t = (REAL1 >= REAL0) ? Truth : Falsity;
    Realpop2;
    Push = t;
}

prim P_fleq()			      /* Test less than or equal */
{
    stackitem t;

    Sl(2 * Realsize);
    t = (REAL1 <= REAL0) ? Truth : Falsity;
    Realpop2;
    Push = t;
}

prim P_fdot()			      /* Print floating point top of stack */
{
    Sl(Realsize);

    V printf("%g ", REAL0);

    Realpop;
}

prim P_float()			      /* Convert integer to floating */
{
    atl_real r;

    Sl(1)
        So(Realsize - 1);
    r = S0;
    stk += Realsize - 1;
    SREAL0(r);
}

prim P_fix()			      /* Convert floating to integer */
{
    stackitem i;

    Sl(Realsize);
    i = (int) REAL0;
    Realpop;
    Push = i;
}

#ifdef MATH

#define Mathfunc(x) Sl(Realsize); SREAL0(x(REAL0))

prim P_acos()			      /* Arc cosine */
{
    Mathfunc(acos);
}

prim P_asin()			      /* Arc sine */
{
    Mathfunc(asin);
}

prim P_atan()			      /* Arc tangent */
{
    Mathfunc(atan);
}

prim P_atan2()			      /* Arc tangent:  y x -- atan */
{
    Sl(2 * Realsize);
    SREAL1(atan2(REAL1, REAL0));
    Realpop;
}

prim P_cos()			      /* Cosine */
{
    Mathfunc(cos);
}

prim P_exp()			      /* E ^ x */
{
    Mathfunc(exp);
}

prim P_log()			      /* Natural log */
{
    Mathfunc(log);
}

prim P_pow()			      /* X ^ Y */
{
    Sl(2 * Realsize);
    SREAL1(pow(REAL1, REAL0));
    Realpop;
}

prim P_sin()			      /* Sine */
{
    Mathfunc(sin);
}

prim P_sqrt()			      /* Square root */
{
    Mathfunc(sqrt);
}

prim P_tan()			      /* Tangent */
{
    Mathfunc(tan);
}
#undef Mathfunc
#endif /* MATH */
#endif /* REAL */

/*  Console I/O primitives  */

#ifdef CONIO
#ifdef LINUX

#endif

prim ATH_getConsole() {
    
    So(1);
    Push = (stackitem) sysConsole;
}

prim ATH_setConsole() {
    Sl(1)
    
    sysConsole = (Console *) S0;
    Pop;
}

prim ATH_brQemit() {
    bool rc;
    
    Sl(1);
    rc = ((Console *)S0)->qemit();
    Pop;
    So(1);
    Push = rc;
    
}

// ?emit
// Return true if I can output a char.
//
prim ATH_qemit() {
    /*
    bool rc;
    
    rc = sysConsole->qemit();
    
    Push=rc;
    */
    
    Push = (stackitem)sysConsole;
    ATH_brQemit();
}

prim ATH_brEmit() {
    uint8_t rc;
    Sl(2);
    
    rc = ((Console *)S0)->emit((uint8_t)S1);
    Pop2;
}

prim ATH_emit() {
    
    Sl(1);
    Push = (stackitem)sysConsole;
    
    ATH_brEmit();
    /*
    rc = sysConsole->emit((uint8_t)S0);
    
    Pop;
    */
}

prim ATH_brQkey() {
    bool rc;
    Sl(1);
    
    rc = ((Console *)S0)->qkey();
    Pop;
    
    So(1);
    Push = rc;
    
}

prim ATH_qkey() {
    /*
    bool rc;
    
    rc = sysConsole->qkey();
    
    So(1);
    Push = rc;
    */
    Push = (stackitem) sysConsole;
    ATH_brQkey();
    
}

prim ATH_brKey() {
    uint8_t k;
    
    Sl(1);
    k=((Console *)S0)->key();
    Pop;
    So(1);
    Push = k;
}

prim ATH_key() {
    /*
    uint8_t k;
    k=sysConsole->key();
    So(1);
    Push = k;
    */
    Push=(stackitem)sysConsole;
    ATH_brKey();
}

prim ATH_hex() {
    base = 16;
}

prim ATH_dec() {
    base = 10;
}
/* Print top of stack, pop it */
prim P_dot() {
    Sl(1);
    char buffer[32];

#ifdef EMBEDDED
//    sprintf(outBuffer,(base == 16 ? "%lX" : "%ld "), S0);  // EMBEDDED
    sprintf(buffer,(base == 16 ? "%lX" : "%ld "), S0);  // EMBEDDED
    sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
    V printf(base == 16 ? "%lX" : "%ld ", S0); // NOT EMBEDDED
#endif

    Pop;
}

/* Print value at address */
prim P_question() {
    char buffer[32];
    Sl(1);
    Hpc(S0);
    stackitem tmp;

    if ( ath_safe_memory == Truth ) {
        Hpc(S0);
    }
    tmp = *((stackitem *) S0);

#ifdef EMBEDDED
    sprintf(buffer,(base == 16 ? "%lX" : "%ld "), tmp);  // EMBEDDED
    sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
//    sprintf(outBuffer,(base == 16 ? "%lX" : "%ld "), *((stackitem *) S0)); // NOT EMBEDDED
#else
    V printf(base == 16 ? "%lX" : "%ld ", *((stackitem *) S0)); // NOT EMBEDDED
#endif
    Pop;
}

/* Carriage return */
prim P_cr() {
#ifdef EMBEDDED
    sysConsole->emit('\n');
    /*
    if(strlen(outBuffer) > 0 ) {
        strcat(outBuffer,"\n");
    } else {
        sprintf(outBuffer,"\n"); // EMBEDDED
    }
    */
#else
    V printf("\n"); // NOT EMBEDDED
#endif
}

/* Print entire contents of stack */
prim P_dots() {
    stackitem *tsp;

#ifdef EMBEDDED
    char tmpBuffer[80];  // One line of the screen.
    sprintf(tmpBuffer,"Stack: ");  // EMBEDDED
    sysConsole->writePipe((void *)tmpBuffer,strlen((char *)tmpBuffer));
#else
    V printf("Stack: ");    // NOT EMBEDDED
#endif

    if (stk == stackbot) {
#ifdef EMBEDDED
        sprintf(tmpBuffer,"Empty.");  // NOT EMBEDDED
        sysConsole->writePipe((void *)tmpBuffer,strlen((char *)tmpBuffer));
#else
        V printf("Empty."); // NOT EMBEDDED

#endif
    } else {
        for (tsp = stack; tsp < stk; tsp++) {
#ifdef EMBEDDED
            sprintf(tmpBuffer,(base == 16 ? "%lX " : "%ld "), *tsp); // EMBEDDED
            sysConsole->writePipe((void *)tmpBuffer,strlen((char *)tmpBuffer));
            // strcat(outBuffer,tmpBuffer);
#else
            V printf(base == 16 ? "%lX " : "%ld ", *tsp); // NOT EMBEDDED
#endif
        }
    }
}

/* Print literal string that follows */
prim P_dotquote() {
    Compiling;
    stringlit = True;		      /* Set string literal expected */
    Compconst(s_dotparen);	      /* Compile .( word */
}

/* Print literal string that follows */
prim P_dotparen() {
    if (ip == NULL) {		      /* If interpreting */
        stringlit = True;	      /* Set to print next string constant */
    } else {			      /* Otherwise, */
        /* print string literal in in-line code. */
#ifdef EMBEDDED
//        sprintf(outBuffer,"%s", ((char *) ip) + 1);  // EMBEDDED
    char *tmp = ((char *) ip) + 1;
    sysConsole->writePipe((void *)tmp,strlen((char *)tmp));
#else
        V printf("%s", ((char *) ip) + 1);  // NOT EMBEDDED
#endif
        Skipstring;		      /* And advance IP past it */
    }
}

/* Print string pointed to by stack */
prim P_type() {
    Sl(1);
    Hpc(S0);

#ifdef EMBEDDED
//    outBuffer[0]='\0';
    
    sysConsole->writePipe((void *)S0,strlen((char *)S0));
    /*
    if(strlen(outBuffer) > 0) {
        strcat(outBuffer,(char *)S0);
    } else {
        sprintf(outBuffer,"%s", (char *) S0); // EMBEDDED
    }
    */
#else
    V printf("%s", (char *) S0); // NOT EMBEDDED
#endif

    Pop;
}

/* List words */
prim P_words() {
#ifndef Keyhit
    int key = 0;
#endif
    dictword *dw = dict;

    while (dw != NULL) {

#ifdef EMBEDDED
        // TODO Not sure how, but fix this.
//        sprintf(outBuffer,"\n%s", dw->wname + 1); // EMBEDDED
        sysConsole->writePipe( (void *)(dw->wname + 1), strlen((char *)(dw->wname + 1)));
        P_cr();
#else
        V printf("\n%s", dw->wname + 1); // NOT EMBEDDED
#endif

        dw = dw->wnext;
#ifdef Keyhit
        if (kbquit()) {
            break;
        }
#else
        // If this system can't trap keystrokes, just stop the WORDS NOT 
        // listing after 20 words. 
        if (++key >= 20)
            break;
#endif
    }
    /*
#ifdef EMBEDDED
    sprintf(outBuffer,"\n"); // EMBEDDED
#else
    V printf("\n"); // NOT EMBEDDED
#endif
*/
}
#endif /* CONIO */
#ifdef FILEIO

prim P_file()			      /* Declare file */
{
    Ho(2);
    P_create(); 		      /* Create variable */
    Hstore = FileSent;		      /* Store file sentinel */
    Hstore = 0; 		      /* Mark file not open */
}

prim P_fopen()			      /* Open file: fname fmodes fd -- flag */
{
    FILE *fd;
    stackitem stat;

    Sl(3);
    Hpc(S2);
    Hpc(S0);
    Isfile(S0);
    fd = fopen((char *) S2, fopenmodes[S1]);
    if (fd == NULL) {
        stat = Falsity;
    } else {
        *(((stackitem *) S0) + 1) = (stackitem) fd;
        stat = Truth;
    }
    Pop2;
    S0 = stat;
}

prim P_fclose() 		      /* Close file: fd -- */
{
    Sl(1);
    Hpc(S0);
    Isfile(S0);
    Isopen(S0);
    V fclose(FileD(S0));
    *(((stackitem *) S0) + 1) = (stackitem) NULL;
    Pop;
}

prim P_fdelete()		      /* Delete file: fname -- flag */
{
    Sl(1);
    Hpc(S0);
    S0 = (unlink((char *) S0) == 0) ? Truth : Falsity;
}

prim P_fgetline()		      /* Get line: fd string -- flag */
{
    Sl(2);
    Hpc(S0);
    Isfile(S1);
    Isopen(S1);
    if (atl_fgetsp((char *) S0, 132, FileD(S1)) == NULL) {
        S1 = Falsity;
    } else {
        S1 = Truth;
    }
    Pop;
}

prim P_fputline()		      /* Put line: string fd -- flag */
{
    Sl(2);
    Hpc(S1);
    Isfile(S0);
    Isopen(S0);
    if (fputs((char *) S1, FileD(S0)) == EOF) {
        S1 = Falsity;
    } else {
        S1 = putc('\n', FileD(S0)) == EOF ? Falsity : Truth;
    }
    Pop;
}

prim P_fread()			      /* File read: fd len buf -- length */
{
    Sl(3);
    Hpc(S0);
    Isfile(S2);
    Isopen(S2);
    S2 = fread((char *) S0, 1, ((int) S1), FileD(S2));
    Pop2;
}

prim P_fwrite() 		      /* File write: len buf fd -- length */
{
    Sl(3);
    Hpc(S1);
    Isfile(S0);
    Isopen(S0);
    S2 = fwrite((char *) S1, 1, ((int) S2), FileD(S0));
    Pop2;
}

prim P_fgetc()			      /* File get character: fd -- char */
{
    Sl(1);
    Isfile(S0);
    Isopen(S0);
    S0 = getc(FileD(S0));	      /* Returns -1 if EOF hit */
}

prim P_fputc()			      /* File put character: char fd -- stat */
{
    Sl(2);
    Isfile(S0);
    Isopen(S0);
    S1 = putc((char) S1, FileD(S0));
    Pop;
}

prim P_ftell()			      /* Return file position:	fd -- pos */
{
    Sl(1);
    Isfile(S0);
    Isopen(S0);
    S0 = (stackitem) ftell(FileD(S0));
}

prim P_fseek()			      /* Seek file:  offset base fd -- */
{
    Sl(3);
    Isfile(S0);
    Isopen(S0);
    V fseek(FileD(S0), (long) S2, (int) S1);
    Npop(3);
}

prim P_fload()			      /* Load source file:  fd -- evalstat */
{
    int estat;
    FILE *fd;

    Sl(1);
    Isfile(S0);
    Isopen(S0);
    fd = FileD(S0);
    Pop;
    estat = atl_load(fd);
    So(1);
    Push = estat;
}
#endif /* FILEIO */

#ifdef EVALUATE

prim P_evaluate()
{				      /* string -- status */
    int es = ATL_SNORM;
    atl_statemark mk;
    atl_int scomm = atl_comment;      /* Stack comment pending state */
    dictword **sip = ip;	      /* Stack instruction pointer */
    char *sinstr = instream;	      /* Stack input stream */
    char *estring;

    Sl(1);
    Hpc(S0);
    estring = (char *) S0;	      /* Get string to evaluate */
    Pop;			      /* Pop so it sees arguments below it */
    atl_mark(&mk);		      /* Mark in case of error */
    ip = NULL;			      /* Fool atl_eval into interp state */
    if ((es = atl_eval(estring)) != ATL_SNORM) {
        atl_unwind(&mk);
    }
    /* If there were no other errors, check for a runaway comment.  If
       we ended the file in comment-ignore mode, set the runaway comment
       error status and unwind the file.  */
    if ((es == ATL_SNORM) && (atl_comment != 0)) {
        es = ATL_RUNCOMM;
        atl_unwind(&mk);
    }
    atl_comment = scomm;	      /* Unstack comment pending status */
    ip = sip;			      /* Unstack instruction pointer */
    instream = sinstr;		      /* Unstack input stream */
    So(1);
    Push = es;			      /* Return eval status on top of stack */
}
#endif /* EVALUATE */

/*  Stack mechanics  */

prim P_depth()			      /* Push stack depth */
{
    stackitem s = stk - stack;

    So(1);
    Push = s;
}

prim P_clear()			      /* Clear stack */
{
    stk = stack;
}

prim P_dup()			      /* Duplicate top of stack */
{
    stackitem s;

    Sl(1);
    So(1);
    s = S0;
    Push = s;
}

prim P_drop()			      /* Drop top item on stack */
{
    Sl(1);
    Pop;
}

prim P_swap()			      /* Exchange two top items on stack */
{
    stackitem t;

    Sl(2);
    t = S1;
    S1 = S0;
    S0 = t;
}

prim P_over()			      /* Push copy of next to top of stack */
{
    stackitem s;

    Sl(2);
    So(1);
    s = S1;
    Push = s;
}

prim P_pick()			      /* Copy indexed item from stack */
{
    Sl(2);
    S0 = stk[-(2 + S0)];
}

prim P_rot()			      /* Rotate 3 top stack items */
{
    stackitem t;

    Sl(3);
    t = S0;
    S0 = S2;
    S2 = S1;
    S1 = t;
}

prim P_minusrot()		      /* Reverse rotate 3 top stack items */
{
    stackitem t;

    Sl(3);
    t = S0;
    S0 = S1;
    S1 = S2;
    S2 = t;
}

prim P_roll()			      /* Rotate N top stack items */
{
    stackitem i, j, t;

    Sl(1);
    i = S0;
    Pop;
    Sl(i + 1);
    t = stk[-(i + 1)];
    for (j = -(i + 1); j < -1; j++)
        stk[j] = stk[j + 1];
    S0 = t;
}

prim P_tor()			      /* Transfer stack top to return stack */
{
    Rso(1);
    Sl(1);
    Rpush = (rstackitem) S0;
    Pop;
}

prim P_rfrom()			      /* Transfer return stack top to stack */
{
    Rsl(1);
    So(1);
    Push = (stackitem) R0;
    Rpop;
}

prim P_rfetch() 		      /* Fetch top item from return stack */
{
    Rsl(1);
    So(1);
    Push = (stackitem) R0;
}

#ifdef Macintosh
/* This file creates more than 32K of object code on the Mac, which causes
   MPW to barf.  So, we split it up into two code segments of <32K at this
   point. */
#pragma segment TOOLONG
#endif /* Macintosh */

/*  Double stack manipulation items  */

#ifdef DOUBLE

prim P_2dup()			      /* Duplicate stack top doubleword */
{
    stackitem s;

    Sl(2);
    So(2);
    s = S1;
    Push = s;
    s = S1;
    Push = s;
}

prim P_2drop()			      /* Drop top two items from stack */
{
    Sl(2);
    stk -= 2;
}

prim P_2swap()			      /* Swap top two double items on stack */
{
    stackitem t;

    Sl(4);
    t = S2;
    S2 = S0;
    S0 = t;
    t = S3;
    S3 = S1;
    S1 = t;
}

prim P_2over()			      /* Extract second pair from stack */
{
    stackitem s;

    Sl(4);
    So(2);
    s = S3;
    Push = s;
    s = S3;
    Push = s;
}

prim P_2rot()			      /* Move third pair to top of stack */
{
    stackitem t1, t2;

    Sl(6);
    t2 = S5;
    t1 = S4;
    S5 = S3;
    S4 = S2;
    S3 = S1;
    S2 = S0;
    S1 = t2;
    S0 = t1;
}

prim P_2variable()		      /* Declare double variable */
{
    P_create(); 		      /* Create dictionary item */
    Ho(2);
    Hstore = 0; 		      /* Initial value = 0... */
    Hstore = 0; 		      /* ...in both words */
}

prim P_2con()			      /* Push double value in body */
{
    So(2);
    Push = *(((stackitem *) curword) + Dictwordl);
    Push = *(((stackitem *) curword) + Dictwordl + 1);
}

prim P_2constant()		      /* Declare double word constant */
{
    Sl(1);
    P_create(); 		      /* Create dictionary item */
    createword->wcode = P_2con;       /* Set code to constant push */
    Ho(2);
    Hstore = S1;		      /* Store double word constant value */
    Hstore = S0;		      /* in the two words of body */
    Pop2;
}

prim P_2bang()			      /* Store double value into address */
{
    stackitem *sp;

    Sl(2);
    Hpc(S0);
    sp = (stackitem *) S0;
    *sp++ = S2;
    *sp = S1;
    Npop(3);
}

prim P_2at()			      /* Fetch double value from address */
{
    stackitem *sp;

    Sl(1);
    So(1);
    Hpc(S0);
    sp = (stackitem *) S0;
    S0 = *sp++;
    Push = *sp;
}
#endif /* DOUBLE */

/*  Data transfer primitives  */

// Push instruction stream literal
prim P_dolit() {
    So(1);
    char buffer[16];
#ifdef TRACE
    if (atl_trace) {
#ifdef EMBEDDED
        sprintf(buffer,"%ld ", (long) *ip); // EMBEDDED
        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
        V printf("%ld ", (long) *ip); // NOT EMBEDDED
#endif
    }
#endif
    Push = (stackitem) *ip++;	      /* Push the next datum from the
                                         instruction stream. */
}

/*  Control flow primitives  */

prim P_nest()			      /* Invoke compiled word */
{
    Rso(1);
#ifdef WALKBACK
    *wbptr++ = curword; 	      /* Place word on walkback stack */
#endif
    Rpush = ip; 		      /* Push instruction pointer */
    ip = (((dictword **) curword) + Dictwordl);
}

prim P_exit()			      /* Return to top of return stack */
{
    Rsl(1);
#ifdef WALKBACK
    wbptr = (wbptr > wback) ? wbptr - 1 : wback;
#endif
    ip = R0;			      /* Set IP to top of return stack */
    Rpop;
}

prim P_branch() 		      /* Jump to in-line address */
{
    ip += (stackitem) *ip;	      /* Jump addresses are IP-relative */
}

prim P_qbranch()		      /* Conditional branch to in-line addr */
{
    Sl(1);
    if (S0 == 0)		      /* If flag is false */
        ip += (stackitem) *ip;	      /* then branch. */
    else			      /* Otherwise */
        ip++;			      /* skip the in-line address. */
    Pop;
}

prim P_if()			      /* Compile IF word */
{
    Compiling;
    Compconst(s_qbranch);	      /* Compile question branch */
    So(1);
    Push = (stackitem) hptr;	      /* Save backpatch address on stack */
    Compconst(0);		      /* Compile place-holder address cell */
}

prim P_else()			      /* Compile ELSE word */
{
    stackitem *bp;

    Compiling;
    Sl(1);
    Compconst(s_branch);	      /* Compile branch around other clause */
    Compconst(0);		      /* Compile place-holder address cell */
    Hpc(S0);
    bp = (stackitem *) S0;	      /* Get IF backpatch address */
    *bp = hptr - bp;
    S0 = (stackitem) (hptr - 1);      /* Update backpatch for THEN */
}

prim P_then()			      /* Compile THEN word */
{
    stackitem *bp;

    Compiling;
    Sl(1);
    Hpc(S0);
    bp = (stackitem *) S0;	      /* Get IF/ELSE backpatch address */
    *bp = hptr - bp;
    Pop;
}

prim P_qdup()			      /* Duplicate if nonzero */
{
    Sl(1);
    if (S0 != 0) {
        stackitem s = S0;
        So(1);
        Push = s;
    }
}

prim P_begin()			      /* Compile BEGIN */
{
    Compiling;
    So(1);
    Push = (stackitem) hptr;	      /* Save jump back address on stack */
}

prim P_until()			      /* Compile UNTIL */
{
    stackitem off;
    stackitem *bp;

    Compiling;
    Sl(1);
    Compconst(s_qbranch);	      /* Compile question branch */
    Hpc(S0);
    bp = (stackitem *) S0;	      /* Get BEGIN address */
    off = -(hptr - bp);
    Compconst(off);		      /* Compile negative jumpback address */
    Pop;
}

prim P_again()			      /* Compile AGAIN */
{
    stackitem off;
    stackitem *bp;

    Compiling;
    Compconst(s_branch);	      /* Compile unconditional branch */
    Hpc(S0);
    bp = (stackitem *) S0;	      /* Get BEGIN address */
    off = -(hptr - bp);
    Compconst(off);		      /* Compile negative jumpback address */
    Pop;
}

prim P_while()			      /* Compile WHILE */
{
    Compiling;
    So(1);
    Compconst(s_qbranch);	      /* Compile question branch */
    Compconst(0);		      /* Compile place-holder address cell */
    Push = (stackitem) (hptr - 1);    /* Queue backpatch for REPEAT */
}

prim P_repeat() 		      /* Compile REPEAT */
{
    stackitem off;
    stackitem *bp1, *bp;

    Compiling;
    Sl(2);
    Hpc(S0);
    bp1 = (stackitem *) S0;	      /* Get WHILE backpatch address */
    Pop;
    Compconst(s_branch);	      /* Compile unconditional branch */
    Hpc(S0);
    bp = (stackitem *) S0;	      /* Get BEGIN address */
    off = -(hptr - bp);
    Compconst(off);		      /* Compile negative jumpback address */
    *bp1 = hptr - bp1;                /* Backpatch REPEAT's jump out of loop */
    Pop;
}

prim P_do()			      /* Compile DO */
{
    Compiling;
    Compconst(s_xdo);		      /* Compile runtime DO word */
    So(1);
    Compconst(0);		      /* Reserve cell for LEAVE-taking */
    Push = (stackitem) hptr;	      /* Save jump back address on stack */
}

prim P_xdo()			      /* Execute DO */
{
    Sl(2);
    Rso(3);
    Rpush = ip + ((stackitem) *ip);   /* Push exit address from loop */
    ip++;			      /* Increment past exit address word */
    Rpush = (rstackitem) S1;	      /* Push loop limit on return stack */
    Rpush = (rstackitem) S0;	      /* Iteration variable initial value to
                                         return stack */
    stk -= 2;
}

prim P_qdo()			      /* Compile ?DO */
{
    Compiling;
    Compconst(s_xqdo);		      /* Compile runtime ?DO word */
    So(1);
    Compconst(0);		      /* Reserve cell for LEAVE-taking */
    Push = (stackitem) hptr;	      /* Save jump back address on stack */
}

prim P_xqdo()			      /* Execute ?DO */
{
    Sl(2);
    if (S0 == S1) {
        ip += (stackitem) *ip;
    } else {
        Rso(3);
        Rpush = ip + ((stackitem) *ip);/* Push exit address from loop */
        ip++;			      /* Increment past exit address word */
        Rpush = (rstackitem) S1;      /* Push loop limit on return stack */
        Rpush = (rstackitem) S0;      /* Iteration variable initial value to
                                         return stack */
    }
    stk -= 2;
}

prim P_loop()			      /* Compile LOOP */
{
    stackitem off;
    stackitem *bp;

    Compiling;
    Sl(1);
    Compconst(s_xloop); 	      /* Compile runtime loop */
    Hpc(S0);
    bp = (stackitem *) S0;	      /* Get DO address */
    off = -(hptr - bp);
    Compconst(off);		      /* Compile negative jumpback address */
    *(bp - 1) = (hptr - bp) + 1;      /* Backpatch exit address offset */
    Pop;
}

prim P_ploop()			      /* Compile +LOOP */
{
    stackitem off;
    stackitem *bp;

    Compiling;
    Sl(1);
    Compconst(s_pxloop);	      /* Compile runtime +loop */
    Hpc(S0);
    bp = (stackitem *) S0;	      /* Get DO address */
    off = -(hptr - bp);
    Compconst(off);		      /* Compile negative jumpback address */
    *(bp - 1) = (hptr - bp) + 1;      /* Backpatch exit address offset */
    Pop;
}

prim P_xloop()			      /* Execute LOOP */
{
    Rsl(3);
    R0 = (rstackitem) (((stackitem) R0) + 1);
    if (((stackitem) R0) == ((stackitem) R1)) {
        rstk -= 3;		      /* Pop iteration variable and limit */
        ip++;			      /* Skip the jump address */
    } else {
        ip += (stackitem) *ip;
    }
}

prim P_xploop() 		      /* Execute +LOOP */
{
    stackitem niter;

    Sl(1);
    Rsl(3);

    niter = ((stackitem) R0) + S0;
    Pop;
    if ((niter >= ((stackitem) R1)) &&
            (((stackitem) R0) < ((stackitem) R1))) {
        rstk -= 3;		      /* Pop iteration variable and limit */
        ip++;			      /* Skip the jump address */
    } else {
        ip += (stackitem) *ip;
        R0 = (rstackitem) niter;
    }
}

prim P_leave()			      /* Compile LEAVE */
{
    Rsl(3);
    ip = R2;
    rstk -= 3;
}

prim P_i()			      /* Obtain innermost loop index */
{
    Rsl(3);
    So(1);
    Push = (stackitem) R0;            /* It's the top item on return stack */
}

prim P_j()			      /* Obtain next-innermost loop index */
{
    Rsl(6);
    So(1);
    Push = (stackitem) rstk[-4];      /* It's the 4th item on return stack */
}

prim P_quit()			      /* Terminate execution */
{
    rstk = rstack;		      /* Clear return stack */
#ifdef WALKBACK
    wbptr = wback;
#endif
    ip = NULL;			      /* Stop execution of current word */
}

prim P_abort()			      /* Abort, clearing data stack */
{
    P_clear();			      /* Clear the data stack */
    P_quit();			      /* Shut down execution */
}

/* Abort, printing message */
prim P_abortq() {
    if (state) {
        stringlit = True;	      /* Set string literal expected */
        Compconst(s_abortq);	      /* Compile ourselves */
    } else {
        /* Otherwise, print string literal in in-line code. */
#ifdef EMBEDDED
//        sprintf(outBuffer,"%s", (char *) ip);  // EMBEDDED
        sysConsole->writePipe((void *)ip,strlen((char *)ip));
#else
        V printf("%s", (char *) ip);  // NOT EMBEDDED
#endif

#ifdef WALKBACK
        pwalkback();
#endif /* WALKBACK */
        P_abort();		      /* Abort */
        atl_comment = state = Falsity;/* Reset all interpretation state */
        forgetpend = defpend = stringlit =
            tickpend = ctickpend = False;
    }
}

/*  Compilation primitives  */

prim P_immediate()		      /* Mark most recent word immediate */
{
    dict->wname[0] |= IMMEDIATE;
}

prim P_lbrack() 		      /* Set interpret state */
{
    Compiling;
    state = Falsity;
}

prim P_rbrack() 		      /* Restore compile state */
{
    state = Truth;
}

Exported void P_dodoes()	      /* Execute indirect call on method */
{
    Rso(1);
    So(1);
    Rpush = ip; 		      /* Push instruction pointer */
#ifdef WALKBACK
    *wbptr++ = curword; 	      /* Place word on walkback stack */
#endif
    /* The compiler having craftily squirreled away the DOES> clause
       address before the word definition on the heap, we back up to
       the heap cell before the current word and load the pointer from
       there.  This is an ABSOLUTE heap address, not a relative offset. */
    ip = *((dictword ***) (((stackitem *) curword) - 1));

    /* Push the address of this word's body as the argument to the
       DOES> clause. */
    Push = (stackitem) (((stackitem *) curword) + Dictwordl);
}

prim P_does()			      /* Specify method for word */
{

    /* O.K., we were compiling our way through this definition and we've
       encountered the Dreaded and Dastardly Does.  Here's what we do
       about it.  The problem is that when we execute the word, we
       want to push its address on the stack and call the code for the
       DOES> clause by diverting the IP to that address.  But...how
       are we to know where the DOES> clause goes without adding a
       field to every word in the system just to remember it.  Recall
       that since this system is portable we can't cop-out through
       machine code.  Further, we can't compile something into the
       word because the defining code may have already allocated heap
       for the word's body.  Yukkkk.  Oh well, how about this?  Let's
       copy any and all heap allocated for the word down one stackitem
       and then jam the DOES> code address BEFORE the link field in
       the word we're defining.

       Then, when (DOES>) (P_dodoes) is called to execute the word, it
       will fetch that code address by backing up past the start of
       the word and seting IP to it.  Note that FORGET must recognise
       such words (by the presence of the pointer to P_dodoes() in
       their wcode field, in case you're wondering), and make sure to
       deallocate the heap word containing the link when a
       DOES>-defined word is deleted.  */

    if (createword != NULL) {
        stackitem *sp = ((stackitem *) createword), *hp;

        Rsl(1);
        Ho(1);

        /* Copy the word definition one word down in the heap to
           permit us to prefix it with the DOES clause address. */

        for (hp = hptr - 1; hp >= sp; hp--)
            *(hp + 1) = *hp;
        hptr++; 		      /* Expand allocated length of word */
        *sp++ = (stackitem) ip;       /* Store DOES> clause address before
                                         word's definition structure. */
        createword = (dictword *) sp; /* Move word definition down 1 item */
        createword->wcode = P_dodoes; /* Set code field to indirect jump */

        /* Now simulate an EXIT to bail out of the definition without
           executing the DOES> clause at definition time. */

        ip = R0;		      /* Set IP to top of return stack */
#ifdef WALKBACK
        wbptr = (wbptr > wback) ? wbptr - 1 : wback;
#endif
        Rpop;			      /* Pop the return stack */
    }
}

prim P_colon()			      /* Begin compilation */
{
    state = Truth;		      /* Set compilation underway */
    P_create(); 		      /* Create conventional word */
}

prim P_semicolon()		      /* End compilation */
{
    Compiling;
    Ho(1);
    Hstore = s_exit;
    state = Falsity;		      /* No longer compiling */
    /* We wait until now to plug the P_nest code so that it will be
       present only in completed definitions. */
    if (createword != NULL)
        createword->wcode = P_nest;   /* Use P_nest for code */
    createword = NULL;		      /* Flag no word being created */
}

/* Take address of next word */
prim P_tick() {
    int i;
    char buffer[255];

    /* Try to get next symbol from the input stream.  If
       we can't, and we're executing a compiled word,
       report an error.  Since we can't call back to the
       calling program for more input, we're stuck. */

    i = token(&instream);	      /* Scan for next token */
    if (i != TokNull) {
        if (i == TokWord) {
            dictword *di;

            ucase(tokbuf);
            if ((di = lookup(tokbuf)) != NULL) {
                So(1);
                Push = (stackitem) di; /* Push word compile address */
            } else {
#ifdef EMBEDDED
                sprintf(buffer," '%s' undefined ", tokbuf); // EMBEDDED
                sysConsole->writePipe((char *)buffer,strlen((char *)buffer));
#else
                V printf(" '%s' undefined ", tokbuf); // NOT EMBEDDED
#endif
            }
        } else {
#ifdef EMBEDDED
            sprintf(buffer,"\nWord not specified when expected.\n"); // EMBEDDED
            sysConsole->writePipe((char *)buffer,strlen((char *)buffer));
#else
            V printf("\nWord not specified when expected.\n"); // NOT EMBEDDED
#endif
            P_abort();
        }
    } else {
        /* O.K., there was nothing in the input stream.  Set the
           tickpend flag to cause the compilation address of the next
           token to be pushed when it's supplied on a subsequent input
           line. */
        if (ip == NULL) {
            tickpend = True;	      /* Set tick pending */
        } else {
#ifdef EMBEDDED
            sprintf(buffer,"\nWord requested by ` not on same input line.\n"); // EMBEDDED
            sysConsole->writePipe((char *)buffer,strlen((char *)buffer));
#else
            V printf("\nWord requested by ` not on same input line.\n"); // NOT EMBEDDED
#endif
            P_abort();
        }
    }
}

prim P_bracktick()		      /* Compile in-line code address */
{
    Compiling;
    ctickpend = True;		      /* Force literal treatment of next
                                     word in compile stream */
}

prim P_execute()		      /* Execute word pointed to by stack */
{
    dictword *wp;

    Sl(1);
    wp = (dictword *) S0;	      /* Load word address from stack */
    Pop;			      /* Pop data stack before execution */
    exword(wp); 		      /* Recursively call exword() to run
                                 the word. */
}

prim P_body()			      /* Get body address for word */
{
    Sl(1);
    S0 += Dictwordl * sizeof(stackitem);
}

prim P_state()			      /* Get state of system */
{
    So(1);
    Push = (stackitem) &state;
}

/*  Definition field access primitives	*/

#ifdef DEFFIELDS

prim P_find()			      /* Look up word in dictionary */
{
    dictword *dw;

    Sl(1);
    So(1);
    Hpc(S0);
    V strcpy(tokbuf, (char *) S0);    /* Use built-in token buffer... */
    dw = lookup(tokbuf);              /* So ucase() in lookup() doesn't wipe */
    /* the token on the stack */
    if (dw != NULL) {
        S0 = (stackitem) dw;
        /* Push immediate flag */
        Push = (dw->wname[0] & IMMEDIATE) ? 1 : -1;
    } else {
        Push = 0;
    }
}

#define DfOff(fld)  (((char *) &(dict->fld)) - ((char *) dict))

prim P_toname() 		      /* Find name field from compile addr */
{
    Sl(1);
    S0 += DfOff(wname);
}

/* Find link field from compile addr */
prim P_tolink() {
    char buffer[80];
    
    if (DfOff(wnext) != 0) {
#ifdef EMBEDDED
        sprintf(buffer,"\n>LINK Foulup--wnext is not at zero!\n");  // EMBEDDED
        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
        V printf("\n>LINK Foulup--wnext is not at zero!\n");  // NOT EMBEDDED
#endif
    }
    /*  Sl(1);
        S0 += DfOff(wnext);  */	      /* Null operation.  Wnext is first */
}

prim P_frombody()		      /* Get compile address from body */
{
    Sl(1);
    S0 -= Dictwordl * sizeof(stackitem);
}

prim P_fromname()		      /* Get compile address from name */
{
    Sl(1);
    S0 -= DfOff(wname);
}

/* Get compile address from link */
prim P_fromlink() {
    char buffer[80];
    
    if (DfOff(wnext) != 0) {
#ifdef EMBEDDED
        sprintf(buffer,"\nLINK> Foulup--wnext is not at zero!\n"); // EMBEDDED
        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
        V printf("\nLINK> Foulup--wnext is not at zero!\n"); // NOT EMBEDDED
#endif
    }
    /*  Sl(1);
        S0 -= DfOff(wnext);  */	      /* Null operation.  Wnext is first */
}

#undef DfOff

#define DfTran(from,to) (((char *) &(dict->to)) - ((char *) &(dict->from)))

prim P_nametolink()		      /* Get from name field to link */
{
    char *from, *to;

    Sl(1);
    /*
       S0 -= DfTran(wnext, wname);
       */
    from = (char *) &(dict->wnext);
    to = (char *) &(dict->wname);
    S0 -= (to - from);
}

prim P_linktoname()		      /* Get from link field to name */
{
    char *from, *to;

    Sl(1);
    /*
       S0 += DfTran(wnext, wname);
       */
    from = (char *) &(dict->wnext);
    to = (char *) &(dict->wname);
    S0 += (to - from);
}

prim P_fetchname()		      /* Copy word name to string buffer */
{
    Sl(2);			      /* nfa string -- */
    Hpc(S0);
    Hpc(S1);
    /* Since the name buffers aren't in the system heap, but
       rather are separately allocated with alloc(), we can't
       check the name pointer references.  But, hey, if the user's
       futzing with word dictionary items on the heap in the first
       place, there's a billion other ways to bring us down at
       his command. */
    V strcpy((char *) S0, *((char **) S1) + 1);
    Pop2;
}

prim P_storename()		      /* Store string buffer in word name */
{
    char tflags;
    char *cp;

    Sl(2);			      /* string nfa -- */
    Hpc(S0);			      /* See comments in P_fetchname above */
    Hpc(S1);			      /* checking name pointers */
    tflags = **((char **) S0);
    free(*((char **) S0));
    *((char **) S0) = cp = alloc((unsigned int) (strlen((char *) S1) + 2));
    V strcpy(cp + 1, (char *) S1);
    *cp = tflags;
    Pop2;
}

#endif /* DEFFIELDS */

#ifdef SYSTEM
prim P_system()
{				      /* string -- status */
    Sl(1);
    Hpc(S0);
    S0 = system((char *) S0);
}

#endif /* SYSTEM */

#ifdef TRACE
prim P_trace()			      /* Set or clear tracing of execution */
{
    Sl(1);
    atl_trace = (S0 == 0) ? Falsity : Truth;
    Pop;
}
#endif /* TRACE */

#ifdef WALKBACK
prim P_walkback()		      /* Set or clear error walkback */
{
    Sl(1);
    atl_walkback = (S0 == 0) ? Falsity : Truth;
    Pop;
}
#endif /* WALKBACK */

#ifdef WORDSUSED

/* List words used by program */
prim P_wordsused() {
    char buffer[80];
    dictword *dw = dict;

    while (dw != NULL) {
        if (*(dw->wname) & WORDUSED) {
#ifdef EMBEDDED
            sprintf(buffer,"\n%s", dw->wname + 1); // EMBEDDED
            sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
            V printf("\n%s", dw->wname + 1); // NOT EMBEDDED
#endif
        }
#ifdef Keyhit
        if (kbquit()) {
            break;
        }
#endif
        dw = dw->wnext;
    }
    /*
#ifdef EMBEDDED
    sprintf(outBuffer,"\n"); // EMBEDDED
#else
    V printf("\n"); // NOT EMBEDDED
#endif
*/
}

/* List words not used by program */
prim P_wordsunused() {
    char buffer[80];
    dictword *dw = dict;

    while (dw != NULL) {
        if (!(*(dw->wname) & WORDUSED)) {
#ifdef EMBEDDED
            sprintf(buffer,"\n%s", dw->wname + 1); // EMBEDDED
            sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
            V printf("\n%s", dw->wname + 1); // NOT EMBEDDED
#endif
        }
#ifdef Keyhit
        if (kbquit()) {
            break;
        }
#endif
        dw = dw->wnext;
    }
    /*
#ifdef EMBEDDED
    sprintf(outBuffer,"\n");  // EMBEDDED
#else
    V printf("\n");  // NOT EMBEDDED
#endif
*/
}
#endif /* WORDSUSED */

#ifdef COMPILERW

prim P_brackcompile()		      /* Force compilation of immediate word */
{
    Compiling;
    cbrackpend = True;		      /* Set [COMPILE] pending */
}

prim P_literal()		      /* Compile top of stack as literal */
{
    Compiling;
    Sl(1);
    Ho(2);
    Hstore = s_lit;		      /* Compile load literal word */
    Hstore = S0;		      /* Compile top of stack in line */
    Pop;
}

prim P_compile()		      /* Compile address of next inline word */
{
    Compiling;
    Ho(1);
    Hstore = (stackitem) *ip++;       /* Compile the next datum from the
                                         instruction stream. */
}

prim P_backmark()		      /* Mark backward backpatch address */
{
    Compiling;
    So(1);
    Push = (stackitem) hptr;	      /* Push heap address onto stack */
}

prim P_backresolve()		      /* Emit backward jump offset */
{
    stackitem offset;

    Compiling;
    Sl(1);
    Ho(1);
    Hpc(S0);
    offset = -(hptr - (stackitem *) S0);
    Hstore = offset;
    Pop;
}

prim P_fwdmark()		      /* Mark forward backpatch address */
{
    Compiling;
    Ho(1);
    Push = (stackitem) hptr;	      /* Push heap address onto stack */
    Hstore = 0;
}

prim P_fwdresolve()		      /* Emit forward jump offset */
{
    stackitem offset;

    Compiling;
    Sl(1);
    Hpc(S0);
    offset = (hptr - (stackitem *) S0);
    *((stackitem *) S0) = offset;
    Pop;
}

#endif /* COMPILERW */

/*  Table of primitive words  */

static struct primfcn primt[] = {
    {(char *)"0+", P_plus},
    {(char *)"0-", P_minus},
    {(char *)"0*", P_times},
    {(char *)"0/", P_div},
    {(char *)"0MOD", P_mod},
    {(char *)"0/MOD", P_divmod},
    {(char *)"0MIN", P_min},
    {(char *)"0MAX", P_max},
    {(char *)"0NEGATE", P_neg},
    {(char *)"0ABS", P_abs},
    {(char *)"0=", P_equal},
    {(char *)"0<>", P_unequal},
    {(char *)"0>", P_gtr},
    {(char *)"0<", P_lss},
    {(char *)"0>=", P_geq},
    {(char *)"0<=", P_leq},

    {(char *)"0AND", P_and},
    {(char *)"0OR", P_or},
    {(char *)"0XOR", P_xor},
    {(char *)"0NOT", P_not},
    {(char *)"0SHIFT", P_shift},

    {(char *)"0DEPTH", P_depth},
    {(char *)"0CLEAR", P_clear},
    {(char *)"0DUP", P_dup},
    {(char *)"0DROP", P_drop},
    {(char *)"0SWAP", P_swap},
    {(char *)"0OVER", P_over},
    {(char *)"0PICK", P_pick},
    {(char *)"0ROT", P_rot},
    {(char *)"0-ROT", P_minusrot},
    {(char *)"0ROLL", P_roll},
    {(char *)"0>R", P_tor},
    {(char *)"0R>", P_rfrom},
    {(char *)"0R@", P_rfetch},

#ifdef SHORTCUTA
    {(char *)"01+", P_1plus},
    {(char *)"02+", P_2plus},
    {(char *)"01-", P_1minus},
    {(char *)"02-", P_2minus},
    {(char *)"02*", P_2times},
    {(char *)"02/", P_2div},
#endif /* SHORTCUTA */

#ifdef SHORTCUTC
    {(char *)"00=", P_0equal},
    {(char *)"00<>", P_0notequal},
    {(char *)"00>", P_0gtr},
    {(char *)"00<", P_0lss},
#endif /* SHORTCUTC */

#ifdef DOUBLE
    {(char *)"02DUP", P_2dup},
    {(char *)"02DROP", P_2drop},
    {(char *)"02SWAP", P_2swap},
    {(char *)"02OVER", P_2over},
    {(char *)"02ROT", P_2rot},
    {(char *)"02VARIABLE", P_2variable},
    {(char *)"02CONSTANT", P_2constant},
    {(char *)"02!", P_2bang},
    {(char *)"02@", P_2at},
#endif /* DOUBLE */

    {(char *)"0VARIABLE", P_variable},
    {(char *)"0CONSTANT", P_constant},
    {(char *)"0VALUE", P_constant},
    {(char *)"0!", P_bang},
    {(char *)"0@", P_at},
    {(char *)"0+!", P_plusbang},
    {(char *)"0ALLOT", P_allot},
    {(char *)"0,", P_comma},
    {(char *)"0C!", P_cbang},
    {(char *)"0C@", P_cat},
    {(char *)"0C,", P_ccomma},
    {(char *)"0C=", P_cequal},
    {(char *)"0HERE", P_here},

#ifdef ARRAY
    {(char *)"0ARRAY", P_array},
#endif

#ifdef STRING
    {(char *)"0(STRLIT)", P_strlit},
    {(char *)"0STRING", P_string},
    {(char *)"0STRCPY", P_strcpy},
    {(char *)"0S!", P_strcpy},
    {(char *)"0STRCAT", P_strcat},
    {(char *)"0S+", P_strcat},
    {(char *)"0STRLEN", P_strlen},
    {(char *)"0STRCMP", P_strcmp},
    {(char *)"0STRCHAR", P_strchar},
    {(char *)"0SUBSTR", P_substr},
    {(char *)"0COMPARE", P_strcmp},
    {(char *)"0STRFORM", P_strform},
#ifdef REAL
    {(char *)"0FSTRFORM", P_fstrform},
    {(char *)"0STRREAL", P_strreal},
#endif
    {(char *)"0STRINT", P_strint},
#endif /* STRING */

#ifdef REAL
    {(char *)"0(FLIT)", P_flit},
    {(char *)"0F+", P_fplus},
    {(char *)"0F-", P_fminus},
    {(char *)"0F*", P_ftimes},
    {(char *)"0F/", P_fdiv},
    {(char *)"0FMIN", P_fmin},
    {(char *)"0FMAX", P_fmax},
    {(char *)"0FNEGATE", P_fneg},
    {(char *)"0FABS", P_fabs},
    {(char *)"0F=", P_fequal},
    {(char *)"0F<>", P_funequal},
    {(char *)"0F>", P_fgtr},
    {(char *)"0F<", P_flss},
    {(char *)"0F>=", P_fgeq},
    {(char *)"0F<=", P_fleq},
    {(char *)"0F.", P_fdot},
    {(char *)"0FLOAT", P_float},
    {(char *)"0FIX", P_fix},
#ifdef MATH
    {(char *)"0ACOS", P_acos},
    {(char *)"0ASIN", P_asin},
    {(char *)"0ATAN", P_atan},
    {(char *)"0ATAN2", P_atan2},
    {(char *)"0COS", P_cos},
    {(char *)"0EXP", P_exp},
    {(char *)"0LOG", P_log},
    {(char *)"0POW", P_pow},
    {(char *)"0SIN", P_sin},
    {(char *)"0SQRT", P_sqrt},
    {(char *)"0TAN", P_tan},
#endif /* MATH */
#endif /* REAL */

    {(char *)"0(NEST)", P_nest},
    {(char *)"0EXIT", P_exit},
    {(char *)"0(LIT)", P_dolit},
    {(char *)"0BRANCH", P_branch},
    {(char *)"0?BRANCH", P_qbranch},
    {(char *)"1IF", P_if},
    {(char *)"1ELSE", P_else},
    {(char *)"1THEN", P_then},
    {(char *)"0?DUP", P_qdup},
    {(char *)"1BEGIN", P_begin},
    {(char *)"1UNTIL", P_until},
    {(char *)"1AGAIN", P_again},
    {(char *)"1WHILE", P_while},
    {(char *)"1REPEAT", P_repeat},
    {(char *)"1DO", P_do},
    {(char *)"1?DO", P_qdo},
    {(char *)"1LOOP", P_loop},
    {(char *)"1+LOOP", P_ploop},
    {(char *)"0(XDO)", P_xdo},
    {(char *)"0(X?DO)", P_xqdo},
    {(char *)"0(XLOOP)", P_xloop},
    {(char *)"0(+XLOOP)", P_xploop},
    {(char *)"0LEAVE", P_leave},
    {(char *)"0I", P_i},
    {(char *)"0J", P_j},
    {(char *)"0QUIT", P_quit},
    {(char *)"0ABORT", P_abort},
    {(char *)"1ABORT\"", P_abortq},

#ifdef SYSTEM
    {(char *)"0SYSTEM", P_system},
#endif
#ifdef TRACE
    {(char *)"0TRACE", P_trace},
#endif
#ifdef WALKBACK
    {(char *)"0WALKBACK", P_walkback},
#endif

#ifdef WORDSUSED
    {(char *)"0WORDSUSED", P_wordsused},
    {(char *)"0WORDSUNUSED", P_wordsunused},
#endif

#ifdef MEMSTAT
    {(char *)"0MEMSTAT", atl_memstat},
#endif

    {(char *)"0:", P_colon},
    {(char *)"1;", P_semicolon},
    {(char *)"0IMMEDIATE", P_immediate},
    {(char *)"1[", P_lbrack},
    {(char *)"0]", P_rbrack},
    {(char *)"0CREATE", P_create},
    {(char *)"0FORGET", P_forget},
    {(char *)"0DOES>", P_does},
    {(char *)"0'", P_tick},
    {(char *)"1[']", P_bracktick},
    {(char *)"0EXECUTE", P_execute},
    {(char *)"0>BODY", P_body},
    {(char *)"0STATE", P_state},

#ifdef DEFFIELDS
    {(char *)"0FIND", P_find},
    {(char *)"0>NAME", P_toname},
    {(char *)"0>LINK", P_tolink},
    {(char *)"0BODY>", P_frombody},
    {(char *)"0NAME>", P_fromname},
    {(char *)"0LINK>", P_fromlink},
    {(char *)"0N>LINK", P_nametolink},
    {(char *)"0L>NAME", P_linktoname},
    {(char *)"0NAME>S!", P_fetchname},
    {(char *)"0S>NAME!", P_storename},
#endif /* DEFFIELDS */

#ifdef COMPILERW
    {(char *)"1[COMPILE]", P_brackcompile},
    {(char *)"1LITERAL", P_literal},
    {(char *)"0COMPILE", P_compile},
    {(char *)"0<MARK", P_backmark},
    {(char *)"0<RESOLVE", P_backresolve},
    {(char *)"0>MARK", P_fwdmark},
    {(char *)"0>RESOLVE", P_fwdresolve},
#endif /* COMPILERW */

#ifdef CONIO
    {(char *)"0GET-CONSOLE", ATH_getConsole },
    {(char *)"0SET-CONSOLE", ATH_setConsole },
    
    {(char *)"0(KEY)", ATH_brKey},
    {(char *)"0KEY", ATH_key},
    
    {(char *)"0(?KEY)", ATH_brQkey},
    {(char *)"0?KEY", ATH_qkey},
    
    {(char *)"0(?EMIT)", ATH_brQemit},
    {(char *)"0?EMIT", ATH_qemit},
    
    {(char *)"0(EMIT)", ATH_brEmit},
    {(char *)"0EMIT", ATH_emit},
    
    {(char *)"0HEX", ATH_hex},
    {(char *)"0DECIMAL", ATH_dec},
    {(char *)"0.", P_dot},
    {(char *)"0?", P_question},
    {(char *)"0CR", P_cr},
    {(char *)"0.S", P_dots},
    {(char *)"1.\"", P_dotquote},
    {(char *)"1.(", P_dotparen},
    {(char *)"0TYPE", P_type},
    {(char *)"0WORDS", P_words},
#endif /* CONIO */

#ifdef FILEIO
    {(char *)"0FILE", P_file},
    {(char *)"0FOPEN", P_fopen},
    {(char *)"0FCLOSE", P_fclose},
    {(char *)"0FDELETE", P_fdelete},
    {(char *)"0FGETS", P_fgetline},
    {(char *)"0FPUTS", P_fputline},
    {(char *)"0FREAD", P_fread},
    {(char *)"0FWRITE", P_fwrite},
    {(char *)"0FGETC", P_fgetc},
    {(char *)"0FPUTC", P_fputc},
    {(char *)"0FTELL", P_ftell},
    {(char *)"0FSEEK", P_fseek},
    {(char *)"0FLOAD", P_fload},
#endif /* FILEIO */

#ifdef EVALUATE
    {(char *)"0EVALUATE", P_evaluate},
#endif /* EVALUATE */

#ifdef ATH
    {(char *)"0MEMSAFE",ATH_memsafe},
    {(char *)"0?MEMSAFE",ATH_qmemsafe},
    {(char *)"0TEST", ATH_test},
    {(char *)"0OBJDUMP", ATH_objdump},
    {(char *)"0DUMP", ATH_dump},
    {(char *)"0ERASE", ATH_erase},
    {(char *)"0FILL", ATH_fill},
    {(char *)"0.FEATURES", ATH_Features},
    //    {(char *)"0DEFER", ATH_defer},
    {(char *)"0BYE", ATH_bye},
    {(char *)"0W!", ATH_wbang},
    {(char *)"0W@", ATH_wat },
    {(char *)"0?REAL", ATH_realq },
    {(char *)"0?MATH", ATH_mathq },
    {(char *)"0?FILEIO", ATH_fileioq },

#endif
    {NULL, (codeptr) 0}
};

/*  ATL_PRIMDEF  --  Initialise the dictionary with the built-in primitive
    words.  To save the memory overhead of separately
    allocated word items, we get one buffer for all
    the items and link them internally within the buffer. */

// Exported void atl_primdef(pt)
Exported void atl_primdef( struct primfcn *pt) {
    struct primfcn *pf = pt;
    dictword *nw;
    int i, n = 0;
#ifdef WORDSUSED
#ifdef READONLYSTRINGS
    unsigned int nltotal;
    char *dynames, *cp;
#endif /* READONLYSTRINGS */
#endif /* WORDSUSED */

    /* Count the number of definitions in the table. */

    while (pf->pname != NULL) {
        n++;
        pf++;
    }

#ifdef WORDSUSED
#ifdef READONLYSTRINGS
    nltotal = n;
    for (i = 0; i < n; i++) {
        nltotal += strlen(pt[i].pname);
    }
    cp = dynames = alloc(nltotal);
    for (i = 0; i < n; i++) {
        strcpy(cp, pt[i].pname);
        cp += strlen(cp) + 1;
    }
    cp = dynames;
#endif /* READONLYSTRINGS */
#endif /* WORDSUSED */

    nw = (dictword *) alloc((unsigned int) (n * sizeof(dictword)));

    nw[n - 1].wnext = dict;
    dict = nw;
    for (i = 0; i < n; i++) {
        nw->wname = pt->pname;
#ifdef WORDSUSED
#ifdef READONLYSTRINGS
        nw->wname = cp;
        cp += strlen(cp) + 1;
#endif /* READONLYSTRINGS */
#endif /* WORDSUSED */
        nw->wcode = pt->pcode;
        if (i != (n - 1)) {
            nw->wnext = nw + 1;
        }
        nw++;
        pt++;
    }
}

#ifdef WALKBACK

/*  PWALKBACK  --  Print walkback trace.  */

static void pwalkback() {
    char buffer[80];
    
    if (atl_walkback && ((curword != NULL) || (wbptr > wback))) {
#ifdef EMBEDDED
        sprintf(buffer,"Walkback:\n"); // EMBEDDED
        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
        V printf("Walkback:\n"); // NOT EMBEDDED
#endif

        if (curword != NULL) {
#ifdef EMBEDDED
            sprintf(buffer,"   %s\n", curword->wname + 1); // EMBEDDED
            sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
            V printf("   %s\n", curword->wname + 1); // NOT EMBEDDED
#endif
        }
        while (wbptr > wback) {
            dictword *wb = *(--wbptr);
#ifdef EMBEDDED
            sprintf(buffer,"   %s\n", wb->wname + 1); // EMBEDDED
            sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
            V printf("   %s\n", wb->wname + 1); // NOT EMBEDDED
#endif
        }
    }
}
#endif /* WALKBACK */

/*  TROUBLE  --  Common handler for serious errors.  */

static void trouble( char *kind) {
    char buffer[80];
#ifdef MEMMESSAGE
#ifdef EMBEDDED
    sprintf(buffer,"\n%s.\n", kind); // EMBEDDED
    sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
    V printf("\n%s.\n", kind); // NOT EMBEDDED
#endif
#endif
#ifdef WALKBACK
    pwalkback();
#endif /* WALKBACK */

    P_abort();			      /* Abort */
    atl_comment = state = Falsity;    /* Reset all interpretation state */
    forgetpend = defpend = stringlit =
        tickpend = ctickpend = False;
}

/*  ATL_ERROR  --  Handle error detected by user-defined primitive.  */

Exported void atl_error( char *kind) {
    trouble(kind);
    evalstat = ATL_APPLICATION;       /* Signify application-detected error */
}

#ifndef NOMEMCHECK

/*  STAKOVER  --  Recover from stack overflow.	*/

Exported void stakover()
{
    trouble((char *)"Stack overflow");
    evalstat = ATL_STACKOVER;
}

/*  STAKUNDER  --  Recover from stack underflow.  */

Exported void stakunder()
{
    trouble((char *)"Stack underflow");
    evalstat = ATL_STACKUNDER;
}

/*  RSTAKOVER  --  Recover from return stack overflow.	*/

Exported void rstakover()
{
    trouble((char *)"Return stack overflow");
    evalstat = ATL_RSTACKOVER;
}

/*  RSTAKUNDER	--  Recover from return stack underflow.  */

Exported void rstakunder()
{
    trouble((char *)"Return stack underflow");
    evalstat = ATL_RSTACKUNDER;
}

/*  HEAPOVER  --  Recover from heap overflow.  Note that a heap
    overflow does NOT wipe the heap; it's up to
    the user to do this manually with FORGET or
    some such. */

Exported void heapover()
{
    trouble((char *)"Heap overflow");
    evalstat = ATL_HEAPOVER;
}

/*  BADPOINTER	--  Abort if bad pointer reference detected.  */

Exported void badpointer()
{
    trouble((char *)"Bad pointer");
    evalstat = ATL_BADPOINTER;
}

/*  NOTCOMP  --  Compiler word used outside definition.  */

static void notcomp()
{
    trouble((char *)"Compiler word outside definition");
    evalstat = ATL_NOTINDEF;
}

/*  DIVZERO  --  Attempt to divide by zero.  */

static void divzero()
{
    trouble((char *)"Divide by zero");
    evalstat = ATL_DIVZERO;
}

#endif /* !NOMEMCHECK */

/*  EXWORD  --	Execute a word (and any sub-words it may invoke). */

static void exword( dictword *wp) {
    curword = wp;
    char buffer[80];
#ifdef TRACE
    if (atl_trace) {
#ifdef EMBEDDED
        sprintf(buffer,"\nTrace: %s ", curword->wname + 1); //  EMBEDDED
        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
        V printf("\nTrace: %s ", curword->wname + 1); // NOT EMBEDDED
#endif
    }
#endif /* TRACE */
    (*curword->wcode)();	      /* Execute the first word */
    while (ip != NULL) {
#ifdef BREAK
#ifdef Keybreak
        Keybreak();		      /* Poll for asynchronous interrupt */
#endif
        if (broken) {		      /* Did we receive a break signal */
            trouble((char *)"Break signal");
            evalstat = ATL_BREAK;
            break;
        }
#endif /* BREAK */
        curword = *ip++;
#ifdef TRACE
        if (atl_trace) {
#ifdef EMBEDDED
            sprintf(buffer,"\nTrace: %s ", curword->wname + 1); // EMBEDDED
            sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
            V printf("\nTrace: %s ", curword->wname + 1); // NOT EMBEDDED
#endif
        }
#endif /* TRACE */
        (*curword->wcode)();	      /* Execute the next word */
    }
    curword = NULL;
}

/*  ATL_INIT  --  Initialise the ATLAST system.  The dynamic storage areas
    are allocated unless the caller has preallocated buffers
    for them and stored the addresses into the respective
    pointers.  In either case, the storage management
    pointers are initialised to the correct addresses.  If
    the caller preallocates the buffers, it's up to him to
    ensure that the length allocated agrees with the lengths
    given by the atl_... cells.  */

void atl_init() {
    if (dict == NULL) {
        atl_primdef(primt);	      /* Define primitive words */
        dictprot = dict;	      /* Set protected mark in dictionary */

        /* Look up compiler-referenced words in the new dictionary and
           save their compile addresses in static variables. */

#define Cconst(cell, name)  cell = (stackitem) lookup(name); if(cell==0)abort()
        Cconst(s_exit, (char *)"EXIT");
        Cconst(s_lit, (char *)"(LIT)");
#ifdef REAL
        Cconst(s_flit, "(FLIT)");
#endif
        Cconst(s_strlit, (char *)"(STRLIT)");
        Cconst(s_dotparen, (char *)".(");
        Cconst(s_qbranch, (char *)"?BRANCH");
        Cconst(s_branch, (char *)"BRANCH");
        Cconst(s_xdo, (char *)"(XDO)");
        Cconst(s_xqdo, (char *)"(X?DO)");
        Cconst(s_xloop, (char *)"(XLOOP)");
        Cconst(s_pxloop, (char *)"(+XLOOP)");
        Cconst(s_abortq, (char *)"ABORT\"");
#undef Cconst

        if (stack == NULL) {	      /* Allocate stack if needed */
            stack = (stackitem *)
                alloc(((unsigned int) atl_stklen) * sizeof(stackitem));
        }
        stk = stackbot = stack;
#ifdef MEMSTAT
        stackmax = stack;
#endif
        stacktop = stack + atl_stklen;
        if (rstack == NULL) {	      /* Allocate return stack if needed */
            rstack = (dictword ***)
                alloc(((unsigned int) atl_rstklen) *
                        sizeof(dictword **));
        }
        rstk = rstackbot = rstack;
#ifdef MEMSTAT
        rstackmax = rstack;
#endif
        rstacktop = rstack + atl_rstklen;
#ifdef WALKBACK
        if (wback == NULL) {
            wback = (dictword **) alloc(((unsigned int) atl_rstklen) *
                    sizeof(dictword *));
        }
        wbptr = wback;
#endif
        if (heap == NULL) {

            /* The temporary string buffers are placed at the start of the
               heap, which permits us to pointer-check pointers into them
               as within the heap extents.  Hence, the size of the buffer
               we acquire for the heap is the sum of the heap and temporary
               string requests. */

            int i;
            char *cp;

            /* Force length of temporary strings to even number of
               stackitems. */
            atl_ltempstr += sizeof(stackitem) -
                (atl_ltempstr % sizeof(stackitem));
            cp = alloc((((unsigned int) atl_heaplen) * sizeof(stackitem)) +
                    ((unsigned int) (atl_ntempstr * atl_ltempstr)));
            heapbot = (stackitem *) cp;
            strbuf = (char **) alloc(((unsigned int) atl_ntempstr) *
                    sizeof(char *));
            for (i = 0; i < atl_ntempstr; i++) {
                strbuf[i] = cp;
                cp += ((unsigned int) atl_ltempstr);
            }
            cstrbuf = 0;
            heap = (stackitem *) cp;  /* Allocatable heap starts after
                                         the temporary strings */
        }
        /* The system state word is kept in the first word of the heap
           so that pointer checking doesn't bounce references to it.
           When creating the heap, we preallocate this word and initialise
           the state to the interpretive state. */
        hptr = heap + 1;
        state = Falsity;
#ifdef MEMSTAT
        heapmax = hptr;
#endif
        heaptop = heap + atl_heaplen;

        /* Now that dynamic memory is up and running, allocate constants
           and variables built into the system.  */

#ifdef FILEIO
        {   static struct {
                              char *sfn;
                              FILE *sfd;
                          } stdfiles[] = {
                              {(char *)"STDIN", NULL},
                              {(char *)"STDOUT", NULL},
                              {(char *)"STDERR", NULL}
                          };
        int i;
        dictword *dw;

        /* On some systems stdin, stdout, and stderr aren't
           constants which can appear in an initialisation.
           So, we initialise them at runtime here. */

        stdfiles[0].sfd = stdin;
        stdfiles[1].sfd = stdout;
        stdfiles[2].sfd = stderr;

        for (i = 0; i < ELEMENTS(stdfiles); i++) {
            if ((dw = atl_vardef(stdfiles[i].sfn,
                            2 * sizeof(stackitem))) != NULL) {
                stackitem *si = atl_body(dw);
                *si++ = FileSent;
                *si = (stackitem) stdfiles[i].sfd;
            }
        }
        }
#endif /* FILEIO */
        dictprot = dict;	      /* Protect all standard words */
    }
}

/*  ATL_LOOKUP	--  Look up a word in the dictionary.  Returns its
    word item if found or NULL if the word isn't
    in the dictionary. */

dictword *atl_lookup( char *name) {
    V strcpy(tokbuf, name);	      /* Use built-in token buffer... */
    ucase(tokbuf);                    /* so ucase() doesn't wreck arg string */
    return lookup(tokbuf);	      /* Now use normal lookup() on it */
}

/*  ATL_BODY  --  Returns the address of the body of a word, given
    its dictionary entry. */

stackitem *atl_body( dictword *dw) {
    return ((stackitem *) dw) + Dictwordl;
}

/*  ATL_EXEC  --  Execute a word, given its dictionary address.  The
    evaluation status for that word's execution is
    returned.  The in-progress evaluation status is
    preserved. */

int atl_exec( dictword *dw) {
    int sestat = evalstat, restat;

    evalstat = ATL_SNORM;
#ifdef BREAK
    broken = False;		      /* Reset break received */
#endif
#undef Memerrs
#define Memerrs evalstat
    Rso(1);
    Rpush = ip; 		      /* Push instruction pointer */
    ip = NULL;			      /* Keep exword from running away */
    exword(dw);
    if (evalstat == ATL_SNORM) {      /* If word ran to completion */
        Rsl(1);
        ip = R0;		      /* Pop the return stack */
        Rpop;
    }
#undef Memerrs
#define Memerrs
    restat = evalstat;
    evalstat = sestat;
    return restat;
}

/*  ATL_VARDEF  --  Define a variable word.  Called with the word's
    name and the number of bytes of storage to allocate
    for its body.  All words defined with atl_vardef()
    have the standard variable action of pushing their
    body address on the stack when invoked.  Returns
    the dictionary item for the new word, or NULL if
    the heap overflows. */

dictword *atl_vardef( char *name, int size) {
    dictword *di;
    int isize = (size + (sizeof(stackitem) - 1)) / sizeof(stackitem);

#undef Memerrs
#define Memerrs NULL
    evalstat = ATL_SNORM;
    Ho(Dictwordl + isize);
#undef Memerrs
#define Memerrs
    if (evalstat != ATL_SNORM)	      /* Did the heap overflow */
        return NULL;		      /* Yes.  Return NULL */
    createword = (dictword *) hptr;   /* Develop address of word */
    createword->wcode = P_var;	      /* Store default code */
    hptr += Dictwordl;		      /* Allocate heap space for word */
    while (isize > 0) {
        Hstore = 0;		      /* Allocate heap area and clear it */
        isize--;
    }
    V strcpy(tokbuf, name);	      /* Use built-in token buffer... */
    ucase(tokbuf);                    /* so ucase() doesn't wreck arg string */
    enter(tokbuf);		      /* Make dictionary entry for it */
    di = createword;		      /* Save word address */
    createword = NULL;		      /* Mark no word underway */
    return di;			      /* Return new word */
}

/*  ATL_MARK  --  Mark current state of the system.  */

void atl_mark( atl_statemark *mp) {
    mp->mstack = stk;		      /* Save stack position */
    mp->mheap = hptr;		      /* Save heap allocation marker */
    mp->mrstack = rstk; 	      /* Set return stack pointer */
    mp->mdict = dict;		      /* Save last item in dictionary */
}

/*  ATL_UNWIND	--  Restore system state to previously saved state.  */

void atl_unwind( atl_statemark *mp) {

    /* If atl_mark() was called before the system was initialised, and
       we've initialised since, we cannot unwind.  Just ignore the
       unwind request.	The user must manually atl_init before an
       atl_mark() request is made. */

    if (mp->mdict == NULL)	      /* Was mark made before atl_init ? */
        return; 		      /* Yes.  Cannot unwind past init */

    stk = mp->mstack;		      /* Roll back stack allocation */
    hptr = mp->mheap;		      /* Reset heap state */
    rstk = mp->mrstack; 	      /* Reset the return stack */

    /* To unwind the dictionary, we can't just reset the pointer,
       we must walk back through the chain and release all the name
       buffers attached to the items allocated after the mark was
       made. */

    while (dict != NULL && dict != dictprot && dict != mp->mdict) {
        free(dict->wname);	      /* Release name string for item */
        dict = dict->wnext;	      /* Link to previous item */
    }
}

#ifdef BREAK

/*  ATL_BREAK  --  Asynchronously interrupt execution.	Note that this
    function only sets a flag, broken, that causes
    exword() to halt after the current word.  Since
    this can be called at any time, it daren't touch the
    system state directly, as it may be in an unstable
    condition. */

void atl_break()
{
    broken = True;		      /* Set break request */
}
#endif /* BREAK */

/*  ATL_LOAD  --  Load a file into the system.	*/

int atl_load( FILE *fp) {
    char buffer[132];
    int es = ATL_SNORM;
    char s[134];
    atl_statemark mk;
    atl_int scomm = atl_comment;      /* Stack comment pending state */
    dictword **sip = ip;	      /* Stack instruction pointer */
    char *sinstr = instream;	      /* Stack input stream */
    int lineno = 0;		      /* Current line number */

    atl_errline = 0;		      /* Reset line number of error */
    atl_mark(&mk);
    ip = NULL;			      /* Fool atl_eval into interp state */
    while (atl_fgetsp(s, 132, fp) != NULL) {
        lineno++;
        if ((es = atl_eval(s)) != ATL_SNORM) {
            atl_errline = lineno;     /* Save line number of error */
            atl_unwind(&mk);
            break;
        }
    }
    /* If there were no other errors, check for a runaway comment.  If
       we ended the file in comment-ignore mode, set the runaway comment
       error status and unwind the file.  */
    if ((es == ATL_SNORM) && (atl_comment == Truth)) {
#ifdef MEMMESSAGE
#ifdef EMBEDDED
        sprintf(buffer,"\nRunaway `(' comment.\n"); // EMBEDDED
        sysConsole->writePipe((void *)buffer, strlen((char *)buffer));
#else
        V printf("\nRunaway `(' comment.\n"); // NOT EMBEDDED
#endif
#endif
        es = ATL_RUNCOMM;
        atl_unwind(&mk);
    }
    atl_comment = scomm;	      /* Unstack comment pending status */
    ip = sip;			      /* Unstack instruction pointer */
    instream = sinstr;		      /* Unstack input stream */
    return es;
}

/*  ATL_PROLOGUE  --  Recognise and process prologue statement.
    Returns 1 if the statement was part of the
    prologue and 0 otherwise. */

int atl_prologue( char *sp) {
    static struct {
        char *pname;
        atl_int *pparam;
    } proname[] = {
        {(char *)"STACK ", &atl_stklen},
        {(char *)"RSTACK ", &atl_rstklen},
        {(char *)"HEAP ", &atl_heaplen},
        {(char *)"TEMPSTRL ", &atl_ltempstr},
        {(char *)"TEMPSTRN ", &atl_ntempstr}
    };

    if (strncmp(sp, "\\ *", 3) == 0) {
        unsigned int i;
        char *vp = sp + 3, *ap;

        ucase(vp);
        for (i = 0; i < ELEMENTS(proname); i++) {
            if (strncmp(sp + 3, proname[i].pname,
                        strlen(proname[i].pname)) == 0) {
                if ((ap = strchr(sp + 3, ' ')) != NULL) {
                    V sscanf(ap + 1, "%li", proname[i].pparam);
#ifdef PROLOGUEDEBUG
#ifdef EMBEDDED
                    sprintf(outBuffer,"Prologue set %sto %ld\n", proname[i].pname, *proname[i].pparam); // EMBEDDED
#else
                    V printf("Prologue set %sto %ld\n", proname[i].pname, *proname[i].pparam); // NOT EMBEDDED
#endif
#endif
                    return 1;
                }
            }
        }
    }
    return 0;
}

/*  ATL_EVAL  --  Evaluate a string containing ATLAST words.  */

int atl_eval(char *sp) {
    int i;

#undef Memerrs
#define Memerrs evalstat
    instream = sp;
    evalstat = ATL_SNORM;	      /* Set normal evaluation status */
#ifdef BREAK
    broken = False;		      /* Reset asynchronous break */
#endif

    /* If automatic prologue processing is configured and we haven't yet
       initialised, check if this is a prologue statement.	If so, execute
       it.	Otherwise automatically initialise with the memory specifications
       currently operative. */

#ifdef PROLOGUE
    if (dict == NULL) {
        if (atl_prologue(sp))
            return evalstat;
        atl_init();
    }
#endif /* PROLOGUE */

    char buffer[80];
    while ((evalstat == ATL_SNORM) && (i = token(&instream)) != TokNull) {
        dictword *di;

        switch (i) {
            case TokWord:
                if (forgetpend) {
                    forgetpend = False;
                    ucase(tokbuf);
                    if ((di = lookup(tokbuf)) != NULL) {
                        dictword *dw = dict;

                        /* Pass 1.  Rip through the dictionary to make sure
                           this word is not past the marker that
                           guards against forgetting too much. */

                        while (dw != NULL) {
                            if (dw == dictprot) {
#ifdef MEMMESSAGE
#ifdef EMBEDDED
                                sprintf(buffer,"\nForget protected.\n"); //  EMBEDDED
                                sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                                V printf("\nForget protected.\n"); // NOT EMBEDDED
#endif
#endif
                                evalstat = ATL_FORGETPROT;
                                di = NULL;
                            }
                            if (strcmp(dw->wname + 1, tokbuf) == 0)
                                break;
                            dw = dw->wnext;
                        }

                        /* Pass 2.  Walk back through the dictionary
                           items until we encounter the target
                           of the FORGET.  Release each item's
                           name buffer and dechain it from the
                           dictionary list. */

                        if (di != NULL) {
                            do {
                                dw = dict;
                                if (dw->wname != NULL)
                                    free(dw->wname);
                                dict = dw->wnext;
                            } while (dw != di);
                            /* Finally, back the heap allocation pointer
                               up to the start of the last item forgotten. */
                            hptr = (stackitem *) di;
                            /* Uhhhh, just one more thing.  If this word
                               was defined with DOES>, there's a link to
                               the method address hidden before its
                               wnext field.  See if it's a DOES> by testing
                               the wcode field for P_dodoes and, if so,
                               back up the heap one more item. */
                            if (di->wcode == (codeptr) P_dodoes) {
#ifdef FORGETDEBUG
#ifdef EMBEDDED
                                sprintf(outBuffer," Forgetting DOES> word. "); //  EMBEDDED
#else
                                V printf(" Forgetting DOES> word. "); // NOT EMBEDDED
#endif
#endif
                                hptr--;
                            }
                        }
                    } else {
#ifdef MEMMESSAGE
#ifdef EMBEDDED
                        char buffer[80];
                        sprintf(buffer," '%s' undefined ", tokbuf); // EMBEDDED
                        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                        V printf(" '%s' undefined ", tokbuf); // NOT EMBEDDED
#endif
#endif
                        evalstat = ATL_UNDEFINED;
                    }
                } else if (tickpend) {
                    tickpend = False;
                    ucase(tokbuf);
                    if ((di = lookup(tokbuf)) != NULL) {
                        So(1);
                        Push = (stackitem) di; /* Push word compile address */
                    } else {
#ifdef MEMMESSAGE
#ifdef EMBEDDED
                        char buffer[80];
                        sprintf(buffer," '%s' undefined ", tokbuf); // EMBEDDED
                        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                        V printf(" '%s' undefined ", tokbuf); // NOT EMBEDDED
#endif
#endif
                        evalstat = ATL_UNDEFINED;
                    }
                } else if (defpend) {
                    /* If a definition is pending, define the token and
                       leave the address of the new word item created for
                       it on the return stack. */
                    defpend = False;
                    ucase(tokbuf);
                    if (atl_redef && (lookup(tokbuf) != NULL)) {
#ifdef EMBEDDED
                        char buffer[80];

                        sprintf(buffer,"\n%s isn't unique.", tokbuf); // NOT EMBEDDED
                        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                        V printf("\n%s isn't unique.", tokbuf); // NOT EMBEDDED
#endif
                    }

                    enter(tokbuf);
                } else {
                    di = lookup(tokbuf);
                    if (di != NULL) {
                        /* Test the state.  If we're interpreting, execute
                           the word in all cases.  If we're compiling,
                           compile the word unless it is a compiler word
                           flagged for immediate execution by the
                           presence of a space as the first character of
                           its name in the dictionary entry. */
                        if (state &&
                                (cbrackpend || ctickpend ||
                                 !(di->wname[0] & IMMEDIATE))) {
                            if (ctickpend) {
                                /* If a compile-time tick preceded this
                                   word, compile a (lit) word to cause its
                                   address to be pushed at execution time. */
                                Ho(1);
                                Hstore = s_lit;
                                ctickpend = False;
                            }
                            cbrackpend = False;
                            Ho(1);	  /* Reserve stack space */
                            Hstore = (stackitem) di;/* Compile word address */
                        } else {
                            exword(di);   /* Execute word */
                        }
                    } else {
#ifdef MEMMESSAGE
#ifdef EMBEDDED
                        char buffer[80];
                        sprintf(buffer," '%s' undefined ", tokbuf); // EMBEDDED
                        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                        V printf(" '%s' undefined ", tokbuf); // NOT EMBEDDED
#endif
#endif
                        evalstat = ATL_UNDEFINED;
                        state = Falsity;
                    }
                }
                break;

            case TokInt:
                if (state) {
                    Ho(2);
                    Hstore = s_lit;   /* Push (lit) */
                    Hstore = tokint;  /* Compile actual literal */
                } else {
                    So(1);
                    Push = tokint;
                }
                break;

#ifdef REAL
            case TokReal:
                if (state) {
                    int i;
                    union {
                        atl_real r;
                        stackitem s[Realsize];
                    } tru;

                    Ho(Realsize + 1);
                    Hstore = s_flit;  /* Push (flit) */
                    tru.r = tokreal;
                    for (i = 0; i < Realsize; i++) {
                        Hstore = tru.s[i];
                    }
                } else {
                    int i;
                    union {
                        atl_real r;
                        stackitem s[Realsize];
                    } tru;

                    So(Realsize);
                    tru.r = tokreal;
                    for (i = 0; i < Realsize; i++) {
                        Push = tru.s[i];
                    }
                }
                break;
#endif /* REAL */

#ifdef STRING
            case TokString:
                if (stringlit) {
                    stringlit = False;
                    if (state) {
                        int l = (strlen(tokbuf) + 1 + sizeof(stackitem)) /
                            sizeof(stackitem);
                        Ho(l);
                        *((char *) hptr) = l;  /* Store in-line skip length */
                        V strcpy(((char *) hptr) + 1, tokbuf);
                        hptr += l;
                    } else {
#ifdef EMBEDDED
                        char buffer[80];
                        sprintf(buffer,"%s", tokbuf); // EMBEDDED
                        sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                        V printf("%s", tokbuf); // NOT EMBEDDED
#endif
                    }
                } else {
                    if (state) {
                        int l = (strlen(tokbuf) + 1 + sizeof(stackitem)) /
                            sizeof(stackitem);
                        Ho(l + 1);
                        /* Compile string literal instruction, followed by
                           in-line skip length and the string literal */
                        Hstore = s_strlit;
                        *((char *) hptr) = l;  /* Store in-line skip length */
                        V strcpy(((char *) hptr) + 1, tokbuf);
                        hptr += l;
                    } else {
                        So(1);
                        V strcpy(strbuf[cstrbuf], tokbuf);
                        Push = (stackitem) strbuf[cstrbuf];
                        cstrbuf = (cstrbuf + 1) % ((int) atl_ntempstr);
                    }
                }
                break;
#endif /* STRING */
            default:
#ifdef EMBEDDED
                sprintf(buffer,"\nUnknown token type %d\n", i); // EMBEDDED
                sysConsole->writePipe((void *)buffer,strlen((char *)buffer));
#else
                V printf("\nUnknown token type %d\n", i); // NOT EMBEDDED
#endif
                break;
        }
    }
    return evalstat;
}
