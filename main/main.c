#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "weather.h"
#include "jsmn.h"


#include "esp_log.h"

#define INCLUDE_vTaskSuspend 1

#define CURRENT_CONDITIONS_URL "https://api.openweathermap.org/data/2.5/onecall?lat=" CONFIG_WEATHER_LAT "&lon=" CONFIG_WEATHER_LNG "&appid=" CONFIG_API_KEY "&exclude=minutely,hourly,daily&units=metric"

#define TAG "WEATHER"
#define UPDATE_PRIORITY 11
#define UPDATE_STACK 4500
#define WEATHER_BUFFER 1200
#define INCLUDE_uxTaskGetStackHighWaterMark 1

httpd_handle_t weather_server = NULL;
struct Weather_Data weather;
//the html is stored as a minified format string
const char* html = "<html> <head> <title>Weather Based Sprinkler Control </title> <style>body{background-color: #424949; font-family: Arial, Helvetica, sans-serif; color: white;}button{background-color: #424949; font-family: Arial, Helvetica, sans-serif; color: cyan; border: none; border-radius: 5%%; width: 5%%; height: 5%%;}img{mix-blend-mode: hard-light;}</style> <script>window.onload=function(){var current_date=new Date(%lld*1000); document.getElementById('date').innerText='Retrieved '+ current_date.toString(); var sprink_bool=%s; document.getElementById('sprink_stat').innerText=(sprink_bool ? 'enabled':'disabled'); document.getElementById('sprink_stat').style.color=(sprink_bool ? 'green':'red');}</script> </head> <body> <font size='100' > <img src='http://openweathermap.org/img/wn/%s@2x.png'> <br>%.2f&deg;C <br>%s </font> <br>Sprinklers are <font id='sprink_stat'></font> <br><br><div id='date'> </div></div></body></html>";
char html_formatted[1700];
nvs_handle nvs_store;
//TaskHandle_t forecast = NULL;
TaskHandle_t weather_subtask = NULL;
TaskHandle_t weather_supertask = NULL;
httpd_handle_t start_weather_webserver(void);


/*Handler for current conditions data
*
*/
esp_err_t current_conditions_handle(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA){
        //ESP_LOGI(TAG,evt->data);
        jsmn_parser parser;
        jsmntok_t tokens[evt->data_len]; 
        jsmn_init(&parser);
        int r = jsmn_parse(&parser, evt->data, evt->data_len, tokens, 2048);
        if(r > 0){
            clearData(&weather);
            for(int i = 1; i < r; i++){
                //the json parser works by giving you the start and end positions of the json tokens,
                //so this mess compares the part of the json string recieved with the string we're looking for, at the position given by the json parser
                if(tokens[i].type==JSMN_STRING){
                    if(strncmp(evt->data + tokens[i].start,"temp",tokens[i].end-tokens[i].start) == 0 && tokens[i].end-tokens[i].start == strlen("temp")){
                        weather.currentTemp = atof((char*)evt->data + tokens[i + 1].start);
                    }
                    if(strncmp(evt->data + tokens[i].start,"dt",tokens[i].end-tokens[i].start) == 0 && tokens[i].end-tokens[i].start == strlen("dt")){
                        weather.timeRetrieved = atoll((char*)evt->data + tokens[i + 1].start);
                    }
                    if(strncmp(evt->data + tokens[i].start,"description",tokens[i].end-tokens[i].start) == 0 && tokens[i].end-tokens[i].start == strlen("description")){//not allowed
                        strncpy(weather.curDescription,((char*)evt->data + tokens[i + 1].start),tokens[i + 1].end-tokens[i + 1].start);
                    }
                    if(strncmp(evt->data + tokens[i].start,"icon",tokens[i].end-tokens[i].start) == 0 && tokens[i].end-tokens[i].start == strlen("icon")){
                        strncpy(weather.currentIco,((char*)evt->data + tokens[i + 1].start),tokens[i + 1].end-tokens[i + 1].start);
                    }
                    /*
                    json structure is:
                    ...
                    weather:[
                        {
                            id: ***
                            ...
                        }
                    ]
                    ...
                    */
                   //currently handles only one weather condition, there may be multiple conditions returned though.
                   //could be a future improvement but this does what I want for now
                    if(strncmp(evt->data + tokens[i].start,"id",tokens[i].end-tokens[i].start) == 0 && tokens[i].end-tokens[i].start == strlen("id")){
                        weather.currentId = atoi((char*)evt->data + tokens[i + 1].start);
                    }
                }
            }
        }//else parsing error

      
    }
    return ESP_OK;
}

