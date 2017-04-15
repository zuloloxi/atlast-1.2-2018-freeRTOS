#include <stdio.h>
#include <mosquitto.h>

#include "atldef.h"

void messageCallback(struct mosquitto *mosq, void *obj,const struct mosquitto_message *message) {
    static char *buffer;
    static bool firstTime=true;

    if(firstTime) {
        firstTime=false;

        atl_eval("here 128 allot");
        buffer=S0;
        Pop;
    }


    printf ("Rx message: %s\n", (char *)message->payload);
    strcpy(buffer,(char *)message->payload);

    Pop;
    Push=(char *)buffer;
    Push=-1;
}

prim mqttInit() {
    Sl(0);
    So(0);

    static bool doneFlag=false;

    if (doneFlag=false) {
        mosquitto_lib_init();
        doneFlag=true;
    }

}

prim mqttNew() {
    Sl(1);
    So(2);

    struct mosquitto *mosq = NULL;

    mosq=mosquitto_new(S0, true, NULL);

    if(!mosq) {
        S0=true;
    } else {
        S0=mosq;
        Push=false;
    }

}

// client topic payload -- bool
prim mqttPublish() {
    struct mosquitto *mosq = NULL;
    char *topic;
    char *payload;

    Sl(3);
    So(1);

    int rc=0;

    payload=S0;
    topic=S1;
    mosq=S2;

    Pop2;

    rc=mosquitto_publish(mosq,NULL,topic,strlen(payload), payload, 0, true);
    if( rc == MOSQ_ERR_SUCCESS) {
        S0=false;
    } else {
        S0=true;
    }

}

prim mqttSubscribe() {
    struct mosquitto *mosq = NULL;
    char *topic;

    int rc=0;

    topic=S0;
    mosq=S1;
    Pop;

    rc = mosquitto_subscribe(mosq,NULL, topic, 0);

    if( rc == MOSQ_ERR_SUCCESS) {
        S0=false;
    } else {
        S0=true;
    }
}

/*
 * id hostname port 
 */
prim mqttClient() {
    Sl(3);
    So(1);

    struct mosquitto *mosq ;
    char *hostname;
    int port;
    int rc;

    port=S0;
    hostname=S1;
    mosq=S2;

    rc = mosquitto_connect(mosq, hostname, port, 0);
    Pop2;

    if( rc == MOSQ_ERR_SUCCESS) {
        mosquitto_message_callback_set (mosq, messageCallback);
        S0=false;
    } else {
        S0=true;
    }

}

// id timeout
prim mqttLoop() {
    int rc;
    struct mosquitto *mosq ;
    int timeout=0;

    timeout=S0;
    mosq=S1;

    Pop2;

    rc = mosquitto_loop(mosq,timeout,1);

    if( rc == MOSQ_ERR_SUCCESS) {
        Push=false;
    } else {
        Push=true;
    }

}


static struct primfcn mqtt[] = {
    {"0MQTT-INIT", mqttInit},
    {"0MQTT-NEW", mqttNew},
    {"0MQTT-CLIENT", mqttClient},
    {"0MQTT-PUB", mqttPublish},
    {"0MQTT-SUB", mqttSubscribe},
    {"0MQTT-LOOP", mqttLoop},
    {NULL, (codeptr) 0}
};

void mqttLoad() {
    atl_primdef( mqtt );
}
