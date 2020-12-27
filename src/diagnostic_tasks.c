#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "esp_system.h"
#include "diagnostic_tasks.h"
#include "ble_server.h"
#include "sys/time.h"

void adc_task(void* pvParameters)
{
    while(true)
    {
        //configPRINTF(("Active:%d\n",*(((struct adc_args*)(pvParameters))->active_task) ));
        uint32_t adc_out_32 = -1;

        struct adc_args adcarg = *((struct adc_args*)(pvParameters));

        if(uxQueueMessagesWaiting(*(adcarg.adc_queue)) > 0)
        {
            xQueueReceive(*(adcarg.adc_queue),&adc_out_32,pdMS_TO_TICKS(50));
        }
        else
        {
            configPRINTF(("ADC Queue is empty!\n"));
        }

        //TODO: Move this to the mass reading and pass within struct to queue
        struct timeval now;
        gettimeofday(&now, NULL);
        int64_t time_us = (int64_t)now.tv_sec * 1000000L + (int64_t)now.tv_usec;
        long int time_ms = time_us/1000;
        
        snprintf(adcarg.payload,adcarg.payload_size,"%d|%ld",adc_out_32,time_ms);
        //printf("adcout=%s\n",adcarg.payload);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
