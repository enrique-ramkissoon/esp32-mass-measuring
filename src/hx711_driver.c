#include "FreeRTOS.h"
#include "task.h"

#include "hx711_driver.h"
#include <stdint.h>

#include "driver/gpio.h"
#include "rom/ets_sys.h"

gpio_num_t GPIO_OUTPUT_SCK = GPIO_NUM_17;
gpio_num_t GPIO_INPUT_DT = GPIO_NUM_27;

void initialize_hx711()
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
    xTaskCreate(sck_task,"SCKTask",20000,NULL,configMAX_PRIORITIES -1,NULL);

}

void sck_task(void* pvParameters)
{
    //Give HX711 a time to initialize
    vTaskDelay(pdMS_TO_TICKS(500));

    while(true)
    {
        //configPRINTF(("Read Task\n"));
        // if(gpio_get_level(GPIO_INPUT_DT) == 1)
        // {
        //     configPRINTF(("READING HIGH\n"));
        // }
        // else
        // {
        //     configPRINTF(("READING LOW\n"));
        // }
        

        // configPRINTF(("Setting Low\n"));
        // gpio_set_level(GPIO_OUTPUT_SCK,0);

        // vTaskDelay(pdMS_TO_TICKS(2000));

        // configPRINTF(("Setting High\n"));
        // gpio_set_level(GPIO_OUTPUT_SCK,1);

        if(gpio_get_level(GPIO_INPUT_DT) == 0)
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

            int32_t result32 = ((int32_t)(result<<8))>>8;; //HX711 outputs in 2s complement
            configPRINTF(("Result= %i\n",result32));

            //Range of ADC Values = 23 ones = 8388607
            //Range of Differential input = 0.5*(Vdd/Gain) = 0.5*(3.3/128) = 12.891
            portDOUBLE Vin = ((portDOUBLE)result32/8388607)*0.012891;
            configPRINTF(("Weight = %f\n",Vin));

            //3.3mV => 10kg
            //1g => (3.3e-3)/10000 = 0.33uV /g
            double weight = (Vin/(0.33e-6));

            configPRINTF(("Weight /g = %f\n",weight));
        }


        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
