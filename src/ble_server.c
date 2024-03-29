/*
    Code for configuring and initializing BLE server adapted from: https://github.com/aws/amazon-freertos/blob/master/demos/ble/gatt_server/iot_ble_gatt_server_demo.c
*/

#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "iot_demo_logging.h"
#include "iot_ble_config.h"

#include "string.h"
#include <stdint.h>

#include "iot_ble.h"
#include "stdio.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "platform/iot_network.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "iot_config.h"
#include "iot_wifi.h"
#include "platform/iot_network.h"

#include "main_util.h"
#include "ble_server.h"
#include "diagnostic_tasks.h"
#include "sys/time.h" 
#include "hx711_driver.h"

//GATT service, characteristics and descriptor UUIDs used by the sample.

#define gattDemoSVC_UUID                 { 0x00, 0xFF, 0x69, 0xD6, 0xC6, 0xBF, 0x14, 0x90, 0x25, 0x41, 0xE7, 0x49, 0xE3, 0xD9, 0xF2, 0xC6 }
#define gattDemoCHAR_UUID_MASK           0x69, 0xD6, 0xC6, 0xBF, 0x14, 0x90, 0x25, 0x41, 0xE7, 0x49, 0xE3, 0xD9, 0xF2, 0xC6
#define gattDemoCHAR_COUNTER_UUID        { 0x01, 0xFF, gattDemoCHAR_UUID_MASK }
#define gattDemoCHAR_CONTROL_UUID        { 0x02, 0xFF, gattDemoCHAR_UUID_MASK }
#define gattDemoCLIENT_CHAR_CFG_UUID     ( 0x2902 )

/**
 * @brief Enable Notification and Enable Indication values as defined by GATT spec.
 */
#define ENABLE_NOTIFICATION              ( 0x01 )
#define ENABLE_INDICATION                ( 0x02 )

#define EVENT_BIT( event )    ( ( uint32_t ) 0x1 << event )

#define xServiceUUID_TYPE             \
    {                                 \
        .uu.uu128 = gattDemoSVC_UUID, \
        .ucType = eBTuuidType128      \
    }
#define xCharCounterUUID_TYPE                  \
    {                                          \
        .uu.uu128 = gattDemoCHAR_COUNTER_UUID, \
        .ucType = eBTuuidType128               \
    }
#define xCharControlUUID_TYPE                  \
    {                                          \
        .uu.uu128 = gattDemoCHAR_CONTROL_UUID, \
        .ucType = eBTuuidType128               \
    }
#define xClientCharCfgUUID_TYPE                  \
    {                                            \
        .uu.uu16 = gattDemoCLIENT_CHAR_CFG_UUID, \
        .ucType = eBTuuidType16                  \
    }

#define NUMBER_ATTRIBUTES 3

static uint16_t usHandlesBuffer[NUMBER_ATTRIBUTES];


static const BTAttribute_t pxAttributeTable[] =
{
    {
        .xServiceUUID = xServiceUUID_TYPE
    },
    {
        .xAttributeType = eBTDbCharacteristic,
        .xCharacteristic =
        {
            .xUuid        = xCharCounterUUID_TYPE,
            .xPermissions = ( IOT_BLE_CHAR_READ_PERM ),
            .xProperties  = ( eBTPropRead)
        }
    },
    {
        .xAttributeType = eBTDbCharacteristic,
        .xCharacteristic =
        {
            .xUuid        = xCharControlUUID_TYPE,
            .xPermissions = (IOT_BLE_CHAR_WRITE_PERM ),
            .xProperties  = (eBTPropWrite )
        }
    }
};

static const BTService_t xGattDemoService =
{
    .xNumberOfAttributes = NUMBER_ATTRIBUTES,
    .ucInstId            = 0,
    .xType               = eBTServiceTypePrimary,
    .pusHandlesBuffer    = usHandlesBuffer,
    .pxBLEAttributes     = ( BTAttribute_t * ) pxAttributeTable
};

QueueHandle_t logs_buffer;
bool message_acknowledged = true;

//QueueHandle_t state_queue; //since state logs are stored throughout program runtime, this queue's data must be persistent. Therefore, a separate queue is used for state

