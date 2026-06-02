#include "esp_camera.h"
#include "esp_http_server.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <SPI.h>
#include <WiFi.h>

const char *ssid = "Mikan Phone";
const char *password = "20102005";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define SPI_SCK_PIN 14
#define SPI_MOSI_PIN 13
#define SPI_SS_PIN 15

SPIClass *hspi = NULL;

const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FPV Control</title>
<style>
body { font-family: sans-serif; 
  background-color: #1a1a1a; 
  color: white; 
  text-align: center; 
  margin: 0; 
  padding: 10px; 
}

.main-layout { 
  display: flex; 
  align-items: center; 
  justify-content: center; 
  gap: 10px; 
  max-width: 600px; 
  margin: 0 auto 20px auto; 
}

.video-container { 
  flex-grow: 1; 
  height: 300px; 
  background-color: #222; 
  border: 3px solid #ffeb3b; 
  border-radius: 8px; 
  overflow: hidden; 
  display: flex; 
  align-items: center; 
  justify-content: center; 
}

#video-stream {   
  width: 100%; 
  height: 100%; 
  object-fit: cover; 
}

.side-btn { 
  background-color: #1976d2; 
  color: white; 
  border: none; 
  border-radius: 8px; 
  width: 60px; 
  height: 120px; 
  font-size: 24px; 
  font-weight: bold; 
  cursor: pointer; 
  user-select: none; 
  touch-action: manipulation; 
}
.side-btn:active { background-color: #1565c0; }

.controls { 
  display: grid; 
  grid-template-columns: repeat(3, 80px); 
  gap: 15px; 
  justify-content: center; 
  margin-top: 10px; 
}
.btn { 
  background-color: #e53935; 
  color: white; 
  border: none; 
  border-radius: 10px; 
  width: 80px; 
  height: 80px; 
  font-size: 24px; 
  font-weight: bold; 
  cursor: pointer; 
  user-select: none; 
  touch-action: manipulation; 
}
.btn:active { background-color: #b71c1c; }

.up { grid-column: 2; grid-row: 1; }
.left { grid-column: 1; grid-row: 2; }
.down { grid-column: 2; grid-row: 2; }
.right { grid-column: 3; grid-row: 2; }

.util-controls { 
  display: flex; 
  justify-content: center; 
  gap: 20px; 
  margin-bottom: 20px; 
}
.util-btn { 
  background-color: #4CAF50; 
  color: white; 
  border: none; 
  border-radius: 8px; 
  padding: 12px 24px; 
  font-size: 18px; 
  font-weight: bold; 
  cursor: pointer; 
  user-select: none; 
  touch-action: manipulation; 
}
.util-btn.pause { background-color: #ff9800; }
.util-btn:active { filter: brightness(0.8); }

.speed-control { 
  margin: 15px auto; 
  max-width: 400px; 
  background-color: #222; 
  padding: 10px 20px; 
  border-radius: 8px; 
  border: 2px solid #ffeb3b; 
}
.speed-control label { 
  display: block; 
  font-size: 18px; 
  font-weight: bold; 
  margin-bottom: 8px; 
  color: #ffeb3b; 
  text-align: left;
}
input[type=range] { 
  width: 100%; 
  accent-color: #e53935; 
  height: 8px;
  outline: none;
}
</style>
</head>
<body>

<div class="util-controls">
  <button class="util-btn" onmousedown="s('C')" ontouchstart="s('C')">CONNECT (C)</button>
  <button class="util-btn pause" onmousedown="s('P')" ontouchstart="s('P')">PAUSE (P)</button>
</div>

<div class="main-layout">
  <button class="side-btn" onmousedown="s('L')" onmouseup="s('X')" ontouchstart="s('L')" ontouchend="s('X')">L</button>
  
  <div class="video-container">
    <img id="video-stream" src="" alt="Đang kết nối camera...">
  </div>
  
  <button class="side-btn" onmousedown="s('R')" onmouseup="s('X')" ontouchstart="s('R')" ontouchend="s('X')">R</button>
</div>

<div class="speed-control">
  <label for="speedSlider">SPEED: <span id="speedValue">0</span></label>
  <input type="range" id="speedSlider" min="0" max="10" value="0" oninput="updateSpeed(this.value)">
</div>

<div class="controls">
  <button class="btn up" onmousedown="s('W')" onmouseup="s('X')" ontouchstart="s('W')" ontouchend="s('X')">W</button>
  <button class="btn left" onmousedown="s('A')" onmouseup="s('X')" ontouchstart="s('A')" ontouchend="s('X')">A</button>
  <button class="btn down" onmousedown="s('S')" onmouseup="s('X')" ontouchstart="s('S')" ontouchend="s('X')">S</button>
  <button class="btn right" onmousedown="s('D')" onmouseup="s('X')" ontouchstart="s('D')" ontouchend="s('X')">D</button>
</div>

</div>

<script>
function s(cmd) {
  fetch(`/action?go=${cmd}`);
}

function updateSpeed(val) {
  document.getElementById('speedValue').innerText = val;
  const chars = ['0','1','2','3','4','5','6','7','8','9','M'];
  s(chars[val]);
}

document.addEventListener('keydown', function(event) {
  if (event.repeat) return; 
  const key = event.key.toUpperCase();
  switch(key) {
    case 'W': s('W'); break; 
    case 'A': s('A'); break; 
    case 'S': s('S'); break; 
    case 'D': s('D'); break; 
    case 'L': s('L'); break; 
    case 'R': s('R'); break; 
    case 'C': s('C'); break; 
    case 'P': s('P'); break; 
    case '0': document.getElementById('speedSlider').value = 0; updateSpeed(0); break;
    case '1': document.getElementById('speedSlider').value = 1; updateSpeed(1); break;
    case '2': document.getElementById('speedSlider').value = 2; updateSpeed(2); break;
    case '3': document.getElementById('speedSlider').value = 3; updateSpeed(3); break;
    case '4': document.getElementById('speedSlider').value = 4; updateSpeed(4); break;
    case '5': document.getElementById('speedSlider').value = 5; updateSpeed(5); break;
    case '6': document.getElementById('speedSlider').value = 6; updateSpeed(6); break;
    case '7': document.getElementById('speedSlider').value = 7; updateSpeed(7); break;
    case '8': document.getElementById('speedSlider').value = 8; updateSpeed(8); break;
    case '9': document.getElementById('speedSlider').value = 9; updateSpeed(9); break;
    case 'M': document.getElementById('speedSlider').value = 10; updateSpeed(10); break;
  }
});
document.addEventListener('keyup', function(event) {
  const key = event.key.toUpperCase();
  if (['W', 'A', 'S', 'D', 'L', 'R'].includes(key)) s('X');
});

window.onload = function() {
  var loc = window.location;
  document.getElementById("video-stream").src = loc.protocol + "//" + loc.hostname + ":81/stream";
};
    </script>
  </body>
</html>
)rawliteral";

void sendDataSPI(char data) {
  Serial.print("[SPI TX] '");
  Serial.print(data);
  Serial.print("' -> ");
  switch (data) {
  case 'W':
    Serial.println("Forward");
    break;
  case 'S':
    Serial.println("Backward");
    break;
  case 'A':
    Serial.println("Turn Left");
    break;
  case 'D':
    Serial.println("Turn Right");
    break;
  case 'L':
    Serial.println("Camera Left");
    break;
  case 'R':
    Serial.println("Camera Right");
    break;
  case 'C':
    Serial.println("Connect (STBY H)");
    break;
  case 'P':
    Serial.println("Pause (STBY L)");
    break;
  case 'X':
    Serial.println("Stop");
    break;
  case 'M':
    Serial.println("Speed MAX");
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    Serial.print("Speed ");
    Serial.println(data);
    break;
  default:
    Serial.print("Unknown (0x");
    Serial.print((uint8_t)data, HEX);
    Serial.println(")");
    break;
  }

  hspi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_SS_PIN, LOW);
  delayMicroseconds(50);
  hspi->transfer(data);
  delayMicroseconds(50);
  digitalWrite(SPI_SS_PIN, HIGH);
  hspi->endTransaction();
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

esp_err_t action_handler(httpd_req_t *req) {
  char buf[100];
  esp_err_t ret = httpd_req_get_url_query_str(req, buf, sizeof(buf));
  if (ret == ESP_OK) {
    char param[32];
    if (httpd_query_key_value(buf, "go", param, sizeof(param)) == ESP_OK) {
      char cmd_char = param[0];
      sendDataSPI(cmd_char);
    }
  }
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY,
                                  strlen(_STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (res == ESP_OK) {
      break;
    }

    if (res != ESP_OK) {
      break;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {.uri = "/",
                           .method = HTTP_GET,
                           .handler = index_handler,
                           .user_ctx = NULL};

  httpd_uri_t action_uri = {.uri = "/action",
                            .method = HTTP_GET,
                            .handler = action_handler,
                            .user_ctx = NULL};

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &action_uri);
    Serial.println("Server điều khiển đã khởi động (port 80)");
  }

  httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
  stream_config.server_port = 81;
  stream_config.ctrl_port = 32769; 

  httpd_uri_t stream_uri = {.uri = "/stream",
                            .method = HTTP_GET,
                            .handler = stream_handler,
                            .user_ctx = NULL};

  if (httpd_start(&stream_httpd, &stream_config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("Server stream đã khởi động (port 81)");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.println("\nĐang khởi động hệ thống...");

  pinMode(SPI_SS_PIN, OUTPUT);
  digitalWrite(SPI_SS_PIN, HIGH); 

  hspi = new SPIClass(HSPI);
  hspi->begin(SPI_SCK_PIN, -1, SPI_MOSI_PIN, SPI_SS_PIN);
  delay(10); 

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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QQVGA; 
    config.jpeg_quality = 25; 
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 25;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Lỗi Camera: 0x%x (Camera không hoạt động, nhưng SPI và WiFi "
                  "vẫn chạy)\n",
                  err);
  } else {
    Serial.println("Camera OK!");
  }

  delay(1000);

  WiFi.mode(WIFI_STA); 

  WiFi.begin(ssid, password);

  Serial.print("Đang kết nối vào WiFi: ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nĐã kết nối WiFi thành công!");
  Serial.print("Địa chỉ IP của Camera là: http://");
  Serial.println(WiFi.localIP()); 

  startCameraServer();
}

void loop() {
  delay(1000);
}
