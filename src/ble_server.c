/*
 * FreeRTOS V202007.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

#include "FreeRTOSConfig.h"
#include "iot_demo_logging.h"
#include "iot_ble_config.h"

#include "iot_ble.h"
#include "task.h"
#include "semphr.h"
#include "platform/iot_network.h"
#include "ble_server.h"



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "FreeRTOS.h"
#include "iot_config.h"
#include "platform/iot_network.h"

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

/**
 * @brief Characteristics used by the GATT sample service.
 */
typedef enum
{
    egattDemoService = 0,      /**< SGatt demo service. */
    egattDemoCharCounter,      /**< Keeps track of a counter value which is incremented periodically and optionally sent as notification to a GATT client */
    egattDemoCharDescrCounter, /**< Client characteristic configuration descriptor used by the GATT client to enable notifications on Counter characteristic */
    egattDemoCharControl,      /**< Accepts commands from a GATT client to stop/start/reset the counter value */
    egattDemoNbAttributes
} eGattDemoAttributes_t;

/**
 * @brief Events generated for the commands sent from GATT client over Control characteristic
 */
// typedef enum
// {
//     eGattDemoStart = 0, /**< Starts/resumes the counter value update. */
//     eGattDemoStop = 1,  /**< Stops counter value update. Also stops sending notifications to GATT client if its already enabled. */
//     eGattDemoReset = 2, /**< Resets the counter value to zero */
// } GattDemoEvent_t;

#define EVENT_BIT( event )    ( ( uint32_t ) 0x1 << event )
//////////////////////////////////////////////////////////////////////////////////////////////////////aws_ble_gatt_server_demo.h




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

static uint16_t usHandlesBuffer[ egattDemoNbAttributes ];


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
            .xProperties  = ( eBTPropRead | eBTPropNotify )
        }
    },
    {
        .xAttributeType = eBTDbDescriptor,
        .xCharacteristicDescr =
        {
            .xUuid        = xClientCharCfgUUID_TYPE,
            .xPermissions = ( IOT_BLE_CHAR_READ_PERM | IOT_BLE_CHAR_WRITE_PERM )
        }
    },
    {
        .xAttributeType = eBTDbCharacteristic,
        .xCharacteristic =
        {
            .xUuid        = xCharControlUUID_TYPE,
            .xPermissions = ( IOT_BLE_CHAR_READ_PERM | IOT_BLE_CHAR_WRITE_PERM ),
            .xProperties  = ( eBTPropRead | eBTPropWrite )
        }
    }
};

static const BTService_t xGattDemoService =
{
    .xNumberOfAttributes = egattDemoNbAttributes,
    .ucInstId            = 0,
    .xType               = eBTServiceTypePrimary,
    .pusHandlesBuffer    = usHandlesBuffer,
    .pxBLEAttributes     = ( BTAttribute_t * ) pxAttributeTable
};

/**
 * @brief Task used to update the counter periodically.
 */
TaskHandle_t xCounterUpdateTask = NULL;

/**
 * @brief Counter value.
 */
uint32_t ulCounter = 0;

/**
 * @brief Interval in Milliseconds to update the counter.
 */
TickType_t xCounterUpdateInterval = pdMS_TO_TICKS( 1000 );

/**
 * @brief Should send the counter update as a notification.
 */
BaseType_t xNotifyCounterUpdate = pdFALSE;

/**
 * @brief BLE connection ID to send the notification.
 */
uint16_t usBLEConnectionID;

/**
 * @brief Callback to read request from a GATT client on the Counter value characteristic.
 * Returns the current counter value.
 * @param[in] pxAttribute Attribute structure for the characteristic
 * @param pEventParam Event param for the read Request.
 */
void vReadCounter( IotBleAttributeEvent_t * pEventParam );

/**
 * @brief Callback to receive write request from a GATT client to set the counter status.
 *
 * Sets the event to start/stop/reset the counter update operation and sends back a response.
 *
 * @param pxAttribute
 * @param pEventParam
 */
void vWriteCommand( IotBleAttributeEvent_t * pEventParam );

/**
 * @brief Callback to enable notification when GATT client writes a value to the Client Characteristic
 * Configuration descriptor.
 *
 * @param pxAttribute  Attribute structure for the Client Characteristic Configuration Descriptor
 * @param pEventParam Write/Read event parametes
 */

void vEnableNotification( IotBleAttributeEvent_t * pEventParam );

// /**
//  * @brief Task used to update the counter value periodically
//  *
//  * Sends a notification of counter update to GATT client if it has enabled notification via Client Characteristic
//  * Configuration Descriptor. Also receives events from GATT client and starts/stops/resets counter update.
//  *
//  * @param pvParams NULL
//  */
// //     static void vCounterUpdateTaskFunction( void * pvParams );

