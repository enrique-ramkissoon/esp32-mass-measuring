#ifndef MAIN_UTIL_H
#define MAIN_UTIL_H

#include "queue.h"

#define DOUT_PIN GPIO_NUM_27

struct Data_Queues
{
    QueueHandle_t* adc_out_queue;
};

#endif
