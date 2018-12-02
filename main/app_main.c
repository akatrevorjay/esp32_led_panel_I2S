#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "graphics.h"
#include "network.h"
#include <math.h>
#include <string.h>

#include <nvs_flash.h>

static const char *LOG_TAG = "app_main";

static module_t module;
static sampler_t sampler;

void module_gif(vec2 *uv, vec4 *out, sampler_t *sampler) {
  sample(sampler, *uv, (vec3 *)out);
}

void module_solid(vec2 *uv, vec4 *out, sampler_t *sampler) { out->z = 1.0f; }

float _k;

void module_sphere(vec2 *uv, vec4 *out, sampler_t *sampler) {
  vec2 cuv = {uv->x * 2.0f - 1.0f, uv->y * 2.0f - 1.0f};
  cuv.x *= 2.0f;
  cuv.x -= sin(_k += 0.00001f);
  out->x = length2(cuv);
}

void module_plasma(vec2 *uv, vec4 *out, sampler_t *sampler) {
  _k += 0.0001f;
  float iTime = _k;
  // v1
  float v1 = sin(uv->x * 5.0 + iTime);

  // v2
  float v2 =
      sin(5.0 * (uv->x * sin(iTime / 2.0) + uv->y * cos(iTime / 3.0)) + iTime);

  // v3
  float cx = uv->x + sin(iTime / 5.0) * 5.0;
  float cy = uv->y + sin(iTime / 3.0) * 5.0;
  float v3 = sin(sqrt(100.0 * (cx * cx + cy * cy)) + iTime);

  float vf = v1 + v2 + v3;
  float r = cos(vf * M_PI);
  float g = sin(vf * M_PI + 6.0 * M_PI / 3.0);
  float b = cos(vf * M_PI + 4.0 * M_PI / 3.0);

  out->x = r;
  out->y = g;
  out->z = b;
}

esp_err_t load_gif(char *param) {
  ESP_LOGI("Main", "requested file: %s", param);

  strcpy(sampler.file, param);
  sampler.loop = true;

  module.fn = module_gif;
  module.sampler = &sampler;
  sampler.anim_speed = 16;

  graphics_run(&module);

  return ESP_OK;
}

static int parse_hex_digit(char hex) {
  switch (hex) {
  case '0' ... '9':
    return hex - '0';
  case 'a' ... 'f':
    return hex - 'a' + 0xa;
  case 'A' ... 'F':
    return hex - 'A' + 0xA;
  default:
    return -1;
  }
}

static char *urldecode(const char *str, size_t len) {
  ESP_LOGV(LOG_TAG, "urldecode: '%.*s'", len, str);

  const char *end = str + len;
  char *out = calloc(1, len + 1);
  char *p_out = out;
  while (str != end) {
    char c = *str++;
    if (c != '%') {
      *p_out = c;
    } else {
      if (str + 2 > end) {
        /* Unexpected end of string */
        return NULL;
      }
      int high = parse_hex_digit(*str++);
      int low = parse_hex_digit(*str++);
      if (high == -1 || low == -1) {
        /* Unexpected character */
        return NULL;
      }
      *p_out = high * 16 + low;
    }
    ++p_out;
  }
  *p_out = 0;
  ESP_LOGV(LOG_TAG, "urldecode result: '%s'", out);
  return out;
}

esp_err_t network_request(httpd_req_t *req) {
  ESP_LOGI(LOG_TAG, "network_request");

  /* const char* arg_load = http_request_get_arg_value(*ctx, "load_gif"); */
  /* if (arg_load != NULL) { */
  /*     ESP_LOGI("Main", "requested file: %s", arg_load); */
  /*  */
  /*     strcpy(sampler.file, arg_load); */
  /*     sampler.loop = true; */
  /*  */
  /*     module.fn = module_gif; */
  /*     module.sampler = &sampler; */
  /*     sampler.anim_speed = 16; */
  /*  */
  /*     graphics_run(&module); */
  /* } */

  ESP_LOGI(LOG_TAG, "network_request: Checking query params");

  char *buf;
  size_t buf_len;

  /* Read URL query string length and allocate memory for length + 1,
   * extra byte for null termination */
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = malloc(buf_len);

    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      ESP_LOGI(LOG_TAG, "Found URL query => %s", buf);
      char param[32];

      /* Get value of expected key from query string */
      if (httpd_query_key_value(buf, "load_gif", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => module=%s", param);

        char *pbuf = urldecode(param, strlen(param));

        ESP_ERROR_CHECK(load_gif(pbuf));

        free(pbuf);

        return ESP_OK;
      }

      if (httpd_query_key_value(buf, "anim_speed", param, sizeof(param)) ==
          ESP_OK) {
        ESP_LOGI(LOG_TAG, "Found URL query parameter => anim_speed=%s", param);

        char *pbuf = urldecode(param, strlen(param));

        sampler.anim_speed = atoi(pbuf);

        free(pbuf);

        return ESP_OK;
      }

      /* if (httpd_query_key_value(buf, "query2", param, sizeof(param)) ==
       * ESP_OK) { */
      /*     ESP_LOGI(LOG_TAG, "Found URL query parameter => query2=%s", param);
       */
      /* } */
    }

    free(buf);
  }

  /* char buf[100]; */
  /* int ret, remaining = req->content_len; */
  /*  */
  /* while (remaining > 0) { */
  /*     #<{(| Read the data for the request |)}># */
  /*     if ((ret = httpd_req_recv(req, buf, */
  /*                     MIN(remaining, sizeof(buf)))) <= 0) { */
  /*         if (ret == HTTPD_SOCK_ERR_TIMEOUT) { */
  /*             #<{(| Retry receiving if timeout occurred |)}># */
  /*             continue; */
  /*         } */
  /*         return ESP_FAIL; */
  /*     } */
  /*  */
  /*     #<{(| Send back the same data |)}># */
  /*     httpd_resp_send_chunk(req, buf, ret); */
  /*     remaining -= ret; */
  /*  */
  /*     #<{(| Log data received |)}># */
  /*     ESP_LOGI(LOG_TAG, "=========== RECEIVED DATA =========="); */
  /*     ESP_LOGI(LOG_TAG, "%.*s", ret, buf); */
  /*     ESP_LOGI(LOG_TAG, "===================================="); */
  /* } */

  /* const char* arg_anim_speed = http_request_get_arg_value(*ctx,
   * "anim_speed"); */
  /* if (arg_anim_speed != NULL) { */
  /*     ESP_LOGI("Main", "requested animation speed: %s", arg_anim_speed); */
  /*  */
  /*     sampler.anim_speed = atoi(arg_anim_speed); */
  /* } */

  // End response
  /* httpd_resp_send_chunk(req, NULL, 0); */

  ESP_LOGI(LOG_TAG, "network_request: ok");

  return ESP_OK;
}

void app_main() {
  esp_err_t ret = nvs_flash_init();
  // if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
  //     ESP_ERROR_CHECK(nvs_flash_erase());
  //     ret = nvs_flash_init();
  // }
  ESP_ERROR_CHECK(ret);

  graphics_init();

  /* module.fn = module_plasma; */
  /* sampler.anim_speed = 16; */
  /*  */
  /* graphics_run(&module); */

  network_init();
  network_set_callback(network_request);

  vTaskDelay(50000000 / portTICK_PERIOD_MS);

  graphics_shutdown();
  network_shutdown();
}
