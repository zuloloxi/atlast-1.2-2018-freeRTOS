#include <stdio.h>
#include <stdlib.h>
#include "atldef.h"
#ifdef PUBSUB
#include "msgs.h"
#endif

// Move this into atlast once updated frm FreeRTOS version
//

extern P_here();
extern P_swap();
extern P_allot();

#ifdef PUBSUB
void mkMsg(void *from, struct cmdMessage *msg, char *cmd, char *key, char *value) {

    memset(msg, 0, sizeof(struct cmdMessage));
    msg->payload.message.fields=3;

    strncpy(msg->sender,(char *)S1, SENDER_SIZE);

    strncpy(msg->payload.message.cmd,cmd,sizeof(msg->payload.message.cmd));
    if( value == NULL) {
        msg->payload.message.fields=2;
        msg->payload.message.value[0]='\0';
    } else {
        strncpy(msg->payload.message.value, value, sizeof(msg->payload.message.value));
    }
    if( key == NULL) {
        msg->payload.message.fields=1;
        msg->payload.message.key[0]='\0';
        msg->payload.message.value[0]='\0';
    } else {
        strncpy(msg->payload.message.key, key, sizeof(msg->payload.message.key));
    }

}
#endif

prim ATH_initRamBlocks() {
    int size;
    Sl(1);
    So(2);

    P_here();
    P_swap();

    size=S0 * 1024;
    S0=size;

    P_allot();

    memset((void *)S0, ' ', size);
}

prim crap() {
    printf("Hello\n");
}

// <ptr> name -- ptr
prim ATH_getenv() {
    Sl(2); // On entry will use this many.
    So(1); // on exit will leave this many.

    char *name=S0;
    char *ptr=S1;
    char *tmp;

    Pop2;

    tmp = getenv(name);
    if(!tmp) {
        Push=-1;
    } else {
        strcpy(ptr, tmp);
        Push=ptr;
        Push=0;
    }
}


static struct primfcn extras[] = {
    {"0INIT-RAM", ATH_initRamBlocks},
    {"0GETENV", ATH_getenv},
    {"0TESTING", crap},
    {NULL, (codeptr) 0}
};

void extrasLoad() {
    atl_primdef( extras );
}
