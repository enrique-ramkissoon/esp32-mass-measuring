#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include "main_util.h"
#include "queue.h"

enum diagnostic_tasks {NONE, TEXT, ADC, STATE, STATS, COMMAND, NETWORK};

int task_manager(struct Data_Queues* data_queues);

int text_task_stdout_redirect(void* c,const char* data,int size);

struct adc_args
{
    char* payload;
    int payload_size;
    QueueHandle_t* adc_queue;

    enum diagnostic_tasks* active_task;
};

struct text_args
{
    char* payload;
    int payload_size;
    QueueHandle_t* text_queue;
    bool* ack;

    enum diagnostic_tasks* active_task;
};

#endif