enum diagnostic_tasks selected = NONE;
enum diagnostic_tasks active = NONE;

char adc_payload[ADC_PAYLOAD_LENGTH];
char text_payload[MAX_TEXT_PAYLOAD_LENGTH]; //reused for stats


char state_payload[STATE_PAYLOAD_LENGTH];
int state_payload_cur_index = 0;
int64_t time_connect;
int64_t time_disconnect;
uint16_t duration = 0xFFFF;

//FILE* default_stdout = NULL;
static char stdout_buf[128];

//Stats Ack
int stats_ack = 0x00;

char cmd_result = (char)0xF0; //random initial value

TaskHandle_t text_task_handle;
TaskHandle_t adc_task_handle;
TaskHandle_t state_task_handle;
TaskHandle_t stats_task_handle;
TaskHandle_t cmd_connect_task_handle;
//TaskHandle_t net_task_handle;
TaskHandle_t adc_calibrate_task_handle;

bool tare_triggered = false;


/**
 * @brief BLE connection ID to send the notification.
 */
uint16_t usBLEConnectionID;
void read_attribute( IotBleAttributeEvent_t * pEventParam );
void write_attribute( IotBleAttributeEvent_t * pEventParam );
static BaseType_t vGattDemoSvcHook( void );

static void _connectionCallback( BTStatus_t xStatus,uint16_t connId, bool bConnected,BTBdaddr_t * pxRemoteBdAddr );

void IotBle_AddCustomServicesCb( void )
{
    vGattDemoSvcHook();
}

static const IotBleAttributeEventCallback_t pxCallBackArray[NUMBER_ATTRIBUTES] =
{
    NULL,
    read_attribute,
    write_attribute
};

void add_state(uint8_t state)
{
    if(state_payload_cur_index<STATE_PAYLOAD_LENGTH)
    {
        state_payload[state_payload_cur_index] = (char)(state);
        state_payload_cur_index++;
    }
}

void delete_active_task()
{
    configPRINTF(("Deleting Active Diagnostic Tasks\n"));
    switch(active)
    {
        case TEXT:
            fflush(stdout);
            fclose(stdout);
            stdout = fopen("/dev/uart/0", "w");
            vTaskDelete(text_task_handle);
            vQueueDelete(logs_buffer);
            break;
        case ADC:
            vTaskDelete(adc_task_handle);
            break;
        case STATE:
            vTaskDelete(state_task_handle);
            break;
        case STATS:
            vTaskDelete(stats_task_handle);
            break;
        case COMMAND:
            //vTaskDelete(cmd_connect_task_handle);
            break;
        case NETWORK:
            //vTaskDelete(net_task_handle);
            break;
        case CALIBRATE:
            vTaskDelete(adc_calibrate_task_handle);
            break;
        default:
            break;
    }
}

int text_task_stdout_redirect(void* c,const char* data,int size)
{
    //fprintf(stderr,"String: %s\n",data); //fprintf of data reveals that data is terminated by 9 random characters.

    if(uxQueueSpacesAvailable(logs_buffer) < size)
    {
        fprintf(stderr,"%s LOGS QUEUE FULL %d\n",pcTaskGetName(NULL),uxQueueMessagesWaiting(logs_buffer));
        return size;
    }

    for(int i=0;i<size-9;i++)
    {

        if((data[i] >=32 && data[i]<=126) || (data[i] == 0x00) || (data[i] == 10))
        {
            xQueueSend(logs_buffer,&(data[i]),pdMS_TO_TICKS(50));
        }
    }

    return size;
}

