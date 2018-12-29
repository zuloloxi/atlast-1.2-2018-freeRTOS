<<<<<<< HEAD:atlast-1.2/atlcfig.h

#define INDIVIDUALLY

#define ARRAY                 /* Array subscripting words */
#define BREAK                 /* Asynchronous break facility */
#define COMPILERW             /* Compiler-writing words */
#define CONIO                 /* Interactive console I/O */
#define DEFFIELDS             /* Definition field access for words */
#define DOUBLE                /* Double word primitives (2DUP) */
#define EVALUATE              /* The EVALUATE primitive */
// #define FILEIO                /* File I/O primitives */
// #define MATH                  /* Math functions */
#define MEMMESSAGE            /* Print message for stack/heap errors */
#define PROLOGUE              /* Prologue processing and auto-init */
// TODO Not defining real breaks build
// #define REAL                  /* Floating point numbers */
#define SHORTCUTA             /* Shortcut integer arithmetic words */
#define SHORTCUTC             /* Shortcut integer comparison */
#define STRING                /* String functions */
// #define SYSTEM                /* System command function */
#define TRACE                 /* Execution tracing */
#define WALKBACK              /* Walkback trace */
#define WORDSUSED             /* Logging of words used and unused */
// TODO Modify to output to buffer.
// #define MEMSTAT          /* Output memory statistics. //
// 
// Stuff added by me
//
// #define ATH
// 

// #define EMBEDDED            // Mods for use in an embedded system.
                            // anything that results in output to stdout, or stderr goes 
                            // to a global buffer.
                            //

=======
#ifndef __ATLAST_CFIG
#define __ATLAST_CFIG
// TODO Remove all references to EMBEDDED in io words.
// Output to a buffer is the norm.
// #define EMBEDDED            // Mods for use in an embedded system.
                            // anything that results in output to stdout, or stderr goes
                            // to a global buffer.
                            //
// #define FREERTOS
#define INDIVIDUALLY

#define ARRAY                 /* Array subscripting words */
#define BREAK                 /* Asynchronous break facility */
#define COMPILERW             /* Compiler-writing words */
#define CONIO                 /* Interactive console I/O */
#define DEFFIELDS             /* Definition field access for words */
#define DOUBLE                /* Double word primitives (2DUP) */
#define EVALUATE              /* The EVALUATE primitive */
// #define FILEIO                /* File I/O primitives */
// #define MATH                  /* Math functions */
#define MEMMESSAGE            /* Print message for stack/heap errors */
#define MEMSTAT
#define PROLOGUE              /* Prologue processing and auto-init */
// TODO Not defining real breaks build
// #define REAL                  /* Floating point numbers */
#define SHORTCUTA             /* Shortcut integer arithmetic words */
#define SHORTCUTC             /* Shortcut integer comparison */
#define STRING                /* String functions */
// #define SYSTEM                /* System command function */
#define TRACE                 /* Execution tracing */
#define WALKBACK              /* Walkback trace */
#define WORDSUSED             /* Logging of words used and unused */
#define BANNER
#define ATH
#define ANSI                /* Enable ANSI compatability words */
// #define Keyhit
// 
// Stuff added by me
//
// #define ATH
// 


char outBuffer[255];
#endif
>>>>>>> 730efb122a88b088d9387d8c350401c148e1b158:atlast-1.2/freertos-atlcfig.h
