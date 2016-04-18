#include <stdio.h>
#include <string.h>
#include "atlast.h"
#include "atlcfig.h"

int main() {
    char t[132];

    atl_init();
    while(1) {
        (void)memset(outBuffer,0,sizeof(outBuffer));
        printf("-> ");

        (void)fgets(t,132,stdin);
        atl_eval(t);

        if(strlen(outBuffer) > 0) {
            printf("%s\n", outBuffer);
            outBuffer[0]='\0';
        }
    }
}