// // /**
// //  * @brief Hook for an application's own custom services.
// //  *
// //  * @return pdTRUE on success creation of the custom services, pdFALSE otherwise.
// //  */
static BaseType_t vGattDemoSvcHook( void );

// /**
//  * @brief Callback for BLE connect/disconnect.
//  *
//  * Stops the Counter Update task on disconnection.
//  *
//  * @param[in] xStatus Status indicating result of connect/disconnect operation
//  * @param[in] connId Connection Id for the connection
//  * @param[in] bConnected true if connection, false if disconnection
//  * @param[in] pxRemoteBdAddr Remote address of the BLE device which connected or disconnected
//  */
static void _connectionCallback( BTStatus_t xStatus,uint16_t connId, bool bConnected,BTBdaddr_t * pxRemoteBdAddr );

void IotBle_AddCustomServicesCb( void )
{
    vGattDemoSvcHook();
}

static const IotBleAttributeEventCallback_t pxCallBackArray[ egattDemoNbAttributes ] =
{
    NULL,
    vReadCounter,
    vEnableNotification,
    vWriteCommand
};


int vGattDemoSvcInit()
{
    int status = EXIT_SUCCESS;


    while( 1 )
    {
        vTaskDelay( 1000 );
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

    static void _connectionCallback( BTStatus_t xStatus,
                                     uint16_t connId,
                                     bool bConnected,
                                     BTBdaddr_t * pxRemoteBdAddr )
    {
        if( ( xStatus == eBTStatusSuccess ) && ( bConnected == false ) )
        {
            if( connId == usBLEConnectionID )
            {
                IotLogInfo( " Disconnected from BLE device. Stopping the counter update \n" );
            }
        }
    }

void vReadCounter( IotBleAttributeEvent_t * pEventParam )
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
        ulCounter+=1; //ADDED THIS LINE TO INCREMENT ON EACH READ

        pxReadParam = pEventParam->pParamRead;
        xResp.pAttrData->handle = pxReadParam->attrHandle;
        xResp.pAttrData->pData = ( uint8_t * ) &ulCounter;
        xResp.pAttrData->size = sizeof( ulCounter );
        xResp.attrDataOffset = 0;
        xResp.eventStatus = eBTStatusSuccess;
        IotBle_SendResponse( &xResp, pxReadParam->connId, pxReadParam->transId );
    }
}

/*-----------------------------------------------------------*/

void vWriteCommand( IotBleAttributeEvent_t * pEventParam )
{
    IotBleWriteEventParams_t * pxWriteParam;
    IotBleAttributeData_t xAttrData = { 0 };
    IotBleEventResponse_t xResp;
    uint8_t ucEvent;

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
            ulCounter = 0;
            xResp.eventStatus = eBTStatusSuccess;

        }

        if( pEventParam->xEventType == eBLEWrite )
        {
            xResp.pAttrData->pData = pxWriteParam->pValue;
            xResp.attrDataOffset = pxWriteParam->offset;
            xResp.pAttrData->size = pxWriteParam->length;
            IotBle_SendResponse( &xResp, pxWriteParam->connId, pxWriteParam->transId );
        }
    }
}

/*-----------------------------------------------------------*/

void vEnableNotification( IotBleAttributeEvent_t * pEventParam )
{
    IotBleWriteEventParams_t * pxWriteParam;
    IotBleAttributeData_t xAttrData = { 0 };
    IotBleEventResponse_t xResp;
    uint16_t ucCCFGValue;


    xResp.pAttrData = &xAttrData;
    xResp.rspErrorStatus = eBTRspErrorNone;
    xResp.eventStatus = eBTStatusFail;
    xResp.attrDataOffset = 0;

    if( ( pEventParam->xEventType == eBLEWrite ) || ( pEventParam->xEventType == eBLEWriteNoResponse ) )
    {
        pxWriteParam = pEventParam->pParamWrite;

        xResp.pAttrData->handle = pxWriteParam->attrHandle;

        if( pxWriteParam->length == 2 )
        {
            ucCCFGValue = ( pxWriteParam->pValue[ 1 ] << 8 ) | pxWriteParam->pValue[ 0 ];

            if( ucCCFGValue == ( uint16_t ) ENABLE_NOTIFICATION )
            {
                IotLogInfo( "Enabled Notification for Read Characteristic\n" );
                xNotifyCounterUpdate = pdTRUE;
                usBLEConnectionID = pxWriteParam->connId;
            }
            else if( ucCCFGValue == 0 )
            {
                xNotifyCounterUpdate = pdFALSE;
            }

            xResp.eventStatus = eBTStatusSuccess;
        }

        if( pEventParam->xEventType == eBLEWrite )
        {
            xResp.pAttrData->pData = pxWriteParam->pValue;
            xResp.pAttrData->size = pxWriteParam->length;
            xResp.attrDataOffset = pxWriteParam->offset;

            IotBle_SendResponse( &xResp, pxWriteParam->connId, pxWriteParam->transId );
        }
    }
}
