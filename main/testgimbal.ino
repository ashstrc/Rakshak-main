#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include <ESP32Servo.h>

// ================= WIFI =================

const char* ssid = "Mohit";
const char* password = "mohit0604";

// ================= XIAO ESP32S3 SENSE CAMERA PINS =================

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1

#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15

#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// ================= GIMBAL =================

// Pan  -> GPIO 2
// Tilt -> GPIO 1

#define PAN_SERVO_PIN     2
#define TILT_SERVO_PIN    1

Servo panServo;
Servo tiltServo;

// Actual Arduino servo command limits
const int PAN_HOME  = 0;
const int PAN_MIN   = 0;
const int PAN_MAX   = 100;

const int TILT_HOME = 120;
const int TILT_MIN  = 70;
const int TILT_MAX  = 170;

// Current commanded positions
int currentPan  = PAN_HOME;
int currentTilt = TILT_HOME;

// ================= STREAM =================

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

static const char* STREAM_BOUNDARY =
  "\r\n--" PART_BOUNDARY "\r\n";

static const char* STREAM_PART =
  "Content-Type: image/jpeg\r\n"
  "Content-Length: %u\r\n\r\n";

// ================= GIMBAL HELPERS =================

void moveGimbal(int pan, int tilt) {

  // Enforce hard mechanical limits
  pan  = constrain(pan, PAN_MIN, PAN_MAX);
  tilt = constrain(tilt, TILT_MIN, TILT_MAX);

  currentPan  = pan;
  currentTilt = tilt;

  panServo.write(currentPan);
  tiltServo.write(currentTilt);

  Serial.printf(
    "Gimbal position -> PAN: %d | TILT: %d\n",
    currentPan,
    currentTilt
  );
}

void moveGimbalHome() {
  Serial.println("Returning gimbal to HOME position");

  currentPan  = PAN_HOME;
  currentTilt = TILT_HOME;

  panServo.write(PAN_HOME);
  tiltServo.write(TILT_HOME);
}

// ================= GIMBAL HTTP HANDLER =================

static esp_err_t gimbal_handler(httpd_req_t *req) {

  char query[128];

  int pan = currentPan;
  int tilt = currentTilt;

  if (httpd_req_get_url_query_len(req) > 0) {

    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > sizeof(query)) {
      query_len = sizeof(query);
    }

    if (httpd_req_get_url_query_str(
          req,
          query,
          query_len
        ) == ESP_OK) {

      char value[16];

      if (httpd_query_key_value(
            query,
            "pan",
            value,
            sizeof(value)
          ) == ESP_OK) {

        pan = atoi(value);
      }

      if (httpd_query_key_value(
            query,
            "tilt",
            value,
            sizeof(value)
          ) == ESP_OK) {

        tilt = atoi(value);
      }
    }
  }

  moveGimbal(pan, tilt);

  char response[128];

  snprintf(
    response,
    sizeof(response),
    "{\"status\":\"ok\",\"pan\":%d,\"tilt\":%d}",
    currentPan,
    currentTilt
  );

  httpd_resp_set_type(req, "application/json");

  return httpd_resp_send(
    req,
    response,
    HTTPD_RESP_USE_STRLEN
  );
}

// ================= HOME HANDLER =================

static esp_err_t home_handler(httpd_req_t *req) {

  moveGimbalHome();

  char response[128];

  snprintf(
    response,
    sizeof(response),
    "{\"status\":\"home\",\"pan\":%d,\"tilt\":%d}",
    currentPan,
    currentTilt
  );

  httpd_resp_set_type(req, "application/json");

  return httpd_resp_send(
    req,
    response,
    HTTPD_RESP_USE_STRLEN
  );
}

// ================= STREAM HANDLER =================

static esp_err_t stream_handler(httpd_req_t *req) {

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  char part_buf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);

  if (res != ESP_OK) {
    return res;
  }

  while (true) {

    fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }

    res = httpd_resp_send_chunk(
      req,
      STREAM_BOUNDARY,
      strlen(STREAM_BOUNDARY)
    );

    if (res == ESP_OK) {

      size_t hlen = snprintf(
        part_buf,
        64,
        STREAM_PART,
        fb->len
      );

      res = httpd_resp_send_chunk(
        req,
        part_buf,
        hlen
      );
    }

    if (res == ESP_OK) {

      res = httpd_resp_send_chunk(
        req,
        (const char *)fb->buf,
        fb->len
      );
    }

    esp_camera_fb_return(fb);

    if (res != ESP_OK) {
      break;
    }
  }

  return res;
}

// ================= START SERVER =================

void startCameraServer() {

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  config.server_port = 80;

  httpd_handle_t stream_httpd = NULL;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t gimbal_uri = {
    .uri       = "/gimbal",
    .method    = HTTP_GET,
    .handler   = gimbal_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t home_uri = {
    .uri       = "/gimbal/home",
    .method    = HTTP_GET,
    .handler   = home_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {

    httpd_register_uri_handler(
      stream_httpd,
      &stream_uri
    );

    httpd_register_uri_handler(
      stream_httpd,
      &gimbal_uri
    );

    httpd_register_uri_handler(
      stream_httpd,
      &home_uri
    );

    Serial.println("HTTP stream + gimbal server started");
  }
}

// ================= SETUP =================

void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("XIAO ESP32S3 Sense Camera + Gimbal");

  // -------- SERVO INITIALIZATION --------

  panServo.setPeriodHertz(50);
  tiltServo.setPeriodHertz(50);

  panServo.attach(PAN_SERVO_PIN, 500, 2400);
  tiltServo.attach(TILT_SERVO_PIN, 500, 2400);

  // Mandatory startup neutral position
  panServo.write(PAN_HOME);
  tiltServo.write(TILT_HOME);

  currentPan  = PAN_HOME;
  currentTilt = TILT_HOME;

  Serial.println("Gimbal initialized at HOME");
  Serial.println("PAN  = 0 degrees");
  Serial.println("TILT = 120 degrees");

  delay(1000);

  // -------- CAMERA CONFIG --------

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIXFORMAT_JPEG;

  // -------- RESOLUTION --------

  if (psramFound()) {

    Serial.println("PSRAM detected");

    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;

  } else {

    Serial.println("PSRAM NOT detected");

    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  // -------- INIT CAMERA --------

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.printf(
      "Camera init failed with error 0x%x\n",
      err
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println("Camera initialized");

  // -------- WIFI --------

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // -------- SERVER --------

  startCameraServer();

  Serial.println();
  Serial.println("Open this stream:");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream");

  Serial.println();
  Serial.println("Gimbal commands:");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/gimbal?pan=40&tilt=135");

  Serial.print("Home command: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/gimbal/home");
}

// ================= LOOP =================

void loop() {
  delay(1000);
}