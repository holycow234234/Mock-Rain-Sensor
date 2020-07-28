#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "nvs_flash.h"
struct Weather_Data{
    nvs_handle storage;
    float currentTemp;
    int currentId;
    char currentIco[4];
    int64_t timeRetrieved;
    int64_t waterDelayEnd;
    bool sprinklersOn;
    char curDescription[30];
};

bool wateringOk (struct Weather_Data weather_data);
void clearData (struct Weather_Data *weather);
void clearCurrent(struct Weather_Data *weather);