#include "esp_wifi.h"
#include "iot_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "wifi_connect.h"

#define WIFI_SSID "Massy-9FA4"
#define WIFI_PASSWORD "DesertOrchid"

//connect to wifi access point
void connect_wifi()
{
    WIFINetworkParams_t networkParameters;
    WIFIReturnCode_t wifiStatus;

    wifiStatus = WIFI_On();

    if( wifiStatus == eWiFiSuccess )
    {
        configPRINT( ( "WiFi module initialization successful.\n") );
    }
    else
    {
        configPRINT( ( "WiFi module initialization failed.\n" ) );
    }

    networkParameters.pcSSID = WIFI_SSID;
    networkParameters.pcPassword = WIFI_PASSWORD;
    networkParameters.ucPasswordLength = sizeof(WIFI_PASSWORD);
    networkParameters.ucSSIDLength = sizeof(WIFI_SSID);
    networkParameters.xSecurity = eWiFiSecurityWPA2;

    wifiStatus = WIFI_ConnectAP( &( networkParameters ) );

    if( wifiStatus == eWiFiSuccess )
    {
        configPRINT( ( "WiFi Connected to AP.\n" ) );
    }
    else
    {
        configPRINT( ( "WiFi failed to connect to AP.\n" ) );
    }
}
