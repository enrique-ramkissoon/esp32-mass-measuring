#include "esp_wifi.h"
#include "iot_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "wifi_connect.h"

#define WIFI_SSID "Massy-9FA4"
#define WIFI_PASSWORD "DesertOrchid"

#define STORAGE_NAMESPACE "storage"

int ssid_length = 0;
int password_length = 0;

//connect to wifi access point
esp_err_t connect_wifi()
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

    nvs_handle nvs_storage_handler;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_storage_handler);

    size_t ssid_blob_size = 0;
    err = nvs_get_blob(nvs_storage_handler, "ssid", NULL, &ssid_blob_size);
    configPRINTF(("ssid blob size = %i\n",ssid_blob_size));

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        configPRINTF(("Failed to get SSID blob\n"));
        configPRINTF(("WiFI Connection Failed"));
        return err;
    }

    uint32_t* stored_ssid = malloc(ssid_blob_size);

    if(ssid_blob_size > 0)
    {
        err = nvs_get_blob(nvs_storage_handler,"ssid",stored_ssid, &ssid_blob_size);

        if(err != ESP_OK)
        {
            free(stored_ssid);
            configPRINTF(("Error Reading SSID Blob"));
            return err;
        }
    }

    char* stored_ssid_str = calloc(ssid_blob_size+1,sizeof(char));

    for(int i=0;i<ssid_blob_size;i++)
    {
        stored_ssid_str[i] = (char)(stored_ssid[i]);
        configPRINTF(("%d\n",stored_ssid[i]));
    }

    configPRINTF(("ssid from nvs: %s\n",stored_ssid_str));

    //Password
    size_t pw_blob_size = 0;
    err = nvs_get_blob(nvs_storage_handler, "pw", NULL, &pw_blob_size);

    configPRINTF(("pw blob size = %i\n",pw_blob_size));

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        configPRINTF(("Failed to get SSID blob\n"));
        configPRINTF(("WiFI Connection Failed"));
        return err;
    }

    uint32_t* stored_pw = malloc(pw_blob_size);

    if(pw_blob_size > 0)
    {
        err = nvs_get_blob(nvs_storage_handler,"pw",stored_pw, &pw_blob_size);

        if(err != ESP_OK)
        {
            free(stored_pw);
            configPRINTF(("Error Reading PW Blob"));
            return err;
        }
    }

    char* stored_pw_str = calloc(pw_blob_size+1,sizeof(char));

    for(int i=0;i<pw_blob_size;i++)
    {
        stored_pw_str[i] = (char)(stored_pw[i]);
        configPRINTF(("%d\n",stored_pw[i]));
    }

    configPRINTF(("pw from nvs: %s\n",stored_pw_str));

    networkParameters.pcSSID = stored_ssid_str;
    networkParameters.pcPassword = stored_pw_str;
    networkParameters.ucPasswordLength = pw_blob_size;
    networkParameters.ucSSIDLength = ssid_blob_size;
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

    free(stored_ssid);
    free(stored_ssid_str);
    free(stored_pw);
    free(stored_pw_str);
    nvs_close(nvs_storage_handler);

    return ESP_OK;
}
