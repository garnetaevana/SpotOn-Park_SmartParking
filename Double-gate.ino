#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

// --- LIBRARY FIREBASE ---
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>


// ==========================================
// 1. KONFIGURASI
// ==========================================
const char* ssid = "Chaaa";             
const char* password = "kka0y0lllllaa"; 

#define API_KEY "AIzaSyAqFcSWRj7qyEFVyfhCAaIyTdU7xaoNyk0"
#define FIREBASE_PROJECT_ID "spoton-park"
#define USER_EMAIL "sp@gmail.com"
#define USER_PASSWORD "admin123"

const char* ocrServer = "nevflix.pythonanywhere.com"; 
const char* ocrPath = "/ocr";                              
const int ocrPort = 443; 

// ==========================================
// 2. PINS & SERVO CONFIG
// ==========================================
const int trigPin = 13; 
const int echoPin = 12; 
#define flashLight 4 

// SERVO 1: GATE MASUK (Pin 4 / Flash)
// Karena Pin 4 dipakai Flash, servo ini akan gerak dikit saat flash nyala
// Sebaiknya pindah pin jika memungkinkan, tp jika tidak biarkan.
const int servoInPin = 14; 
Servo servoIn;

// SERVO 2: GATE KELUAR (Pin 14 atau 2)
const int servoOutPin = 15; 
Servo servoOut;

// Posisi Servo
int servoInClosed = 90; 
int servoInOpen = 0; 
int servoOutClosed = 39; 
int servoOutOpen = 131; 

// Gate Logic Vars
bool isGateOutOpen = false;
unsigned long gateOutOpenTime = 0;
const unsigned long gateOutDuration = 15000; // 15 Detik

// ==========================================

WebServer server(80);
WiFiClientSecure client; 
FirebaseData fbDO;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseReady = false;
bool isProcessing = false; 

unsigned long lastCaptureMillis = 0;
const unsigned long captureCooldown = 5000; 

const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 25200; 
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP, ntpServer, utcOffsetInSeconds);

// Camera Pins
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

// Declarations
String uploadToOCR(camera_fb_t* fb);
void saveLogToFirestore(String plate);
void openBarrierIn();
void closeBarrierIn();
void openBarrierOut();
void closeBarrierOut();
void checkSensor(); 
void checkGateCommand();
void updateGateStatusClosed();

void handleStream() {
  WiFiClient clientStream = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace;boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (true) {
    if (!clientStream.connected()) break;
    
    checkSensor();
    
    if (isGateOutOpen && (millis() - gateOutOpenTime >= gateOutDuration)) {
        closeBarrierOut();
        updateGateStatusClosed();
    }
    
    static unsigned long lastGateCheck = 0;
    if (millis() - lastGateCheck > 1000) { 
        checkGateCommand();
        lastGateCheck = millis();
    }
    
    if(isProcessing) { delay(100); continue; }

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) break;
    
    server.sendContent("--frame\r\nContent-Type: image/jpeg\r\n\r\n");
    clientStream.write(fb->buf, fb->len);
    server.sendContent("\r\n");
    esp_camera_fb_return(fb);
    delay(20); 
  }
}

void handleRoot() { server.send(200, "text/plain", "Smart Parking Active"); }

// --- CEK PERINTAH DARI FIRESTORE ---
void checkGateCommand() {
    if (!firebaseReady) return;

    // Set ukuran buffer respon agar tidak memakan memori berlebih
    fbDO.setResponseSize(1024); 

    String path = "gate_control/exit_gate"; 

    // --- DEBUG SISA MEMORI SEBELUM REQUEST ---
    Serial.print("[MEM] Free Heap sebelum request: ");
    Serial.println(ESP.getFreeHeap());

    if (Firebase.Firestore.getDocument(&fbDO, FIREBASE_PROJECT_ID, "", path.c_str(), "")) {
        Serial.print("HTTP CODE: ");
        Serial.println(fbDO.httpCode());

        if (fbDO.httpCode() == 200) {
            // GUNAKAN payload(), BUKAN jsonString()
            // payload() mengambil data mentah dari buffer HTTP
            String rawData = fbDO.payload(); 
            
            Serial.print("Panjang Data: ");
            Serial.println(rawData.length());
            Serial.println("Isi Data: [" + rawData + "]");
            
            rawData.toUpperCase();

            // Logika "Brute Force"
            if (rawData.indexOf("OPEN") != -1) {
                if (!isGateOutOpen) {
                    Serial.println(">>> TRIGGER: MEMBUKA GATE KELUAR! <<<");
                    updateGateStatusClosed(); 
                    openBarrierOut();
                }
            }
        } 
    } else {
        Serial.print("Error Koneksi: ");
        Serial.println(fbDO.errorReason());
    }
}

