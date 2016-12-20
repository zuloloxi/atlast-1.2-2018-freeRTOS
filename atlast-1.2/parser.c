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
#else
#include "tasks.h"
#endif

#include "Small.h"


char *cmdParse(struct Small *db, struct cmdMessage *msg,bool publish) {
	char *res=NULL;
	bool failFlag=true;
	int len=0;
	bool freeMemory=true;  // if this is set to true return the block via osPoolFree.
#ifdef LINUX
	int rc=0;
    char *from;
//    char from[SENDER_SIZE];
#else
	osStatus rc;
	QueueHandle_t *from;
	from = msg->sender;
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
		strcpy(msg->message.cmd,"SET");

#ifdef LINUX
        printf("%s=%s\n", name,res);

#else
		rc=osMessagePut((QueueHandle_t *) from, (uint32_t )msg, osWaitForever);
#endif
	} else if(!strcmp(cmd,"SUB")) {
#ifdef LINUX
        struct client *temp;
        /*
        len=SENDER_SIZE;
		memcpy( from, msg->sender, len);
        */
        temp=(struct client *)malloc(sizeof(struct client));
        if(!temp) {
            perror("cmdParse");
            exit(2);
        }

        strncpy(temp->name,msg->sender,sizeof(temp->name));
        temp->pipe=(mqd_t) -1;
        from=(void *)temp;
#else
		from = msg->sender;
#endif

		dbSubscribe(db, (void *)from,name);
	} else if(!strcmp(cmd,"UNSUB")) {
#ifdef LINUX
        len=SENDER_SIZE;
		memcpy( from, msg->sender, len);
#else
        /*
		len = sizeof(QueueHandle_t);
		from = (QueueHandle_t *)malloc( len );
		*/
		from = msg->sender;
#endif
		dbUnsubscribe(db, (void *)from,name);
	}

#ifndef LINUX
	if(freeMemory) {
		rc=osPoolFree(mpool_id, msg);
	}
#endif
	return res;
}


