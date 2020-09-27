//#include "iot_config.h"

/* FreeRTOS includes. */

#include "FreeRTOS.h"
#include "task.h"

/* AWS System includes. */
#include "iot_system_init.h"
#include "iot_logging_task.h"

#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_interface.h"

#include "hx711_driver.h"

/* Logging Task Defines. */
#define mainLOGGING_MESSAGE_QUEUE_LENGTH    ( 32 )
#define mainLOGGING_TASK_STACK_SIZE         ( configMINIMAL_STACK_SIZE * 4 )

void hello_world_task(void* pvParameters)
{
    while(true)
    {
        configPRINTF(("Hello Task2!\n"));

        vTaskDelay(pdMS_TO_TICKS(1000));   //task goes to "blocked" state for 1s
    }

}

void hello_world_task2(void* pvParameters)
{
    while(true)
    {
        configPRINTF(("Hello Task3!\n"));

        vTaskDelay(pdMS_TO_TICKS(1000));   //task goes to "blocked" state for 1s
    }

}

extern void vApplicationIPInit( void );

static void prvMiscInitialization( void )
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();

    if( ( ret == ESP_ERR_NVS_NO_FREE_PAGES ) || ( ret == ESP_ERR_NVS_NEW_VERSION_FOUND ) )
    {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );

    /* Create tasks that are not dependent on the WiFi being initialized. */
    xLoggingTaskInitialize( mainLOGGING_TASK_STACK_SIZE, tskIDLE_PRIORITY + 5,mainLOGGING_MESSAGE_QUEUE_LENGTH );

#if AFR_ESP_LWIP
    configPRINTF( ("Initializing lwIP TCP stack\r\n") );
    tcpip_adapter_init();
#else
    configPRINTF( ("Initializing FreeRTOS TCP stack\r\n") );
    vApplicationIPInit();
#endif
}


int app_main(void)
{
    prvMiscInitialization();

    if( SYSTEM_Init() == pdPASS )
    {
        //xTaskCreate(hello_world_task,"HelloTask",configMINIMAL_STACK_SIZE,NULL,configMAX_PRIORITIES -1,NULL);
        //xTaskCreate(hello_world_task2,"HelloTask2",configMINIMAL_STACK_SIZE,NULL,configMAX_PRIORITIES -2,NULL);

        initialize_hx711();
    }

    return 0;
}

#if !AFR_ESP_LWIP
/*-----------------------------------------------------------*/
void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
    uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
    system_event_t evt;

    if( eNetworkEvent == eNetworkUp )
    {
        /* Print out the network configuration, which may have come from a DHCP
         * server. */
        FreeRTOS_GetAddressConfiguration(
            &ulIPAddress,
            &ulNetMask,
            &ulGatewayAddress,
            &ulDNSServerAddress );

        evt.event_id = SYSTEM_EVENT_STA_GOT_IP;
        evt.event_info.got_ip.ip_changed = true;
        evt.event_info.got_ip.ip_info.ip.addr = ulIPAddress;
        evt.event_info.got_ip.ip_info.netmask.addr = ulNetMask;
        evt.event_info.got_ip.ip_info.gw.addr = ulGatewayAddress;
        esp_event_send( &evt );
    }
}
#endif
