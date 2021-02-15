#include "FreeRTOS.h"
#include "task.h"

#include "hx711_driver.h"
#include "main_util.h"
#include "ble_server.h"
#include <stdint.h>

#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "sys/time.h"

gpio_num_t GPIO_OUTPUT_SCK = GPIO_NUM_17;
gpio_num_t GPIO_INPUT_DT = GPIO_NUM_27;

double TARE = 0;
double calibration_factor = 0.004684425;

void initialize_hx711(struct Data_Queues* data_queues)
{
    gpio_config_t sck_config; 

    sck_config.mode = GPIO_MODE_OUTPUT;
    sck_config.intr_type = GPIO_PIN_INTR_DISABLE;

    uint64_t gpio_mask_sck = 1 << GPIO_OUTPUT_SCK;

    sck_config.pin_bit_mask = gpio_mask_sck;
    sck_config.pull_down_en = 0;
    sck_config.pull_up_en = 0;


    gpio_config_t dt_config;
    dt_config.mode = GPIO_MODE_INPUT;
    dt_config.intr_type = GPIO_PIN_INTR_DISABLE;

    uint64_t gpio_mask_dt = 1 << GPIO_INPUT_DT;

    dt_config.pin_bit_mask = gpio_mask_dt;
    dt_config.pull_down_en = 0;
    dt_config.pull_up_en = 0;

    gpio_config(&sck_config);
    gpio_config(&dt_config);

    //TODO: Calculate a proper stack allocation
    xTaskCreate(mass_read_task,"MassRead",10000,(void*)(data_queues),configMAX_PRIORITIES -1,NULL);

}

void mass_read_task(void* pvParameters)
{
    bool stdout_uart = true; //true if stdout is currently directed to uart0

    //Give HX711 time to initialize
    vTaskDelay(pdMS_TO_TICKS(500));

    configPRINTF(("Taring\n"));
    tare(10);

    while(true)
    {
        //configPRINTF(("ActiveFromADC = %d\n",active));

        if(active == TEXT && stdout_uart == true)
        {
            fflush(stdout);
            fclose(stdout);
            stdout = fwopen(NULL,&text_task_stdout_redirect); //redirect stdout to ble server

            stdout_uart = false;
        }
        else if(active != TEXT && stdout_uart == false)
        {
            fflush(stdout);
            fclose(stdout);
            stdout = fopen("/dev/uart/0", "w"); //redirect stdout back to uart0

            stdout_uart = true;
        }
        

        if(gpio_get_level(GPIO_INPUT_DT) == 0)
        {
            struct adc_queue_structure adc_reading;

            adc_reading.adc_out = get_adc_out_32();

            struct timeval now;
            gettimeofday(&now, NULL);
            int64_t time_us = (int64_t)now.tv_sec * 1000000L + (int64_t)now.tv_usec;
            long int time_millis = time_us/1000;

            //configPRINTF(("TIME = %ld\n", time_millis));

            adc_reading.time_ms = time_millis;
        

            xQueueOverwrite(*((QueueHandle_t*)( ((struct Data_Queues*)(pvParameters))->adc_out_queue )) , (void*)(&adc_reading));
            double weight = get_weight(adc_reading.adc_out);
        
            configPRINTF(("Weight /g = %f\n",weight));
        }


        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int32_t get_adc_out_32()
{
    //Once DT goes low, ADC output is ready.

    gpio_set_level(GPIO_OUTPUT_SCK,0);

    uint32_t result = 0;

    //Begin reading data in.
    //The read is executed in a critical block to prevent the scheduler from switching tasks while reading
    //This ensures that PD_SCK is never high for more than 60us (power down condition)
    //This critical block takes 98us to run
    taskENTER_CRITICAL();
        for(int i=23;i>=0;i--)
        {
            gpio_set_level(GPIO_OUTPUT_SCK,1);

            //DT takes 0.1us to ready after PD_SDK is set High.
            //PD_SDK high time ranges 0.2 - 50us
            ets_delay_us(2);

            gpio_set_level(GPIO_OUTPUT_SCK,0);

            uint32_t dt_bit = gpio_get_level(GPIO_INPUT_DT);
            dt_bit = dt_bit << i;
            result = result | dt_bit;

            //PD_SCK Low time ranges from 0.2us - infinity
            ets_delay_us(2);
        }

        //One additional PD_SCK clock cycle to set gain of next read to 128
        gpio_set_level(GPIO_OUTPUT_SCK,1);
        ets_delay_us(2);
        gpio_set_level(GPIO_OUTPUT_SCK,0);

    taskEXIT_CRITICAL();

    int32_t result32 = ((int32_t)(result<<8))>>8;; //HX711 outputs in 2s complement so convert to signed 32 bit number
    configPRINTF(("Result= %i\n",result32));
    return result32;
}

// double get_weight(int32_t result32)
// {
//     //Range of ADC Values = 23 ones = 8388607
//     //Range of Differential input = 0.5*(Vdd/Gain) = 0.5*(3.3/128) = 12.891mV
//     portDOUBLE Vin = ((portDOUBLE)result32/8388607)*0.012891; //in V
//     //configPRINTF(("Vin = %f\n",Vin));

//     //3.3mV => 10kg_EVENT_STA_DISCONNECTED: 2

//     //1g => (3.3e-3)/10000 = 0.33uV /g
//     double weight = (Vin/(0.33e-6));

//     return(weight);
// }

double get_weight(int32_t result32)
{
    int32_t result32_tared = result32 - TARE;

    double weight = (calibration_factor)*((double)result32_tared); //y=mx

    return(weight);
}

void tare(int iterations)
{
    double total = 0;
    for(int i=0;i<iterations;i++)
    {
        while(gpio_get_level(GPIO_INPUT_DT) == 1)
        {
            vTaskDelay(pdMS_TO_TICKS(125));
        }

        int32_t result = get_adc_out_32();
        total += result;
    }

    TARE = ((double)total/iterations);
    configPRINTF(("TARE = %f\n",TARE));
}

void set_calibration_factor(double cf)
{
    calibration_factor = cf;
    configPRINTF(("Set Calibration factor to %f\n",cf));
}

double get_calibration_factor()
{
    return calibration_factor;
}

double get_tare()
{
    return TARE;
}
