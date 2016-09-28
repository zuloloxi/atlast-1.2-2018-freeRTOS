#ifndef __CONSOLE_CLASS
#define __CONSOLE_CLASS
#include <fcntl.h>
#include <sys/time.h>
#include <termios.h>

// #include "message.h"


// Errors:
// Return this if method is unimplemented.
//
#define OK 0x00
#define UNIMPLIMENTED (uint8_t) 0xff
#define ACCESS_VIOLATION (uint8_t) 0xfe
//
//
#define WRITE_ONLY 0222
#define READ_ONLY 0444
#define READ_WRITE 0666
// 
// ioctl functions
//
#define IOCTL_OK (uint8_t) 0x80     // All's good.
#define SET_TIMEOUT (uint8_t) 0x81 

#define NB_DISABLE 1
#define NB_ENABLE 0

enum IOCTL { 
    IOCTL_EOL,
    IOCTL_EDIT
};

class Console {
private:
    void nonblock(int state);
    bool kbhit();
    
    
protected:
    int waitTime;
    int access;
    bool verbose;
    void *qid;
    bool lineEndings;    // True if readPipe returns on line endings.
    bool lineEdit;      // If true certain characters (^h, del) will be used to edit a line
    
public:
    Console() ;
    virtual bool qemit() ;
    virtual uint8_t emit(uint8_t) ;
    
    virtual bool qkey() ;
    virtual uint8_t key() ;
    
    virtual int readPipe(void *,int);
    virtual int writePipe( void *, int);
    virtual int ioctl(uint8_t, int);
    
    virtual void setDebug() ;
    virtual void clrDebug() ;
    
    virtual void setWaitTime(int timeOut) ;
    virtual void setAccess(mode_t) ;
    virtual void dump() ;
    
    virtual ~Console() {} ;
};

#endif
