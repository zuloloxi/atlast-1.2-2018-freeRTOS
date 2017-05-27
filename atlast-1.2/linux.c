#include <stdio.h>
#include <stdlib.h>
#include "atldef.h"

prim crap() {
    printf("Hello\n");
}

prim ATH_getenv() {
    S0 = getenv(S0);
}


static struct primfcn extras[] = {
    {"0GETENV", ATH_getenv},
    {"0TESTING", crap},
    {NULL, (codeptr) 0}
};

void extrasLoad() {
    atl_primdef( extras );
}
