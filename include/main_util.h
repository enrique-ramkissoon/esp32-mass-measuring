#ifndef MAIN_UTIL_H
#define MAIN_UTIL_H

#include "queue.h"
#include "ble_server.h"

struct Data_Queues
{
    enum diagnostic_tasks* active_task;
    QueueHandle_t* adc_out_queue;
};

#endif
