#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <message.h>
#include <Console.h>
#include "atldef.h"
#include "atlast.h"
#include "atlcfig.h"


int main() {
    char t[132];
    char prompt[] = "C++ -> ";
    
    Message *sysConsole;
    
    sysConsole = new Console();
    
    sysConsole->ioctl(IOCTL_EOL, true);

    atl_init();
    while(1) {
        (void)memset(outBuffer,0,sizeof(outBuffer));
        
        sysConsole->writePipe((char *)prompt,sizeof(prompt));

        sysConsole->readPipe((char *)t, sizeof(t));
        atl_eval(t);

        if(strlen(outBuffer) > 0) {
            printf("%s\n", outBuffer);
            outBuffer[0]='\0';
        }
    }
}
