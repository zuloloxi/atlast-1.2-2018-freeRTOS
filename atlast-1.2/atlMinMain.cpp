#include <stdio.h>
#include <string.h>
#include <stdint.h>
// #include <message.h>
#include "Console.h"
#include "atldef.h"
#include "atlast.h"
#include "atlcfig.h"

// Global so that atlast.c can see it.
Console *sysConsole;

int main() {
    char t[132];
    char prompt[] = "C++ -> ";
    
    
    sysConsole = new Console();
    
    sysConsole->ioctl(IOCTL_EOL, true);
    sysConsole->ioctl(IOCTL_EDIT,true);

    atl_init();
    while(1) {
        sysConsole->writePipe((char *)prompt,sizeof(prompt));
        sysConsole->readPipe((char *)t, sizeof(t));
        atl_eval(t);
    }
}
