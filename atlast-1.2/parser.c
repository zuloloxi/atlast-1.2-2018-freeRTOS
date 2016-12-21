/*
 * parser.c
 *
 *  Created on: 7 Dec 2016
 *      Author: andrew.holt
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef LINUX
#include "cmsis_os.h"
extern osPoolId mpool_id;
#endif

#include "tasks.h"
#include "Small.h"


char *cmdParse(struct Small *db, struct cmdMessage *msg,bool publish) {

	char *res=NULL;
	bool failFlag=true;
	int len=0;
	bool freeMemory=true;  // if this is set to true return the block via osPoolFree.
#ifdef LINUX
	int rc=0;
    struct client from;
    struct client *c;
//    char from[SENDER_SIZE];
#else
    // TODO These are volatile for debugging purposes, remove.
	osStatus rc;
	QueueHandle_t *from;
	QueueHandle_t *c;
	from = msg->sender;
#endif

#ifdef LINUX
    strncpy(from.name,msg->sender,SENDER_SIZE);
#endif
	char *cmd   = msg->message.cmd;
	char *name  = msg->message.key;
	char *value = msg->message.value;

	if(!strcmp(cmd,"SET")) {
		failFlag = addRecord(db,name,value);
		if(publish) {
			dbPublish(db,name);
		}
	} else if(!strcmp(cmd,"GET")) {
		freeMemory=false;
		res=dbLookup(db,name);

		if(res == NULL ) {
			strcpy(msg->message.value,"NODATA");
		} else {
			strncpy(msg->message.value,res,sizeof(msg->message.value));
		}
		msg->message.fields = 3;
#ifdef LINUX
        // Don'y want a reply, so make this message anonymous
        memset(&(msg->sender),0,SENDER_SIZE);
#endif
        strncpy(cmd, "SET", sizeof(cmd));
        strncpy(value, res, sizeof(value));
		msg->message.fields = 3;

#ifdef LINUX
        printf("%s=%s\n", name,res);

        mqd_t outMq = mq_open(from.name,O_WRONLY);
        if( outMq == (mqd_t)-1) {
            perror("COMMS mq_open reply");
        } else {
            rc = mq_send(outMq,msg,sizeof(struct cmdMessage),NULL);
            mq_close(outMq);
        }

#else
		rc=osMessagePut((QueueHandle_t *) from, (uint32_t )msg, osWaitForever);
#endif
	} else if(!strcmp(cmd,"SUB")) {
#ifdef LINUX

		res=dbLookup(db,name);

        if(res) {
            c=calloc(1,sizeof(struct client));
            if(c == NULL) {
                perror("calloc");
            } else {
                strncpy(c->name,from.name,SENDER_SIZE);
                c->pipe=-1;
            }
        }
        /*
        len=SENDER_SIZE;
		memcpy( from, msg->sender, len);
        */
#else
//		from = msg->sender;
		c = msg->sender;
#endif

		dbSubscribe(db, c,name);
		// dbSubscribe(db, (void *)from,name);
	} else if(!strcmp(cmd,"UNSUB")) {
#ifdef LINUX
        len=SENDER_SIZE;
//		memcpy( from, msg->sender, len);
#else
        /*
		len = sizeof(QueueHandle_t);
		from = (QueueHandle_t *)malloc( len );
		*/
//		from = msg->sender;
		c = msg->sender;
#endif
		dbUnsubscribe(db, (void *)c,name);
	}

#ifndef LINUX
	if(freeMemory) {
		rc=osPoolFree(mpool_id, msg);
	}
#endif
	return res;
}


