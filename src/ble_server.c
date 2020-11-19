#include "FreeRTOSConfig.h"
#include "iot_demo_logging.h"
#include "iot_ble_config.h"

#include "iot_ble.h"
#include "task.h"
#include "semphr.h"
#include "platform/iot_network.h"
#include "ble_server.h"

#include "FreeRTOS.h"
#include "iot_config.h"
#include "platform/iot_network.h"

#include "ble_server.h"
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
#define MAX_PAYLOAD_LENGTH 15

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

char payload[MAX_PAYLOAD_LENGTH];

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

int compile_payload(struct Data_Queues data_queues)
{
    int status = EXIT_SUCCESS;

    while(true)
    {
        uint32_t adc_out_32 = -1;

        if(uxQueueMessagesWaiting(*(data_queues.adc_out_queue)) > 0)
        {
            xQueueReceive(*(data_queues.adc_out_queue),&adc_out_32,pdMS_TO_TICKS(50));
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
        
        snprintf(payload,MAX_PAYLOAD_LENGTH,"%d|%ld",adc_out_32,time_ms);
        printf("%s\n",payload);

        vTaskDelay(pdMS_TO_TICKS(100));


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
        if( connId == usBLEConnectionID )
        {
            IotLogInfo("Disconnected from BLE device.\n");
        }
    }
}

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
        xResp.pAttrData->pData = ( uint8_t * ) payload;
        xResp.pAttrData->size = MAX_PAYLOAD_LENGTH;
        xResp.attrDataOffset = 0;
        xResp.eventStatus = eBTStatusSuccess;
        IotBle_SendResponse( &xResp, pxReadParam->connId, pxReadParam->transId );
    }
}

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

        if( pxWriteParam->length == 1 && *(pxWriteParam->pValue) == 0xFF)
        {
            xResp.eventStatus = eBTStatusSuccess;

        }

        if( pEventParam->xEventType == eBLEWrite )
        {
            xResp.pAttrData->pData = pxWriteParam->pValue;
            xResp.attrDataOffset = pxWriteParam->offset;
            xResp.pAttrData->size = pxWriteParam->length;
            IotBle_SendResponse( &xResp, pxWriteParam->connId, pxWriteParam->transId ); //TODO: Use this to send an acknowledgement of write. Remember to give the attribute Read perms.
        }
    }
}
