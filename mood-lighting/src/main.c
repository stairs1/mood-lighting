#include <string.h>

#include "freertos/FreeRTOS.h"

#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "driver/ledc.h"

#include "mongoose.h"
#include "webpage.h"

#define WIFI_SSID "not"
#define WIFI_PASS "it"

#define MG_LISTEN_ADDR "80"

ledc_timer_config_t ledc_timer = {
    .duty_resolution = 10,
    .freq_hz = 1000,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .clk_cfg = LEDC_AUTO_CLK,
  };

ledc_channel_config_t ledc_channels[3] = {
  {
    .channel = LEDC_CHANNEL_0,
    .duty = 0,
    .gpio_num = 4,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .hpoint = 0,
    .timer_sel = LEDC_TIMER_0,
  },
  {
    .channel = LEDC_CHANNEL_1,
    .duty = 0,
    .gpio_num = 18,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .hpoint = 0,
    .timer_sel = LEDC_TIMER_0,
  },
  {
    .channel = LEDC_CHANNEL_2,
    .duty = 0,
    .gpio_num = 19,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .hpoint = 0,
    .timer_sel = LEDC_TIMER_0,
  },
};

static void init_leds(){
  
  ledc_timer_config(&ledc_timer);
  for(int ch = 0; ch < 3; ch++){
    ledc_channel_config(&ledc_channels[ch]);
  }
  ledc_fade_func_install(0);

  //test only
  ledc_set_duty(ledc_channels[0].speed_mode, ledc_channels[0].channel, 500);
  ledc_update_duty(ledc_channels[0].speed_mode, ledc_channels[0].channel);
}

static void set_leds(int red, int green, int blue){
  if(red >=0 && red <= 1023){
    ledc_set_duty(ledc_channels[0].speed_mode, ledc_channels[0].channel, red);
    ledc_update_duty(ledc_channels[0].speed_mode, ledc_channels[0].channel);
  }
  if(green >= 0 && green <= 1023){
    ledc_set_duty(ledc_channels[1].speed_mode, ledc_channels[1].channel, green);
    ledc_update_duty(ledc_channels[1].speed_mode, ledc_channels[1].channel);
  }
  if(blue >= 0 && blue <= 1023){
    ledc_set_duty(ledc_channels[2].speed_mode, ledc_channels[2].channel, blue);
    ledc_update_duty(ledc_channels[2].speed_mode, ledc_channels[2].channel);
  }
}

static esp_err_t event_handler(void *ctx, system_event_t *event) {
  (void) ctx;
  (void) event;
  return ESP_OK;
}

static void log_request(struct http_message *hm, struct mg_connection *nc){
  /*
   * Print request to serial for debugging
   */
  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
  printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len,
          hm->method.p, (int) hm->uri.len, hm->uri.p);
}

static void default_endpoint(struct http_message *hm, struct mg_connection *nc){
  /* 
   * GET: serve rgb selection webpage
   * POST: set rgb of lights
   */

  char addr[32];
  mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                      MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);

  // if get, sauce page, if post, write body to serial
  if(mg_vcmp(&hm->method, "GET") == 0){
    printf("received get request\n");
    static const char *reply_fmt =
      "HTTP/1.0 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html\r\n"
      "\r\n"
      "%s\n";
    // read from controller_html_start to controller_html_end.
    // send this data as text/html in response body.
    mg_printf(nc, reply_fmt, html);
    // mg_printf(nc, reply_fmt, addr);
  }
  else if(mg_vcmp(&hm->method, "POST") == 0){
    printf("POST\n");

    // max body size 100 bytes
    char buf[100] = {0};
    memcpy(buf, hm->body.p, sizeof(buf) -1 < hm->body.len ? sizeof(buf) - 1: hm->body.len);

    // red=xxx&green=xxx&blue=xxx
    // takes form variables. May add json support later
    // takes a 4-digit number for each color. Intended range 0-255.
    char sred[5], sgreen[5], sblue[5];
    int red = -1, green = -1, blue = -1;
    if(mg_get_http_var(&hm->body, "red", sred, sizeof(sred)) > 0){
      red = atoi(sred);
    }
    if(mg_get_http_var(&hm->body, "green", sgreen, sizeof(sgreen)) > 0){
      green = atoi(sgreen);
    } 
    if(mg_get_http_var(&hm->body, "blue", sblue, sizeof(sblue)) > 0){
      blue = atoi(sblue);
    }
    printf("%d, %d, %d\n", red, green, blue);
    set_leds(red, green, blue);

    //reload page for extra input
    static const char *reply_fmt =
      "HTTP/1.0 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html\r\n"
      "\r\n"
      "%s\n";
    mg_printf(nc, reply_fmt, html);
 
  }
  else{
    printf("unsupported http request type\n");
    static const char *reply_fmt =
      "HTTP/1.0 400 Bad Request\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "GET/POST only supported %s\n";
    mg_printf(nc, reply_fmt, addr);
  }
  nc->flags |= MG_F_SEND_AND_CLOSE;

}

static void handle_http_request(struct http_message *hm, struct mg_connection *nc){
  /*
   * Route http requests by url to endpoint functions
   * Also logs reqest for debugging
   */
  log_request(hm, nc);
  if(mg_vcmp(&hm->uri, "/") == 0){
    default_endpoint(hm, nc);
  }
  else{
    printf("url not found\n");
    char addr[32];
    mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                        MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
    static const char *reply_fmt =
      "HTTP/1.0 404 Not Found\r\n"
      "Connection: close\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "404 Not Found\n";
    mg_printf(nc, reply_fmt, addr);
    nc->flags |= MG_F_SEND_AND_CLOSE;
  }
}

static void mg_ev_handler(struct mg_connection *nc, int ev, void *ev_data) {

  switch (ev) {
    case MG_EV_ACCEPT: {
      char addr[32];
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      printf("Connection %p from %s\n", nc, addr);
      break;
    }
    case MG_EV_HTTP_REQUEST: {
      struct http_message *hm = (struct http_message *) ev_data;
      handle_http_request(hm, nc);
      break;
    }
    case MG_EV_CLOSE: {
      printf("Connection %p closed\n", nc);
      break;
    }
  }
}

void app_main(void) {
  nvs_flash_init();
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  /* Set up lights */
  init_leds();

  /* Initialize WiFi */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  wifi_config_t sta_config = {
      .sta = {.ssid = WIFI_SSID, .password = WIFI_PASS, .bssid_set = false}};
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());

  /* Start Mongoose */
  struct mg_mgr mgr;
  struct mg_connection *nc;

  printf("Starting web-server on port %s\n", MG_LISTEN_ADDR);

  mg_mgr_init(&mgr, NULL);

  nc = mg_bind(&mgr, MG_LISTEN_ADDR, mg_ev_handler);
  if (nc == NULL) {
    printf("Error setting up listener!\n");
    return;
  }
  mg_set_protocol_http_websocket(nc);

  /* Processing events */
  while (1) {
    mg_mgr_poll(&mgr, 1000);
  }
}
