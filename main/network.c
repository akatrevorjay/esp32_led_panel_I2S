
#include <esp_event_loop.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <sys/param.h>

#include "network.h"
#include <stdlib.h>
#include <string.h>

/* static http_server_t server; */
/* static httpd_handle_t server = NULL; */

/* static EventGroupHandle_t wifi_event_group; */
/* static const int WIFI_CONNECTED_BIT = BIT0; */
static request_callback_t request_callback;

static const char *LOG_TAG = "network";

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 * The examples use simple WiFi configuration that you can set via
 * 'make menuconfig'.
 * If you'd rather not, just change the below entries to strings
 * with the config you want -
 * ie. #define EXAMPLE_WIFI_SSID "mywifissid"
 */
/* #define EXAMPLE_WIFI_SSID CONFIG_WIFI_SSID */
/* #define EXAMPLE_WIFI_PASS CONFIG_WIFI_PASSWORD */

/* An HTTP GET handler */
esp_err_t hello_get_handler(httpd_req_t *req) {
  char *buf;
  size_t buf_len;

  /* Get header value string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    /* Copy null terminated value string into buffer */
    if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
      ESP_LOGI(LOG_TAG, "Found header => Host: %s", buf);
    }
    free(buf);
  }

  buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) ==
        ESP_OK) {
      ESP_LOGI(LOG_TAG, "Found header => Test-Header-2: %s", buf);
    }
    free(buf);
  }

  buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) ==
        ESP_OK) {
      ESP_LOGI(LOG_TAG, "Found header => Test-Header-1: %s", buf);
    }
    free(buf);
  }

  /* Read URL query string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      ESP_LOGI(LOG_TAG, "Found URL query => %s", buf);
      char param[32];
      /* Get value of expected key from query string */
      if (httpd_query_key_value(buf, "query1", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => query1=%s", param);
      }
      if (httpd_query_key_value(buf, "query3", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => query3=%s", param);
      }
      if (httpd_query_key_value(buf, "query2", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => query2=%s", param);
      }
    }
    free(buf);
  }

  /* Set some custom headers */
  httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
  httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

  /* Send response with custom headers and body set as the
   * string passed in user context*/
  const char *resp_str = (const char *)req->user_ctx;
  httpd_resp_send(req, resp_str, strlen(resp_str));

  /* After sending the HTTP response the old HTTP request
   * headers are lost. Check if HTTP request headers can be read now. */
  if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
    ESP_LOGI(LOG_TAG, "Request headers lost");
  }
  return ESP_OK;
}

httpd_uri_t route_hello = {.uri = "/hello",
                           .method = HTTP_GET,
                           .handler = hello_get_handler,
                           /* Let's pass response string in user
                            * context to demonstrate it's usage */
                           .user_ctx = "Hello World!"};

/* An HTTP POST handler */
esp_err_t echo_post_handler(httpd_req_t *req) {
  char buf[100];
  int ret, remaining = req->content_len;

  while (remaining > 0) {
    /* Read the data for the request */
    if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
      if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        /* Retry receiving if timeout occurred */
        continue;
      }
      return ESP_FAIL;
    }

    /* Send back the same data */
    httpd_resp_send_chunk(req, buf, ret);
    remaining -= ret;

    /* Log data received */
    ESP_LOGI(LOG_TAG, "=========== RECEIVED DATA ==========");
    ESP_LOGI(LOG_TAG, "%.*s", ret, buf);
    ESP_LOGI(LOG_TAG, "====================================");
  }

  // End response
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

