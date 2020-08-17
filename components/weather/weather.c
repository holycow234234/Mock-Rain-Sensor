#include "weather.h"
#include "esp_log.h"
#include <stdbool.h>
#define WATER_HOLD_SECS CONFIG_HOLD_TIME*60*60
//Check temp is above threshold and id > 700 to allow watering, weather ids below 700 indicate precipitation
bool wateringOk (struct Weather_Data weather_data){
    int64_t holdtime = 0; 
    if (nvs_get_i64(weather_data.storage, "hold",&holdtime) != ESP_OK){
        ESP_LOGI("WEATHER","retrieve error");
    }
    bool result = weather_data.currentId >= 700;
    weather_data.sprinklersOn = result;
    //only increase hold time if conditions are bad
    if(!result){
        if(nvs_set_i64(weather_data.storage,"hold",weather_data.timeRetrieved + WATER_HOLD_SECS) != ESP_OK){
                ESP_LOGI("WEATHER","save error");
        }
    }
    //low temperature should not result in hold time
    if (weather_data.currentTemp < CONFIG_MIN_TEMP) result = false;
    //check to make sure we don't have a current weather hold
    if(holdtime > weather_data.timeRetrieved) result = false;

    return result;
}

void clearData (struct Weather_Data *weather){
    /*
    nvs_handle storage;
    float currentTemp;
    int currentId;
    char currentIco[4];
    int64_t timeRetrieved;
    int64_t waterDelayEnd;
    bool sprinklersOn;
    char curDescription[30];
    */
    weather->currentTemp = 0.0;
    weather->currentId = 0;
    memset(&weather->currentIco,'\0',sizeof(weather->currentIco));
    weather->timeRetrieved = 0;
    weather->waterDelayEnd = 0;
    weather->sprinklersOn = false;
    memset(&weather->curDescription,'\0',sizeof(weather->curDescription));
}

