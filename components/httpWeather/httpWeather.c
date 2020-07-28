#include "httpWeather.h"
#include <esp_http_server.h>

esp_err_t get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    const char resp[] = "<html><header><title>Weather Based Sprinkler Control </title><style>body{background-color:grey;font-family:Arial,Helvetica,sans-serif;color:white}.weather{padding-left:5%;padding-top:5%;background-color:#424949;height:50%;width:20%;border-radius:5%;display:inline-block}button{background-color:#424949;font-family:Arial,Helvetica,sans-serif;color:cyan;border:none;border-radius:5%;width:5%;height:5%}</style></header><body><div id=conditions><div class=weather id=current>Current:<br><font size=\"100\">⚡☔<br>20°C</font></div><div id=future class=\"weather\">Future:<br><font size=\"100\">🌞<br>21°C</font></div></div><div>Retrieved Saturday November 09,2019 at 19:17 CST </div><br><button>Refresh </button></body></html>";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

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
    }
    /* If server failed to start, handle will be NULL */
    return server;
}