void updateGateStatusClosed() {
    if (!firebaseReady) return;
    String path = "gate_control/exit_gate"; 
    FirebaseJson updateContent;
    updateContent.set("fields/command/stringValue", "CLOSED");
    if (Firebase.Firestore.patchDocument(&fbDO, FIREBASE_PROJECT_ID, "", path.c_str(), updateContent.raw(), "command")){
      Serial.println("[FIREBASE DEBUG] PATCH CLOSED Berhasil.");
    } else {
        Serial.println("[FIREBASE DEBUG] PATCH CLOSED Gagal: " + fbDO.errorReason());
    }
}

void openBarrierOut() { 
    if(!isGateOutOpen) {
        servoOut.attach(servoOutPin);
        servoOut.write(servoOutOpen); 
        isGateOutOpen = true;
        gateOutOpenTime = millis(); 
        Serial.println("Gate Keluar TERBUKA");
    } else {
        gateOutOpenTime = millis(); 
    }
}

void closeBarrierOut() {
    if(isGateOutOpen) {
        servoOut.write(servoOutClosed); 
        delay(500); 
        servoOut.detach(); 
        isGateOutOpen = false;
        Serial.println("Gate Keluar TERTUTUP");
    }
}

void openBarrierIn() { servoIn.write(servoInOpen); }
void closeBarrierIn() { servoIn.write(servoInClosed); }

float measureDistanceCm() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  unsigned long duration = pulseIn(echoPin, HIGH, 25000); 
  if(duration == 0) return -1.0;
  return (duration * 0.034 / 2);
}

void checkSensor() {
    if (isProcessing || (millis() - lastCaptureMillis < captureCooldown)) return;
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead < 100) return;
    lastSensorRead = millis();

    float d = measureDistanceCm();
    if (d > 0 && d <= 18) {
        Serial.printf("\n[TRIGGER] Jarak %.2f cm\n", d);
        isProcessing = true; 
        lastCaptureMillis = millis();
        processCapture(); 
        isProcessing = false; 
    }
}

// --- PROSES CAPTURE DIPERBAIKI (Flash + High Res) ---
void processCapture() {
  servoIn.detach(); 
  camera_fb_t* fb = NULL;
  
  // 1. Nyalakan Flash agar terang
  digitalWrite(flashLight, HIGH);
  delay(200); // Tunggu cahaya stabil

  // 2. Buang frame lama (buffer cleaning)
  fb = esp_camera_fb_get(); 
  if(fb) esp_camera_fb_return(fb);
  
  // 3. Ambil frame baru (High Quality)
  fb = esp_camera_fb_get(); 
  
  // Matikan flash segera
  digitalWrite(flashLight, LOW); 
  
  if (!fb) { 
      Serial.println("Capture Failed"); 
      servoIn.attach(servoInPin); 
      return; 
  }

  Serial.printf("FOTO DIAMBIL. Ukuran: %d bytes. Uploading to OCR...\n", fb->len);
  String plate = uploadToOCR(fb);
  
  if (plate.length() > 0) {
    Serial.println(">>> PLAT DETECTED: " + plate);
    saveLogToFirestore(plate);
    servoIn.attach(servoInPin);
    openBarrierIn();
    delay(10000); 
    closeBarrierIn();
  } else {
    Serial.println(">>> No Plate Found (Cek Cahaya/Fokus)");
    servoIn.attach(servoInPin);
  }
  esp_camera_fb_return(fb);
}

String getISO8601Time() {
  time_t now = time(nullptr);
  struct tm* ptm = localtime(&now);
  char buf[30];
  // Menggunakan %z untuk offset zona waktu (contoh: +0700)
  strftime(buf, 30, "%Y-%m-%dT%H:%M:%S%z", ptm); 
  return String(buf);
}
void saveLogToFirestore(String plate) {
  if (!firebaseReady) return;
  time_t now = time(nullptr);
  Serial.print("Saving DB...");
  String docPath = "parking_logs/log_" + String(now);
  FirebaseJson content;
  content.set("fields/plateNumber/stringValue", plate);
  content.set("fields/photoUrl/stringValue", "https://via.placeholder.com/150?text=No+Image"); 
  content.set("fields/entryTime/stringValue", getISO8601Time());
  content.set("fields/status/stringValue", "Masuk");
  content.set("fields/paymentStatus/stringValue", "unpaid");
  fbDO.setResponseSize(512);
  if (Firebase.Firestore.createDocument(&fbDO, FIREBASE_PROJECT_ID, "", docPath.c_str(), content.raw())) {
    Serial.println(" OK!");
  } else {
    Serial.println(" Fail: " + fbDO.errorReason());
  }
}