void get_weather_subtask(void * pvParameters){
    bool curSuccess = false;
    for(; ; ){
        ESP_LOGI("SUBTASK","Running");
        //clearData(weather);
        esp_http_client_config_t config = {
        .url =  CURRENT_CONDITIONS_URL,
        .event_handler = current_conditions_handle,
        .buffer_size = WEATHER_BUFFER,
        .timeout_ms = 10000, //shitty network :(
    };
    esp_http_client_handle_t client;
    esp_err_t err;
    if(!curSuccess){
        client = esp_http_client_init(&config);
        err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            
            ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
            curSuccess = true;
    }
        esp_http_client_cleanup(client);
    
    

    }
    

    if(weather.timeRetrieved != 0 && weather_server == NULL){
        //we have data to show, so now we can run our webserver
        ESP_LOGI(TAG,"EL Connecto");
        weather_server = start_weather_webserver();
    }
    //call wateringOk() & update gpio;
    bool sprinklers = wateringOk(weather);
    gpio_set_level(12,(sprinklers ? 0 : 1));
    weather.sprinklersOn = sprinklers;
    if(curSuccess){
        vTaskSuspend( NULL );
        curSuccess = false;
    }else{
        //wait 10 sec to try again
        vTaskDelay(10*1000/ portTICK_PERIOD_MS);
        ESP_LOGI("SUBTASK","Retrying in 10");
    }
    
    }
    
}



void update_weather(void * pvParameters){
    for( ; ; ){
    ESP_LOGI("SUPERTASK","Running");
    if(weather_subtask == NULL) {
        xTaskCreate(get_weather_subtask, "Data Refresh", UPDATE_STACK, NULL, UPDATE_PRIORITY,&weather_subtask);
    }else{
        vTaskResume(weather_subtask);
    }
    
    vTaskDelay(CONFIG_REFRESH_INTERVAL*1000/ portTICK_PERIOD_MS);        
    }
}

esp_err_t get_handler(httpd_req_t *req)
{
    ESP_LOGI("TASK_DEBUG","Supertask HighwaterMark: %lu",uxTaskGetStackHighWaterMark(weather_supertask));
    ESP_LOGI("TASK_DEBUG","Subtask HighwaterMark: %lu",uxTaskGetStackHighWaterMark(weather_subtask));
    ESP_LOGI("TASK_DEBUG","Supertask State: %d",eTaskGetState(weather_supertask));
    ESP_LOGI("TASK_DEBUG","Subtask State: %d",eTaskGetState(weather_subtask));
    memset(html_formatted,'\0',sizeof(html_formatted));
    snprintf(html_formatted,sizeof(html_formatted) ,html,weather.timeRetrieved,(weather.sprinklersOn ? "true" : "false"),weather.currentIco,weather.currentTemp,weather.curDescription);
    httpd_resp_send(req, html_formatted, strlen(html_formatted));
    ESP_LOGI(TAG,"Sent Index Page");
    return ESP_OK;
}

// initially planned to add manual refresh, but its disabled for now, may add later
// esp_err_t http_refresh(httpd_req_t *req){
//     char resp[] = "";
//     httpd_resp_send(req, resp, strlen(resp));
//     //todo manual refresh
//     //vTaskResume(weather_subtask);
//     gpio_set_level(12,1);
//     return ESP_OK;
// }

httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};


httpd_handle_t start_weather_webserver(void)
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        //httpd_register_uri_handler(server, &uri_refresh);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}

esp_err_t event_handler(void *ctx, system_event_t *event)
{    
    if(event->event_id == SYSTEM_EVENT_STA_GOT_IP && weather.timeRetrieved == 0){
        //we are on the network, but have no data, so we can now start our refresh tasks
        if(weather_supertask == NULL)  xTaskCreate(update_weather, "Data Refresh Supertask", UPDATE_STACK+1300, NULL, UPDATE_PRIORITY,&weather_supertask);
    }
    if(event->event_id==  SYSTEM_EVENT_STA_LOST_IP){
        //attempt wifi reconnect on wifi disconnect
    	esp_wifi_disconnect();
        esp_wifi_connect();
    }
    	
    return ESP_OK;
}

void app_main(void)
{
    
    nvs_flash_init();
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &nvs_store));
    weather.storage = nvs_store;
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    wifi_config_t sta_config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &sta_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    for(;;){
        //if we don't see wifi on boot, keep trying to connect to the network.
        if(esp_wifi_connect() == ESP_OK){
            break;
        }else{
            vTaskDelay(5*1000/ portTICK_PERIOD_MS);
        }
    }
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = 1ULL<<12;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
    //gpio_set_direction(GP, GPIO_MODE_OUTPUT);
    gpio_set_level(12,0);

}
