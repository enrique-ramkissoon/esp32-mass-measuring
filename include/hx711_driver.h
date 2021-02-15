#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

#include "queue.h"
#include "main_util.h"

void initialize_hx711(struct Data_Queues* data_queues);
void mass_read_task(void* pvParameters);
int32_t get_adc_out_32();
double get_weight(int32_t result32);
void tare(int iterations);
void set_calibration_factor(double cf);
double get_calibration_factor();
double get_tare();
double get_last_mass();

extern enum diagnostic_tasks active;

#endif
