#ifndef DIAGNOSTIC_TASKS_H
#define DIAGNOSTIC_TASKS_H

void adc_task(void* pvParameters);
void text_task(void* pvParameters);
void state_task(void* pvParameters);
void stats_task(void* pvParameters);
void command_verify_connect_task(void* pvParameters);
void command_verify_sample_rate_task(void* pvParameters);
void lc_calibrate(void* pvParameters);

#endif
