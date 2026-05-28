//Thêm thư viện
#include "esp_camera.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>

//Cấu hình Wifi từ điện thoại
const char *ssid = "Mikan Phone"; //Tên wifi
const char *password = "20102005"; //Mật khẩu wifi

//Khởi tạo WebServer
WebServer server(80);

//Khai báo chân cho ESP CAM
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

//Khai báo chân để kết nối Mega128 bằng giao thức SPI
#define SPI_SCK_PIN 14
#define SPI_MOSI_PIN 13
#define SPI_SS_PIN 15

//Khởi tạo con trỏ SPIClass cho bộ HSPI
SPIClass *hspi = NULL;

//Giao diện Web
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

/* Bố cục hàng ngang chứa 2 nút LR và Camera */
.main-layout { 
  display: flex; 
  align-items: center; 
  justify-content: center; 
  gap: 10px; 
  max-width: 600px; 
  margin: 0 auto 20px auto; 
}

/* Khung video sẽ tự co giãn lấp đầy không gian giữa 2 nút */
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

/* Nút xoay 2 bên (hình chữ nhật đứng) */
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

/* Cụm điều hướng W-A-S-D bên dưới */
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

/* Các nút chức năng (Connect, Pause) */
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
/* Speed Control */
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
// gửi lệnh điều khiển
function s(cmd) {
  fetch(`/action?go=${cmd}`);
}

// cập nhật tốc độ
function updateSpeed(val) {
  document.getElementById('speedValue').innerText = val;
  // Gửi ký tự '0'-'9' cho tốc độ 0-9, và 'M' (Max) cho tốc độ 10
  const chars = ['0','1','2','3','4','5','6','7','8','9','M'];
  s(chars[val]);
}

// Điều khiển bằng bàn phím
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
    // Phím tắt tốc độ
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

// tải ảnh thông minh không bị nghẽn
window.onload = function() {
  const img = document.getElementById("video-stream");
  function fetchNextFrame() {
    img.src = `/capture?_cb=` + Date.now();
  }
  // Khi ảnh cũ đã tải xong -> lập tức gọi ảnh mới không cần chờ
  img.onload = function() {
    requestAnimationFrame(fetchNextFrame);
  };
  // Nếu mạng lag bị đứt ảnh, chờ một chút rồi kết nối lại
  img.onerror = function() {
    setTimeout(fetchNextFrame, 500);
  };
  // Kích mồi phát đầu tiên
  fetchNextFrame();
};
    </script>
  </body>
</html>
)rawliteral";

//Hàm gửi 1 ký tự đến Mega128-V2 qua giao thức SPI
void sendDataSPI(char data) {
  //Bắt đầu giao thức SPI với tốc độ 1MHz an toàn cho Mega128-V2, truyền bit cao (MSB) trước
  hspi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  //Kéo Slave Select xuống LOW để báo Mega128-V2 chuẩn bị nhận
  digitalWrite(SPI_SS_PIN,LOW); 
      
  //Delay nhỏ chuẩn bị đường truyền (Fix nhiễu SPI)
  delayMicroseconds(50);          
  
  //Đẩy dữ liệu ra chân MOSI
  hspi->transfer(data);           
  
  //Delay đảm bảo Mega128 đã đọc xong
  delayMicroseconds(50);          
  
  //Kéo Slave Select lên HIGH chốt dữ liệu
  digitalWrite(SPI_SS_PIN, HIGH); 

  //Kết thúc giao thức SPI
  hspi->endTransaction();
}

void setup() {
  //Tắt tính năng kiểm tra sụt áp hệ thống, không bị ngắt chương trình hoạt động dù camera đã Ok !
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

  Serial.begin(115200);
  Serial.println("\nĐang khởi động hệ thống...");

  //Khai báo và khởi tạo SPI
  pinMode(SPI_SS_PIN, OUTPUT);
  digitalWrite(SPI_SS_PIN, HIGH); //Giữ chân SS ở mức cao (chưa xác định chip nhận)

  hspi = new SPIClass(HSPI);
  //Khởi tạo SPI: SCK, MISO (-1 vì chỉ truyền 1 chiều), MOSI, SS
  hspi->begin(SPI_SCK_PIN, -1, SPI_MOSI_PIN, SPI_SS_PIN);
  delay(10); //Dừng lại 10ms cho chân SPI ổn định

  //Cấu hình Camera
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
    config.frame_size = FRAMESIZE_QQVGA; //Đổi xuống độ phân giải thấp nhất (160x120)
    config.jpeg_quality = 25;            //Ảnh mờ nhưng dung lượng siêu nhỏ nên cho video khá mượt
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 25;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Lỗi Camera: 0x%x\n", err);
    return;
  }
  Serial.println("Camera OK!");

  //Thêm một khoảng trễ nhỏ để ổn định điện áp sau khi khởi động Camera
  //Việc khởi động WiFi ngay lập tức sẽ tạo ra mức tiêu thụ dòng điện rất cao làm sụt áp
  delay(1000);

  //Kết nối vào WiFi hotspot
  WiFi.mode(WIFI_STA); //Đặt mạch về chế độ Station (Nhận sóng Wifi từ thiết bị khác)
  
  WiFi.begin(ssid, password);

  //Hạ công suất phát wifi tránh lỗi sụt áp...
  //(Tạm thời comment lại vì sóng quá yếu có thể là nguyên nhân không bắt được WiFi)
  //WiFi.setTxPower(WIFI_POWER_8_5dBm);

  Serial.print("Đang kết nối vào WiFi: ");
  Serial.println(ssid);

  //Vòng lặp chờ kết nối thành công
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nĐã kết nối WiFi thành công!");
  Serial.print("Địa chỉ IP của Camera là: http://");
  Serial.println(WiFi.localIP()); //Xem dòng này trên Serial Monitor để biết địa chỉ IP dành cho Camera

  //Định tuyến serverweb
  server.on("/", []() { server.send(200, "text/html", INDEX_HTML); });

  //Nhận lệnh điều khiển từ web và phát qua SPI
  server.on("/action", []() {
    if (server.hasArg("go")) {
      String cmd_str = server.arg("go");
      char cmd_char = cmd_str.charAt(0); //Lấy ký tự lệnh (W, A, S, D, X, 0, 1)

      sendDataSPI(cmd_char); //Kích hoạt truyền SPI

      Serial.print("SPI Out: ");
      Serial.println(cmd_char);
    }
    server.send(200, "text/plain", "OK");
  });

  //Capture và gửi 1 frame Camera
  server.on("/capture", []() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      server.send(500, "text/plain", "Loi chup anh");
      return;
    }

    server.setContentLength(fb->len);
    server.send(200, "image/jpeg", "");
    server.client().write(fb->buf, fb->len);

    esp_camera_fb_return(fb);
  });

  server.begin();
}

void loop() { server.handleClient(); }