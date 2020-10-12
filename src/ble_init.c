#include "ble_init.h"

#include "iot_ble.h"
#include "iot_ble_config.h"
#include "iot_ble_wifi_provisioning.h"
#include "iot_ble_numericComparison.h"
#include "aws_ble_gatt_server_demo.h"


//Advertising UUID = 0x00,0xFF,0x32, 0xF9, 0x79, 0xE6, 0xB5, 0x83, 0xFB, 0x4E, 0xAF, 0x48, 0x68, 0x11, 0x7F, 0x8A
static const BTUuid_t _advUUID =
{
    .uu.uu128 = IOT_BLE_ADVERTISING_UUID,
    .ucType   = eBTuuidType128
};

#define xServiceUUID_TYPE \
{\
    .uu.uu128 = gattDemoSVC_UUID, \
    .ucType   = eBTuuidType128 \
}
#define xCharCounterUUID_TYPE \
{\
    .uu.uu128 = gattDemoCHAR_COUNTER_UUID,\
    .ucType   = eBTuuidType128\
}
#define xCharControlUUID_TYPE \
{\
    .uu.uu128 = gattDemoCHAR_CONTROL_UUID,\
    .ucType   = eBTuuidType128\
}
#define xClientCharCfgUUID_TYPE \
{\
    .uu.uu16 = gattDemoCLIENT_CHAR_CFG_UUID,\
    .ucType  = eBTuuidType16\
}

static uint16_t usHandlesBuffer[4];

static const BTAttribute_t pxAttributeTable[] = {
     {    
         .xServiceUUID =  xServiceUUID_TYPE
     },
    {
         .xAttributeType = eBTDbCharacteristic,
         .xCharacteristic = 
         {
              .xUuid = xCharCounterUUID_TYPE,
              .xPermissions = ( IOT_BLE_CHAR_READ_PERM ),
              .xProperties = ( eBTPropRead | eBTPropNotify )
          }
     },
     {
         .xAttributeType = eBTDbDescriptor,
         .xCharacteristicDescr =
         {
             .xUuid = xClientCharCfgUUID_TYPE,
             .xPermissions = ( IOT_BLE_CHAR_READ_PERM | IOT_BLE_CHAR_WRITE_PERM )
          }
     },
    {
         .xAttributeType = eBTDbCharacteristic,
         .xCharacteristic = 
         {
              .xUuid = xCharControlUUID_TYPE,
              .xPermissions = ( IOT_BLE_CHAR_READ_PERM | IOT_BLE_CHAR_WRITE_PERM  ),
              .xProperties = ( eBTPropRead | eBTPropWrite )
          }
     }
};

void read(IotBleAttributeEvent_t * pEventParam)
{

}

void write(IotBleAttributeEvent_t * pEventParam)
{

}

void en_notification(IotBleAttributeEvent_t * pEventParam)
{

}

static const IotBleAttributeEventCallback_t pxCallBackArray[4] =
{
    NULL,
    read,
    en_notification,
    write
};

static const BTService_t xGattDemoService = 
{
  .xNumberOfAttributes = 4,
  .ucInstId = 0,
  .xType = eBTServiceTypePrimary,
  .pusHandlesBuffer = usHandlesBuffer,
  .pxBLEAttributes = (BTAttribute_t *)pxAttributeTable
};

// void IotBle_AddCustomServicesCb(void)
// {
//     BTStatus_t xStatus;

//     configPRINTF(("Creating BLE Service\n"));
//     /* Select the handle buffer. */
//     xStatus = IotBle_CreateService( (BTService_t *)&xGattDemoService, (IotBleAttributeEventCallback_t *)pxCallBackArray );
// }

void start_ble()
{
    

    configPRINTF(("Starting BLE\n"));

    IotBle_CreateService( (BTService_t *)&xGattDemoService, (IotBleAttributeEventCallback_t *)pxCallBackArray );
    IotBle_Init();
    vTaskDelay(1000);
}
