#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

#include "queue.h"
#include "main_util.h"

void initialize_hx711(struct Data_Queues* data_queues);
void mass_read_task(void* pvParameters);
int32_t get_adc_out_32();
double get_weight(int32_t result32);
void tare(int iterations);

extern enum diagnostic_tasks active;

#endif
