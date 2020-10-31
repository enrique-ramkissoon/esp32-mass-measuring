#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

#include "queue.h"

void initialize_hx711(QueueHandle_t* adc_queue);
void mass_read_task(void* pvParameters);
int32_t get_adc_out_32();
double get_weight(int32_t result32);
void tare(int iterations);

#endif
