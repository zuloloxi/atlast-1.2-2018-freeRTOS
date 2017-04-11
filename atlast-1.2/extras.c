#include <stdio.h>
#include "atldef.h"

prim crap() {
    printf("Hello\n");
}


static struct primfcn extras[] = {
    {"0TESTING", crap},
    {NULL, (codeptr) 0}
};

void extrasLoad() {
    atl_primdef( extras );
}
