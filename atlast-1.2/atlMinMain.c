#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "atlast.h"
#include "atlcfig.h"
#include "atldef.h"

uint8_t nvramrc[] = ": tst 10 0 do\ni . cr\nloop\n;\n";

int main() {
    char t[132];
    int8_t len;

    uint8_t lineBuffer[MAX_LINE];
    dictword *var;
    int *tst;
    extern dictword *rf;

    atl_init();

    do {
        memset(lineBuffer,0,MAX_LINE);
        len=readLineFromArray(nvramrc,lineBuffer);
        atl_eval(lineBuffer);
    } while(len >= 0);

    var = atl_vardef("TEST",4);

    if(var == NULL) {
        fprintf(stderr,"Vardef failed\n");
    } else {
        *((int *)atl_body(var))=42;
    }

    tst = (int *) atl_body(rf);


    while(*tst) {
        (void)memset(outBuffer,0,sizeof(outBuffer));
        printf("-> ");

        (void)fgets(t,132,stdin);
        atl_eval(t);

    }
    return 0;
}
