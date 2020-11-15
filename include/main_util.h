#ifndef MAIN_UTIL_H
#define MAIN_UTIL_H

#include "queue.h"

//This struct contains the queues used for bluetooth communication
struct Data_Queues
{
    QueueHandle_t* adc_out_queue;
    QueueHandle_t* logs_queue;
};

#endif
