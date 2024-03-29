#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "esp_system.h"

#include "diagnostic_tasks.h"
#include "ble_server.h"
#include "hx711_driver.h"

#include "sys/time.h"
#include "rom/ets_sys.h"
#include "stdio.h"
#include "string.h"

void adc_task(void* pvParameters)
{
    while(true)
    {
        //configPRINTF(("Active:%d\n",*(((struct adc_args*)(pvParameters))->active_task) ));
        //uint32_t adc_out_32 = -1;

        struct adc_args adcarg = *((struct adc_args*)(pvParameters));

        struct adc_queue_structure adc_reading;

        if(uxQueueMessagesWaiting(*(adcarg.adc_queue)) > 0)
        {
            xQueueReceive(*(adcarg.adc_queue),&adc_reading,pdMS_TO_TICKS(50));
        }
        else
        {
            configPRINTF(("ADC Queue is empty!\n"));
        }

        // //TODO: Move this to the mass reading and pass within struct to queue
        // struct timeval now;
        // gettimeofday(&now, NULL);
        // int64_t time_us = (int64_t)now.tv_sec * 1000000L + (int64_t)now.tv_usec;
        // long int time_ms = time_us/1000;
        
        snprintf(adcarg.payload,adcarg.payload_size,"%d|%ld",adc_reading.adc_out,adc_reading.time_ms);
        //printf("adcout=%s\n",adcarg.payload);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void text_task(void* pvParameters)
{
    struct text_args textarg = *((struct text_args*)(pvParameters));
    while(true)
    {
        if(*(textarg.ack) == true)
        {
            fprintf(stderr,"TextTask: loading next payload\n");

            memset(textarg.payload,0,textarg.payload_size);

            for(int i=0;i<=498;i++)
            {
                if(uxQueueMessagesWaiting(*(textarg.text_queue)) > 0)
                {
                    xQueueReceive(*(textarg.text_queue),&(textarg.payload[i]), pdMS_TO_TICKS(20));
                }
                else
                {
                    *(textarg.ack) = false;
                    break;
                }
                
            }

            *(textarg.ack) = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void state_task(void* pvParameters)
{
    while(true)
    {
        configPRINTF(("STATE TASK\n"));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//this task sends:
//Task Names
//Memory Usage - Overall and per Task
//CPU Load - Overall and per Task

void stats_task(void* pvParameters)
{
    const int tasks_number = uxTaskGetNumberOfTasks();
    TaskStatus_t tasks[tasks_number];
    uint32_t total_runtime = 0;
    uint32_t free_heap = 0;

    struct stats_args statsarg = *((struct stats_args*)(pvParameters));

    while(true)
    {
        configPRINTF(("Stats Task\n"));

        uxTaskGetSystemState(tasks,tasks_number,&total_runtime);
        free_heap = xPortGetFreeHeapSize();

        clear_text_payload();

        configPRINTF(("Total Runtime: %u\n",total_runtime));
        configPRINTF(("Total Heap Free: %u\n",free_heap));

        for(int i=0;i<tasks_number;i++)
        {
            configPRINTF(("Task: %s, Runtime: %i, MemFree: %u\n",tasks[i].pcTaskName,tasks[i].ulRunTimeCounter,tasks[i].usStackHighWaterMark));
        }

        //task names
        int cur_index = 1;
        for(int i=0;i<tasks_number;i++)
        {
            snprintf(statsarg.payload + cur_index,MAX_TEXT_PAYLOAD_LENGTH,"%s|",tasks[i].pcTaskName);
            cur_index = text_payload_get_current_index();
        }
        statsarg.payload[0] = '1';

        //wait for task names to be acknowledged
        while(*(statsarg.ack) != 0x41)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        configPRINTF(("Task Names Acknowledged\n"));

        clear_text_payload();

        cur_index = 1;
        snprintf(statsarg.payload+cur_index,MAX_TEXT_PAYLOAD_LENGTH,"%u|",total_runtime);
        cur_index = text_payload_get_current_index();
        for(int i=0;i<tasks_number;i++)
        {
            snprintf(statsarg.payload+cur_index,MAX_TEXT_PAYLOAD_LENGTH,"%u|",tasks[i].ulRunTimeCounter);
            cur_index = text_payload_get_current_index();
        }
        statsarg.payload[0] = '1';

        //wait for runtime to be acknowledged
        while(*(statsarg.ack) != 0x42)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        configPRINTF(("Runtime Acknowledged\n"));

        clear_text_payload();

        cur_index = 1;
        snprintf(statsarg.payload+cur_index,MAX_TEXT_PAYLOAD_LENGTH,"%u|",free_heap);
        cur_index = text_payload_get_current_index();
        for(int i=0;i<tasks_number;i++)
        {
            snprintf(statsarg.payload+cur_index,MAX_TEXT_PAYLOAD_LENGTH,"%u|",tasks[i].usStackHighWaterMark);
            cur_index = text_payload_get_current_index();
        }
        statsarg.payload[0] = '1';

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void command_verify_connect_task(void* pvParameters)
{
    configPRINTF(("Verifying HX711 Connection\n"));

    char* cmd_result = (char*)(pvParameters);

    *cmd_result = (char)0x01;

    taskENTER_CRITICAL();
    if(gpio_get_level(DOUT_PIN) == 1)
    {
        ets_delay_us(200000);
        
        if(gpio_get_level(DOUT_PIN) == 1)
        {
            configPRINTF(("DOUT not connected \n"));
            
            *cmd_result = (char)0x00;
        }else
        {
            get_adc_out_32();
        
            ets_delay_us(2);

            if(gpio_get_level(DOUT_PIN) != 1)
            {
                configPRINTF(("EITHER SCK OR DOUT DISCONNECTED\n"));
                *cmd_result = (char)0x00;
            }
        }
        
    }else
    {
        get_adc_out_32();
        
        ets_delay_us(2);

        if(gpio_get_level(DOUT_PIN) != 1)
        {
            configPRINTF(("EITHER SCK OR DOUT DISCONNECTED\n"));
            *cmd_result = (char)0x00;
        }
    }  
    
    taskEXIT_CRITICAL();

    configPRINTF(("Exiting Verify Connect\n"));
    vTaskDelete(NULL);
}

void command_verify_sample_rate_task(void* pvParameters)
{
    configPRINTF(("Verifying HX711 Sample Rate\n"));

    struct cmd_sr_args cmdarg = *((struct cmd_sr_args*)(pvParameters));

    long int last;
    int last_delta = 0;
    int delta_sum = 0;

    for(int i=0;i<10;i++)
    {
        struct adc_queue_structure adc_reading;

        xQueueReceive(*(cmdarg.adc_queue),&adc_reading,pdMS_TO_TICKS(300));

        if(i != 0)
        {
            last_delta = adc_reading.time_ms - last;
            delta_sum += last_delta;
        }

        last = adc_reading.time_ms;
    }

    int delta_average = delta_sum / 9;

    configPRINTF(("Delta Average = %d\n",delta_average));

    *(cmdarg.result) = (char)delta_average;

    vTaskDelete(NULL);
}

void lc_calibrate(void* pvParameters)
{
    while(true)
    {
        struct adc_args adcarg = *((struct adc_args*)(pvParameters));

        struct adc_queue_structure adc_reading;

        if(uxQueueMessagesWaiting(*(adcarg.adc_queue)) > 0)
        {
            xQueueReceive(*(adcarg.adc_queue),&adc_reading,pdMS_TO_TICKS(50));
        }
        else
        {
            configPRINTF(("ADC Queue is empty!\n"));
        }
        
        snprintf(adcarg.payload,adcarg.payload_size,"%d|%f|%.2f",adc_reading.adc_out - (int)get_tare(),get_calibration_factor(),get_last_mass());

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
