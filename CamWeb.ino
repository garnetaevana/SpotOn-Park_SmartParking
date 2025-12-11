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
const char* ssid = "UGM-Hotspot";             
const char* password = ""; 

#define API_KEY "AIzaSyAqFcSWRj7qyEFVyfhCAaIyTdU7xaoNyk0"
#define FIREBASE_PROJECT_ID "spoton-park"
#define USER_EMAIL "sp@gmail.com"
#define USER_PASSWORD "admin123"

const char* ocrServer = "nevflix.pythonanywhere.com"; 
const char* ocrPath = "/ocr";                              
const int ocrPort = 443; 

// Pins
const int trigPin = 13; 
const int echoPin = 12; 
const int servoPin = 4; 
#define flashLight 4 

WebServer server(80);
WiFiClientSecure client; 
Servo myservo;
int servoClosed = 90; 
int servoOpen = 45;

FirebaseData fbDO;
FirebaseAuth auth;
FirebaseConfig config;
bool firebaseReady = false;
bool isProcessing = false; // Flag agar tidak double process

unsigned long lastCaptureMillis = 0;
const unsigned long captureCooldown = 5000; 

// NTP
const char* ntpServer = "pool.ntp.org";
const long utcOffsetInSeconds = 25200; 
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, utcOffsetInSeconds);

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

String uploadToOCR(camera_fb_t* fb);
void saveLogToFirestore(String plate);
void openBarrier();
void closeBarrier();
void checkSensor(); // Deklarasi fungsi baru

// --- STREAMING YANG BISA DIPUTUS (NON-BLOCKING) ---
void handleStream() {
  WiFiClient clientStream = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace;boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (true) {
    // Cek apakah klien putus
    if (!clientStream.connected()) break;
    
    // Cek sensor setiap frame! Agar tidak buta saat streaming
    checkSensor();
    
    // Jika sedang proses capture (ada mobil), hentikan stream sebentar
    if(isProcessing) {
        delay(100); // Beri waktu proses capture jalan
        continue;   // Skip kirim frame
    }

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) break;
    
    server.sendContent("--frame\r\nContent-Type: image/jpeg\r\n\r\n");
    clientStream.write(fb->buf, fb->len);
    server.sendContent("\r\n");
    esp_camera_fb_return(fb);
    
    // Delay kecil agar sensor punya kesempatan dibaca
    delay(20); 
  }
}

void handleRoot() { server.send(200, "text/plain", "System Ready"); }

// --- LOGIKA UTAMA ---
float measureDistanceCm() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  unsigned long duration = pulseIn(echoPin, HIGH, 25000); 
  if(duration == 0) return -1.0;
  return (duration * 0.034 / 2);
}

void checkSensor() {
    // Jika sedang cooldown atau sedang proses, abaikan
    if (isProcessing || (millis() - lastCaptureMillis < captureCooldown)) return;

    // Batasi pembacaan sensor setiap 100ms agar tidak spam
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead < 100) return;
    lastSensorRead = millis();

    float d = measureDistanceCm();
    
    // Debug Sensor (Tampilkan setiap 2 detik biar tau hidup)
    static unsigned long lastDebug = 0;
    if(millis() - lastDebug > 2000) {
        Serial.printf("Sensor Check: %.2f cm\n", d);
        lastDebug = millis();
    }

    // TRIGGER CAPTURE
    if (d > 0 && d <= 10) {
        Serial.printf("\n[TRIGGER] Object Detected at %.2f cm!\n", d);
        isProcessing = true; // Set flag sibuk
        lastCaptureMillis = millis();
        
        // Panggil fungsi proses (ini akan memakan waktu beberapa detik)
        // Karena dipanggil dari dalam checkSensor -> handleStream, stream akan pause
        processCapture(); 
        
        isProcessing = false; // Selesai, stream boleh lanjut
    }
}

void processCapture() {
  myservo.detach(); 
  camera_fb_t* fb = NULL;
  
  // Ambil frame bersih
  fb = esp_camera_fb_get(); 
  if(fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get(); 
  
  if (!fb) { 
      Serial.println("Capture Failed"); 
      myservo.attach(servoPin);
      return; 
  }

  Serial.println("Uploading to OCR...");
  String plate = uploadToOCR(fb);
  
  if (plate.length() > 0) {
    Serial.println("PLAT: " + plate);
    saveLogToFirestore(plate);
    
    myservo.attach(servoPin);
    openBarrier();
    delay(10000);
    closeBarrier();
  } else {
    Serial.println("No Plate");
    myservo.attach(servoPin);
  }
  
  esp_camera_fb_return(fb);
}

// --- SIMPAN KE FIRESTORE ---
void saveLogToFirestore(String plate) {
  if (!firebaseReady) return;
  Serial.print("Saving DB...");
  
  String docPath = "parking_logs/log_" + String(timeClient.getEpochTime());
  FirebaseJson content;
  content.set("fields/plateNumber/stringValue", plate);
  content.set("fields/photoUrl/stringValue", "https://via.placeholder.com/150?text=No+Image"); 
  content.set("fields/entryTime/integerValue", timeClient.getEpochTime() * 1000LL);
  content.set("fields/status/stringValue", "Masuk");
  content.set("fields/paymentStatus/stringValue", "unpaid");
  
  fbDO.setResponseSize(512);
  if (Firebase.Firestore.createDocument(&fbDO, FIREBASE_PROJECT_ID, "", docPath.c_str(), content.raw())) {
    Serial.println(" OK!");
  } else {
    Serial.println(" Fail: " + fbDO.errorReason());
  }
}

// --- UPLOAD OCR ---
String uploadToOCR(camera_fb_t* fb) {
  if (!fb) return "";
  client.setInsecure(); 
  client.setNoDelay(true);
  client.setTimeout(10000); 

  if (!client.connect(ocrServer, ocrPort)) return "";
  
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
    if(millis() - start > 15000) break;
  }
  client.stop();
  
  int idx = response.indexOf("\"plate\"");
  if (idx != -1) {
      int startQuote = response.indexOf("\"", idx + 8); 
      int endQuote = response.indexOf("\"", startQuote + 1);
      return response.substring(startQuote + 1, endQuote);
  }
  return ""; 
}

void openBarrier() { myservo.write(servoOpen); }
void closeBarrier() { myservo.write(servoClosed); }

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  
  pinMode(flashLight, OUTPUT); digitalWrite(flashLight, LOW);
  pinMode(trigPin, OUTPUT); pinMode(echoPin, INPUT);
  
  ESP32PWM::allocateTimer(0);
  myservo.setPeriodHertz(50);
  myservo.attach(servoPin, 1000, 2000);
  myservo.write(servoClosed);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK");
  Serial.println(WiFi.localIP());

  timeClient.begin();

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
  
  if(psramFound()){
    configCam.frame_size = FRAMESIZE_VGA; 
    configCam.jpeg_quality = 12; 
    configCam.fb_count = 1;
  } else {
    configCam.frame_size = FRAMESIZE_QVGA;
    configCam.jpeg_quality = 12;
    configCam.fb_count = 1;
  }
  esp_camera_init(&configCam);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.begin();
  Serial.println("System Ready (Non-Blocking Stream).");
}

void loop() {
  server.handleClient(); // Tangani klien stream
  
  // Update waktu
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate > 60000) {
      timeClient.update();
      lastTimeUpdate = millis();
  }

  // Cek sensor secara manual juga di loop utama (Backup jika tidak ada client stream)
  checkSensor();
  
  delay(10);
}