httpd_uri_t route_echo = {.uri = "/echo",
                          .method = HTTP_POST,
                          .handler = echo_post_handler,
                          .user_ctx = NULL};

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
esp_err_t ctrl_put_handler(httpd_req_t *req) {
  char buf;
  int ret;

  if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  if (buf == '0') {
    /* Handler can be unregistered using the uri string */
    ESP_LOGI(LOG_TAG, "Unregistering /hello and /echo URIs");
    httpd_unregister_uri(req->handle, "/hello");
    httpd_unregister_uri(req->handle, "/echo");
  } else {
    ESP_LOGI(LOG_TAG, "Registering /hello and /echo URIs");
    httpd_register_uri_handler(req->handle, &route_hello);
    httpd_register_uri_handler(req->handle, &route_echo);
  }

  /* Respond with empty body */
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

httpd_uri_t route_ctrl = {.uri = "/ctrl",
                          .method = HTTP_PUT,
                          .handler = ctrl_put_handler,
                          .user_ctx = NULL};

esp_err_t req_handler_GET(httpd_req_t *req) {
  char *buf;
  size_t buf_len;

  /* Get header value string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    /* Copy null terminated value string into buffer */
    if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
      ESP_LOGI(LOG_TAG, "Found header => Host: %s", buf);
    }
    free(buf);
  }

  /* Read URL query string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      ESP_LOGI(LOG_TAG, "Found URL query => %s", buf);
      char param[32];
      /* Get value of expected key from query string */
      if (httpd_query_key_value(buf, "query1", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => query1=%s", param);
      }
      if (httpd_query_key_value(buf, "query3", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => query3=%s", param);
      }
      if (httpd_query_key_value(buf, "query2", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => query2=%s", param);
      }
    }
    free(buf);
  }

  if (request_callback)
    request_callback(req);

  FILE *f = fopen("/spiffs/web/index.html", "r");
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *data = malloc(fsize);
  fread(data, fsize, 1, f);

  /* http_response_begin(http_ctx, 200, "text/html",
   * HTTP_RESPONSE_SIZE_UNKNOWN); */
  /* const http_buffer_t buf = { */
  /*     .data = data, */
  /*     .data_is_persistent = true */
  /* }; */
  /* http_response_write(http_ctx, &buf); */
  /* http_response_end(http_ctx); */

  /* Set some custom headers */
  httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
  httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

  httpd_resp_set_hdr(req, "Content-Type", "text/html");
  httpd_resp_send(req, data, strlen(data));

  fclose(f);
  free(data);

  /* After sending the HTTP response the old HTTP request
   * headers are lost. Check if HTTP request headers can be read now. */
  if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
    ESP_LOGI(LOG_TAG, "Request headers lost");
  }
  return ESP_OK;
}

httpd_uri_t route_root = {.uri = "/",
                          .method = HTTP_GET,
                          .handler = req_handler_GET,
                          /* Let's pass response string in user
                           * context to demonstrate it's usage */
                          .user_ctx = NULL};

httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Start the httpd server
  ESP_LOGI(LOG_TAG, "Starting server on port: '%d'", config.server_port);

  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(LOG_TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &route_hello);
    httpd_register_uri_handler(server, &route_echo);
    httpd_register_uri_handler(server, &route_ctrl);
    httpd_register_uri_handler(server, &route_root);
    return server;
  }

  ESP_LOGI(LOG_TAG, "Error starting server!");
  return NULL;
}

esp_err_t stop_webserver(httpd_handle_t server) {
  // Stop the httpd server
  httpd_stop(server);

  return ESP_OK;
}

void network_set_callback(request_callback_t req_cb) {
  request_callback = req_cb;
}

static esp_err_t event_handler(void *ctx, system_event_t *event) {
  httpd_handle_t *server = (httpd_handle_t *)ctx;

  switch (event->event_id) {

  case SYSTEM_EVENT_STA_START:
    ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_STA_START");
    ESP_ERROR_CHECK(esp_wifi_connect());
    break;

  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_STA_GOT_IP");
    ESP_LOGI(LOG_TAG, "Got IP: '%s'",
             ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

    /* xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT); */

    /* Start the web server */
    if (*server == NULL) {
      *server = start_webserver();
    }

    break;

  case SYSTEM_EVENT_STA_DISCONNECTED:
    ESP_LOGI(LOG_TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
    ESP_ERROR_CHECK(esp_wifi_connect());

    /* xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT); */

    /* Stop the web server */
    if (*server) {
      stop_webserver(*server);
      *server = NULL;
    }

    break;

  default:
    break;
  }

  return ESP_OK;
}

esp_err_t init_wifi(void *arg) {
  tcpip_adapter_init();

  /* wifi_event_group = xEventGroupCreate(); */
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = NETWORK_SSID,
              .password = NETWORK_PSK,
          },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  /* xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, 0, 0,
   * portMAX_DELAY); */

  return ESP_OK;
}

void network_init() {
  ESP_LOGI(LOG_TAG, "init");

  /* ESP_ERROR_CHECK(nvs_flash_init()); */

  static httpd_handle_t server = NULL;

  ESP_ERROR_CHECK(init_wifi(&server));
}

void network_shutdown() {
  ESP_LOGI(LOG_TAG, "shutdown");

  // ESP_ERROR_CHECK(esp_wifi_stop());
  // ESP_ERROR_CHECK(esp_wifi_deinit());
  // vEventGroupDelete(wifi_event_group);
}