String uploadToOCR(camera_fb_t* fb) {
  if (!fb) return "";
  client.setInsecure(); 
  client.setNoDelay(true);
  client.setTimeout(10000); 
  if (!client.connect(ocrServer, ocrPort)) {
      Serial.println("Gagal Konek ke Server OCR!");
      return "";
  }
  
  String boundary = "------------------------ESP32Boundary";
  String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  uint32_t totalLen = head.length() + fb->len + tail.length();

  client.println("POST " + String(ocrPath) + " HTTP/1.0");
  client.println("Host: " + String(ocrServer));
  client.println("Content-Length: " + String(totalLen));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.println("Connection: close");
  client.println();
  client.print(head);
  
  uint8_t *fbBuf = fb->buf;
  size_t fbLen = fb->len;
  for (size_t n = 0; n < fbLen; n = n + 2048) {
    if (n + 2048 < fbLen) { client.write(fbBuf, 2048); fbBuf += 2048; }
    else if (fbLen % 2048 > 0) { size_t remainder = fbLen % 2048; client.write(fbBuf, remainder); }
  }
  client.print(tail);

  String response = "";
  long start = millis();
  while (client.connected() || client.available()) {
    if (client.available()) { char c = client.read(); response += c; }
    if(millis() - start > 20000) break;
  }
  client.stop();
  
  // DEBUG RESPONSE DARI SERVER
  // Serial.println("Raw Response: " + response); // Uncomment jika ingin lihat isi balasan server

  int idx = response.indexOf("\"plate\"");
  if (idx != -1) {
      int startQuote = response.indexOf("\"", idx + 8); 
      int endQuote = response.indexOf("\"", startQuote + 1);
      return response.substring(startQuote + 1, endQuote);
  }
  return ""; 
}


#define TZ_INFO "WIB-7"
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  
  pinMode(flashLight, OUTPUT); digitalWrite(flashLight, LOW);
  pinMode(trigPin, OUTPUT); pinMode(echoPin, INPUT);
  
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  
  servoIn.setPeriodHertz(50);
  servoIn.attach(servoInPin, 1000, 2000);
  servoIn.write(servoInClosed);

  servoOut.setPeriodHertz(50);
  servoOut.attach(servoOutPin, 1000, 2000);
  servoOut.write(servoOutClosed);
  delay(500);
  servoOut.detach(); 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK. IP: "); Serial.println(WiFi.localIP());

  configTime(utcOffsetInSeconds, 0, ntpServer); 

  Serial.print("Waiting for time sync...");
  time_t now = time(nullptr);
  // Tunggu hingga waktu disinkronkan
  while (now < 10000) { 
      delay(500);
      Serial.print(".");
      now = time(nullptr);
  }

  // SET TIME ZONE (menggunakan fungsi yang sudah ada)
  configTzTime(TZ_INFO, ntpServer);

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  Serial.println("\nTime Synced: " + String(asctime(&timeinfo)));

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  firebaseReady = true;

  camera_config_t configCam;
  configCam.ledc_channel = LEDC_CHANNEL_0;
  configCam.ledc_timer = LEDC_TIMER_0;
  configCam.pin_d0 = Y2_GPIO_NUM;
  configCam.pin_d1 = Y3_GPIO_NUM;
  configCam.pin_d2 = Y4_GPIO_NUM;
  configCam.pin_d3 = Y5_GPIO_NUM;
  configCam.pin_d4 = Y6_GPIO_NUM;
  configCam.pin_d5 = Y7_GPIO_NUM;
  configCam.pin_d6 = Y8_GPIO_NUM;
  configCam.pin_d7 = Y9_GPIO_NUM;
  configCam.pin_xclk = XCLK_GPIO_NUM;
  configCam.pin_pclk = PCLK_GPIO_NUM;
  configCam.pin_vsync = VSYNC_GPIO_NUM;
  configCam.pin_href = HREF_GPIO_NUM;
  configCam.pin_sscb_sda = SIOD_GPIO_NUM;
  configCam.pin_sscb_scl = SIOC_GPIO_NUM;
  configCam.pin_pwdn = PWDN_GPIO_NUM;
  configCam.pin_reset = RESET_GPIO_NUM;
  configCam.xclk_freq_hz = 20000000;
  configCam.pixel_format = PIXFORMAT_JPEG;
  
  // RESOLUSI DITINGKATKAN KE SVGA UNTUK OCR YANG LEBIH BAIK
  if(psramFound()){
    Serial.println("PSRAM ON. Setting CIF (400x296)");
    configCam.frame_size = FRAMESIZE_CIF; 
    configCam.jpeg_quality = 12; // Kualitas tinggi (angka kecil = bagus)
    configCam.fb_count = 1;
  } else {
    Serial.println("No PSRAM. Setting QVGA (640x480)");
    configCam.frame_size = FRAMESIZE_QVGA;
    configCam.jpeg_quality = 12;
    configCam.fb_count = 1;
  }
  esp_camera_init(&configCam);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  
  server.begin();
  Serial.println("System Ready.");
}


void loop() {
  server.handleClient(); 
  
  if (isGateOutOpen && (millis() - gateOutOpenTime >= gateOutDuration)) {
      closeBarrierOut();
      updateGateStatusClosed(); 
  }
  
  static unsigned long lastGateCheck = 0;
  if (millis() - lastGateCheck > 1000) { 
      checkGateCommand();
      lastGateCheck = millis();
  }

  checkSensor();
  delay(10);
}