int task_manager(struct Data_Queues* data_queues)
{
    int status = EXIT_SUCCESS;

    struct adc_args adcarg;
    adcarg.adc_queue = data_queues->adc_out_queue;
    adcarg.payload = adc_payload;
    adcarg.payload_size = ADC_PAYLOAD_LENGTH;
    adcarg.active_task = &active;

    struct text_args textarg;
    message_acknowledged = true;
    textarg.ack = &message_acknowledged;
    textarg.active_task = &active;
    textarg.payload = text_payload;
    textarg.text_queue = &logs_buffer;
    textarg.payload_size = MAX_TEXT_PAYLOAD_LENGTH;

    struct stats_args statsarg;
    statsarg.payload = text_payload;
    statsarg.payload_size = MAX_TEXT_PAYLOAD_LENGTH;
    statsarg.ack = &stats_ack;

    struct cmd_sr_args cmdarg;
    cmdarg.adc_queue = data_queues->adc_out_queue;
    cmdarg.result = &cmd_result;

    //state_queue = xQueueCreate(1000,sizeof(uint8_t)); //holds state information in format 0xFF,MAC,CMDS,DURATION,0XFF,MAC2,CMDS2,DURATION2,0xFF,...

    while(true)
    {
        //run selected task if not already running
        if(active != selected)
        {
            delete_active_task();
            
            switch(selected)
            {
                case NONE:
                    configPRINTF(("no diagnostic tasks selected\n"));
                    break;
                case TEXT:
                    configPRINTF(("Starting Text Dump Task\n"));
                    configPRINTF(("Redirecting STDOUT\n"));

                    logs_buffer = xQueueCreate(10000,sizeof(char));
                    //set default for redirecting back to uart0 after text dump page is closed
                    //default_stdout = stdout;

                    fflush(stdout);
                    fclose(stdout);

                    stdout = fwopen(NULL,&text_task_stdout_redirect);

                    setvbuf(stdout, stdout_buf, _IOLBF, sizeof(stdout_buf));
                    xTaskCreate(text_task,"text_task",DIAGNOSTIC_TASKS_STACK_SIZE,&textarg,4,&text_task_handle);
                    break;
                case ADC:
                    configPRINTF(("Starting ADC Task\n"));
                    xTaskCreate(adc_task,"adc_task",DIAGNOSTIC_TASKS_STACK_SIZE,&adcarg,4,&adc_task_handle);
                    break;
                case STATE:
                    configPRINTF(("State Selected\n"));
                    xTaskCreate(state_task,"state_task",DIAGNOSTIC_TASKS_STACK_SIZE,NULL,4,&state_task_handle);
                    break;
                case STATS:
                    configPRINTF(("Starting Stats task\n"));
                    xTaskCreate(stats_task,"stats_task",DIAGNOSTIC_TASKS_STACK_SIZE,&statsarg,4,&stats_task_handle);
                    break;
                case COMMAND:
                    configPRINTF(("Starting Connect Verification Task\n"));
                    xTaskCreate(command_verify_connect_task,"verify_connect",DIAGNOSTIC_TASKS_STACK_SIZE,&cmd_result,4,&cmd_connect_task_handle);
                    break;
                case COMMAND_SR:
                    configPRINTF(("Starting Sample Rate Verification Task\n"));
                    xTaskCreate(command_verify_sample_rate_task,"verify_sr",DIAGNOSTIC_TASKS_STACK_SIZE,&cmdarg,4,&cmd_connect_task_handle);
                    break;
                case NETWORK:
                    configPRINTF(("Network Configuration Enabled\n"));
                    break;
                case CALIBRATE:
                    configPRINTF(("Load Cell Calibration Enabled\n"));
                    xTaskCreate(lc_calibrate,"lc_calibrate",DIAGNOSTIC_TASKS_STACK_SIZE,&adcarg,4,&adc_calibrate_task_handle);
                    break;
                default:
                    configPRINTF(("ERROR: Unknown Diagnostic Task Selected\n"));
                    break;

            }

            active = selected;
        }

        if(tare_triggered == true)
        {
            tare(5);
            tare_triggered = false;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return status;
}


static BaseType_t vGattDemoSvcHook( void )
{
    BaseType_t xRet = pdFALSE;
    BTStatus_t xStatus;
    IotBleEventsCallbacks_t xCallback;

    /* Select the handle buffer. */
    xStatus = IotBle_CreateService( ( BTService_t * ) &xGattDemoService, ( IotBleAttributeEventCallback_t * ) pxCallBackArray );

    if( xStatus == eBTStatusSuccess )
    {
        xRet = pdTRUE;
    }

    if( xRet == pdTRUE )
    {
        xCallback.pConnectionCb = _connectionCallback;

        if( IotBle_RegisterEventCb( eBLEConnection, xCallback ) != eBTStatusSuccess )
        {
            xRet = pdFAIL;
        }
    }
    
    return xRet;
}

static void _connectionCallback( BTStatus_t xStatus, uint16_t connId, bool bConnected,BTBdaddr_t * pxRemoteBdAddr )
{
    if( ( xStatus == eBTStatusSuccess ) && ( bConnected == false ) )
    {
        selected = NONE; //stop all diagnostic tasks if device disconnects unexpectedly.

        if( connId == usBLEConnectionID )
        {
            IotLogInfo("Disconnected from BLE device.\n");
        }

        //log disconnect time
        struct timeval now;
        gettimeofday(&now, NULL);
        time_disconnect = (int64_t)now.tv_sec;

        duration = (uint16_t)(time_disconnect - time_connect);

        uint8_t dur1 = (uint8_t)((duration & 0xFF00) >> 8);
        uint8_t dur2 = (uint8_t)(duration & 0x00FF);

        add_state(dur1);
        add_state(dur2);
        add_state(0x3F);
    }
    else if(( xStatus == eBTStatusSuccess ) && ( bConnected == true ))
    {
        IotLogInfo("Connected to BLE Device %X %X %X %X %X %X\n", (pxRemoteBdAddr->ucAddress)[0], (pxRemoteBdAddr->ucAddress)[1],(pxRemoteBdAddr->ucAddress)[2],
                                                                    (pxRemoteBdAddr->ucAddress)[3],(pxRemoteBdAddr->ucAddress)[4],(pxRemoteBdAddr->ucAddress)[5]);

        
        if(state_payload_cur_index < (STATE_PAYLOAD_LENGTH-7))
        {
            state_payload[state_payload_cur_index] =  (char)(0x3F);
            state_payload_cur_index++;
        }
        else
        {
            configPRINTF(("State Array Full\n"));
            return;
        }
        
        
        //Add device MAC Address
        for(int i=0;i<=5;i++)
        {
            state_payload[state_payload_cur_index] = (pxRemoteBdAddr->ucAddress)[i];
            state_payload_cur_index++;
        }

        //log connection time
        struct timeval now;
        gettimeofday(&now, NULL);
        time_connect = (int64_t)now.tv_sec;
        time_disconnect = time_connect;
        
        duration = 0xFFFF;
        
    }
}

//this function is called when the device attempts to read from the attribute
void read_attribute(IotBleAttributeEvent_t * pEventParam )
{
    IotBleReadEventParams_t * pxReadParam;
    IotBleAttributeData_t xAttrData = { 0 };
    IotBleEventResponse_t xResp;

    xResp.pAttrData = &xAttrData;
    xResp.rspErrorStatus = eBTRspErrorNone;
    xResp.eventStatus = eBTStatusFail;
    xResp.attrDataOffset = 0;

    if( pEventParam->xEventType == eBLERead )
    {
        pxReadParam = pEventParam->pParamRead;
        xResp.pAttrData->handle = pxReadParam->attrHandle;
        
        if(active == ADC)
        {
            xResp.pAttrData->pData = ( uint8_t * ) adc_payload;
            xResp.pAttrData->size = 20;
        }
        else if(active == TEXT)
        {
            if(message_acknowledged == false)
            {
                text_payload[MAX_TEXT_PAYLOAD_LENGTH-1] = (char)0;

                //fprintf(stderr,"Payload: %s",text_payload);
                xResp.pAttrData->pData = ( uint8_t * ) text_payload;
                xResp.pAttrData->size = MAX_TEXT_PAYLOAD_LENGTH;
            }
        }
        else if(active == STATE)
        {
            xResp.pAttrData->pData = ( uint8_t * ) state_payload;
            xResp.pAttrData->size = STATE_PAYLOAD_LENGTH;
        }
        else if(active == STATS)
        {
            xResp.pAttrData->pData = ( uint8_t * ) text_payload;
            xResp.pAttrData->size = (size_t)(text_payload_get_current_index() + 10);
        }
        else if(active == COMMAND || active == COMMAND_SR)
        {
            xResp.pAttrData->pData = ( uint8_t * )(&cmd_result);
            xResp.pAttrData->size = (size_t)(1);
        }
        else if (active == NETWORK)
        {
            xResp.pAttrData->pData = ( uint8_t * )(text_payload);
            xResp.pAttrData->size = (size_t)(10);
        }
        else if(active == CALIBRATE)
        {
            xResp.pAttrData->pData = ( uint8_t * ) adc_payload;
            xResp.pAttrData->size = ADC_PAYLOAD_LENGTH;
        }

        xResp.attrDataOffset = 0;
        xResp.eventStatus = eBTStatusSuccess;
        IotBle_SendResponse( &xResp, pxReadParam->connId, pxReadParam->transId );
    }
}

//this function is called when a value is placed into the attribute
void write_attribute(IotBleAttributeEvent_t * pEventParam )
{
    IotBleWriteEventParams_t * pxWriteParam;
    IotBleAttributeData_t xAttrData = { 0 };
    IotBleEventResponse_t xResp;

    xResp.pAttrData = &xAttrData;
    xResp.rspErrorStatus = eBTRspErrorNone;
    xResp.eventStatus = eBTStatusFail;
    xResp.attrDataOffset = 0;

    if( ( pEventParam->xEventType == eBLEWrite ) || ( pEventParam->xEventType == eBLEWriteNoResponse ) )
    {
        pxWriteParam = pEventParam->pParamWrite;
        xResp.pAttrData->handle = pxWriteParam->attrHandle;

        if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x00)
        {
            configPRINTF(("0x00 ENTERED. Stopping all diagnostic tasks\n"));
            selected = NONE;
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x01)
        {
            configPRINTF(("0X01 ENTERED. Starting TextDump Task\n"));
            selected = TEXT;

            add_state(0x01);
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x1F)
        {
            configPRINTF(("0x1F ENTERED. Message Acknowledged\n"));
            message_acknowledged = true;
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x02)
        {
            configPRINTF(("0X02 ENTERED. Starting ADC Task\n"));
            selected = ADC;

            add_state(0x02);
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x03)
        {
            configPRINTF(("0x03 ENTERED. Starting State Info Task\n"));
            selected = STATE;

            add_state(0x03);
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x04)
        {
            configPRINTF(("0x04 ENTERED. STATS Selected\n"));
            selected = STATS;

            add_state(0x04);
        }
        else if(pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x41)
        {
            configPRINTF(("0x41 Entered. Task Names Ack'd\n"));
            stats_ack = 0x41;
        }
        else if(pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x42)
        {
            configPRINTF(("0x41 Entered. Runtime Ack'd\n"));
            stats_ack = 0x42;
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x51)
        {
            configPRINTF(("0x51 ENTERED. Verify HX711 Connection Selected\n"));
            selected = COMMAND;
            active = NONE; //set active = none to allow for executing the command multiple times.

            add_state(0x51);
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x52)
        {
            configPRINTF(("0x52 ENTERED. Verify HX711 Sample Rate Selected\n"));
            selected = COMMAND_SR;
            active = NONE;

            add_state(0x52);
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x06)
        {
            configPRINTF(("0x06 ENTERED. Network Configuration Mode Selected\n"));
            selected = NETWORK;

            add_state(0x06);
        }
        else if(active == NETWORK)
        {
            if((pxWriteParam->pValue)[0] == 0x41) // starts with A
            {
                //write new ssid to nvs
                nvs_handle nvs_storage_handler_w;
                esp_err_t err;

                err = nvs_open("storage", NVS_READWRITE, &nvs_storage_handler_w);

                const int ssid_size = (pxWriteParam->length) - 1;

                uint32_t ssid_32[ssid_size];

                for(int i=0;i<ssid_size;i++)
                {
                    ssid_32[i] = *((pxWriteParam->pValue) + 1 + i);
                }

                err = nvs_set_blob(nvs_storage_handler_w, "ssid", ssid_32, ssid_size*sizeof(uint32_t));

                if(err != ESP_OK)
                {
                    configPRINTF(("Failed to write ssid to nvs\n"));
                }

                configPRINTF(("Set SSID to %c%c%c%c%c%c%c%c%c%c\n",ssid_32[0],ssid_32[1],ssid_32[2],ssid_32[3],ssid_32[4],ssid_32[5]
                ,ssid_32[6],ssid_32[7],ssid_32[8],ssid_32[9]));

                nvs_close(nvs_storage_handler_w);


                const uint8_t ucNumNetworks = 12; //Get 12 scan results
                WIFIScanResult_t xScanResults[ ucNumNetworks ];
                WIFI_Scan( xScanResults, ucNumNetworks );

                int8_t wifi_rssi = 0;
                int8_t wifi_ch = 0;

                char ssid_8[ssid_size + 1];
                
                for(int i=0;i<ssid_size;i++)
                {
                    ssid_8[i] = (char)(ssid_32[i]);
                }
                ssid_8[ssid_size] = (char)0x00; //end with null terminator

                for(int i=0;i<ucNumNetworks;i++)
                {
                    if(strcmp(ssid_8,xScanResults[i].cSSID) == 0)
                    {
                        configPRINTF(("Found AP\n"));
                        wifi_rssi = xScanResults[i].cRSSI;
                        wifi_ch = xScanResults[i].cChannel;
                        break;
                    }
                }

                clear_text_payload();

                snprintf(text_payload, 500, "%i|%i", wifi_rssi, wifi_ch);

            }
            else if((pxWriteParam->pValue)[0] == 0x42) //starts with B
            {
                //write new pw to nvs
                nvs_handle nvs_storage_handler_w;
                esp_err_t err;

                err = nvs_open("storage", NVS_READWRITE, &nvs_storage_handler_w);

                const int pw_size = (pxWriteParam->length) - 1;

                uint32_t pw_32[pw_size];

                for(int i=0;i<pw_size;i++)
                {
                    pw_32[i] = *((pxWriteParam->pValue) + 1 + i);
                }

                err = nvs_set_blob(nvs_storage_handler_w, "pw", pw_32, pw_size*sizeof(uint32_t));

                if(err != ESP_OK)
                {
                    configPRINTF(("Failed to write pw to nvs\n"));
                }

                configPRINTF(("Set PW to %c%c%c%c%c%c%c%c%c%c%c%c\n",pw_32[0],pw_32[1],pw_32[2],pw_32[3],pw_32[4],pw_32[5]
                ,pw_32[6],pw_32[7],pw_32[8],pw_32[9],pw_32[10],pw_32[11]));

                nvs_close(nvs_storage_handler_w);
            }
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x07)
        {
            configPRINTF(("0x07 ENTERED. Load Cell Calibration Selected\n"));
            selected = CALIBRATE;

            add_state(0x07);
        }
        else if(active == CALIBRATE)
        {
            double cal_factor = 0;

            cal_factor = strtod((char*)(pxWriteParam->pValue),NULL);

            set_calibration_factor(cal_factor);
        }
        else if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0x08)
        {
            configPRINTF(("0x08 Entered. Zeroing Scale\n"));

            tare_triggered = true;
        }
        

        xResp.eventStatus = eBTStatusSuccess;

        if( pEventParam->xEventType == eBLEWrite )
        {
            xResp.pAttrData->pData = pxWriteParam->pValue;
            xResp.attrDataOffset = pxWriteParam->offset;
            xResp.pAttrData->size = pxWriteParam->length;
            IotBle_SendResponse( &xResp, pxWriteParam->connId, pxWriteParam->transId ); //TODO: Use this to send an acknowledgement of write. Remember to give the attribute Read perms.
        }
    }
}

//returns the next fillable index in text_payload array. this function is used in the stats task
//since TaskStatus_t does not provide its array lengths
int text_payload_get_current_index()
{
    for(int i=1;i<MAX_TEXT_PAYLOAD_LENGTH;i++)
    {
        if(text_payload[i] == (char)0x00)
        {
            return i;
        }
    }

    configPRINTF(("text_payload[] IS FULL\n. Returning 0\n"));
    return 0;
}

void clear_text_payload()
{
    for(int j=0;j<MAX_TEXT_PAYLOAD_LENGTH;j++)
    {
        text_payload[j] = (char)0x00;
    }
}
