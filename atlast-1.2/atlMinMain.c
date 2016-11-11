#include <stdio.h>
#include <string.h>
#include "atlast.h"
#include "atlcfig.h"
#include "atldef.h"

int main() {
    char t[132];
    dictword *var;
    int *tst;
    extern dictword *rf;

    atl_init();

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

        if(strlen(outBuffer) > 0) {
            printf("%s\n", outBuffer);
            outBuffer[0]='\0';
        }
    }
    return 0;
}
