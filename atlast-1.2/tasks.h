#ifdef LINUX
#include <mqueue.h>
#endif

#ifndef TASKS_H_
#define TASKS_H_

#define MAX_CMD 8       // Length of longest command i.e. UNSUB
#define MAX_VALUE 32    // Length of longest value.
#define MAX_KEY   16    // Length of longest key.
#define SENDER_SIZE 32

struct payload {
    uint8_t fields; // <= 3
    char cmd[MAX_CMD];
    char key[MAX_KEY];
    char value[MAX_VALUE];
};

#ifdef FREERTOS
struct cmdMessage {
    QueueHandle_t sender;
    struct payload message;
};
#endif

#ifdef LINUX
struct cmdMessage {
    char sender[32];
    struct payload message;
};
#endif

#ifdef LINUX
struct client {
    char name[32];
    mqd_t pipe;
};
#else
struct client {
    QueueHandle_t pipe;
};
#endif

#endif
