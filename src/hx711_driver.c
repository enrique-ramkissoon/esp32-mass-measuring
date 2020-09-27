#include "FreeRTOS.h"
#include "task.h"

#include "hx711_driver.h"
#include <stdint.h>

#include "driver/gpio.h"

gpio_num_t GPIO_OUTPUT_SCK = GPIO_NUM_17;
gpio_num_t GPIO_INPUT_DT = GPIO_NUM_27;

void sck_task(void* pvParameters)
{
    while(true)
    {
        if(gpio_get_level(GPIO_INPUT_DT) == 1)
        {
            configPRINTF(("READING HIGH\n"));
        }
        else
        {
            configPRINTF(("READING LOW\n"));
        }
        

        // configPRINTF(("Setting Low\n"));
        // gpio_set_level(GPIO_OUTPUT_SCK,0);

        // vTaskDelay(pdMS_TO_TICKS(2000));

        // configPRINTF(("Setting High\n"));
        // gpio_set_level(GPIO_OUTPUT_SCK,1);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

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

    xTaskCreate(sck_task,"SCKTask",configMINIMAL_STACK_SIZE,NULL,configMAX_PRIORITIES -1,NULL);

}
