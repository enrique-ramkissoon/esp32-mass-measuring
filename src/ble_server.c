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

#include "iot_config.h"
#include "platform/iot_network.h"

#include "main_util.h"
#include "ble_server.h"
#include "diagnostic_tasks.h"
#include "sys/time.h" 

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

#define ADC_PAYLOAD_LENGTH 20
#define MAX_TEXT_PAYLOAD_LENGTH 500
#define STATE_PAYLOAD_LENGTH 500

QueueHandle_t logs_buffer;
bool message_acknowledged = true;

//QueueHandle_t state_queue; //since state logs are stored throughout program runtime, this queue's data must be persistent. Therefore, a separate queue is used for state

enum diagnostic_tasks selected = NONE;
enum diagnostic_tasks active = NONE;

char adc_payload[ADC_PAYLOAD_LENGTH];
char text_payload[MAX_TEXT_PAYLOAD_LENGTH];


char state_payload[STATE_PAYLOAD_LENGTH];
int state_payload_cur_index = 0;
int64_t time_connect;
int64_t time_disconnect;
uint16_t duration = 0xFFFF;

//FILE* default_stdout = NULL;
static char stdout_buf[128];

TaskHandle_t text_task_handle;
TaskHandle_t adc_task_handle;
TaskHandle_t state_task_handle;
TaskHandle_t stats_task_handle;
TaskHandle_t cmd_task_handle;
TaskHandle_t net_task_handle;

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
    printf("Deleting Diagnostic Tasks\n");
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
            vTaskDelete(cmd_task_handle);
            break;
        case NETWORK:
            vTaskDelete(net_task_handle);
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
                    configPRINTF(("no diag selected\n"));
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
                    xTaskCreate(text_task,"text_task",configMINIMAL_STACK_SIZE*20,&textarg,4,&text_task_handle);
                    break;
                case ADC:
                    configPRINTF(("Starting ADC Task\n"));
                    xTaskCreate(adc_task,"adc_task",configMINIMAL_STACK_SIZE*5,&adcarg,4,&adc_task_handle);
                    break;
                case STATE:
                    configPRINTF(("State Selected\n"));
                    xTaskCreate(state_task,"state_task",configMINIMAL_STACK_SIZE*5,&adcarg,4,&state_task_handle);
                    break;
                default:
                    break;

            }

            active = selected;
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
            xResp.pAttrData->size = ADC_PAYLOAD_LENGTH;
        }
        else if(active == TEXT)
        {
            if(message_acknowledged == false)
            {
                text_payload[499] = (char)0;

                fprintf(stderr,"Payload: %s",text_payload);
                xResp.pAttrData->pData = ( uint8_t * ) text_payload;
                xResp.pAttrData->size = MAX_TEXT_PAYLOAD_LENGTH;
            }
        }
        else if(active == STATE)
        {
            xResp.pAttrData->pData = ( uint8_t * ) state_payload;
            xResp.pAttrData->size = STATE_PAYLOAD_LENGTH;
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
