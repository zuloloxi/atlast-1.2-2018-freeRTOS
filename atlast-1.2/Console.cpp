
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>

using namespace std;

// #include "message.h"
#include "Console.h"
//
// See:
// http://stackoverflow.com/questions/318064/how-do-you-declare-an-interface-in-c
//

Console::Console() {
   cout << "Console Message class\n";
}

void Console::nonblock(int state) {
    struct termios ttystate;

    //get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);

    if (state==NB_ENABLE) {
        //turn off canonical mode
        ttystate.c_lflag &= ~ICANON;
        //minimum of number input read.
        ttystate.c_cc[VMIN] = 1;
    } else if (state==NB_DISABLE) {
        //turn on canonical mode
        ttystate.c_lflag |= ICANON;
    }
    //set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

}
bool Console::kbhit() {
    bool rc=false;
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
    rc = (FD_ISSET(STDIN_FILENO, &fds) == 0) ? false : true ;
    return rc;
}

bool Console::qemit() {
    return true;
}

// 
// tx a byte, return an error code.
//
uint8_t Console::emit(uint8_t c) {
    uint8_t st;
    
    if( READ_ONLY == access) {
        st = ACCESS_VIOLATION;
    } else {
        putchar( c );
        st = OK;
    }
}

bool Console::qkey() {
    return kbhit();
}

uint8_t Console::key() {
    int c;
    unsigned int ret=0;
    
    nonblock(NB_ENABLE);
    
    c=fgetc(stdin);
    nonblock(NB_DISABLE);
    
    return (uint8_t)(c&0xff);
}
// 
// msg points to a buffer big enough to accept
// the biggest message i.e. maxMessageSize bytes.
//
int Console::readPipe(void *msg,int len ) {
    int status = 0;
    uint8_t k;
    int i=0;
    
    for( i=0; i< len; i++) {
        k = key();
        
        ((char *)msg)[i] = k;
        
        if( lineEndings & ( (k == '\n') || (k == '\r') )) {
            break;
        }
    }
    ((char *)msg)[i+1] = '\0';
    return status;
}

int Console::writePipe( void *msg, int len ) {
    int status=0;
    char *local;
    int i=0;
    
    local=(char *)msg;
    
    for(i=0; i < len; i++) {
        emit( local[i]);
    }
    fflush(stdout);
    return status;
}

int Console::ioctl(uint8_t cmd , int param ) {
    switch( cmd ) {
        case IOCTL_EOL:
            lineEndings = (bool) param;
            break;
    }
}

void Console::setDebug() {
    verbose=true;
    cout << "Debug set\n";
}

void Console::clrDebug() {
    if(verbose) {
        cout << "Debug clr\n";
    }
    verbose=false;
}

void Console::setWaitTime(int t) {
    waitTime = t;
}

void Console::setAccess(mode_t a) {
    access = a;
}

void Console::dump() {
    
    cout << "Class Dump\n\n";
    cout << "Debug   :" << verbose << "\n";
    
}

