#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include "main_util.h"
#include "queue.h"

int task_manager(struct Data_Queues data_queues);

struct adc_args
{
    char* payload;
    int payload_size;
    QueueHandle_t* adc_queue;
};

#endif
