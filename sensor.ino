#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NewPing.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h> // Library Wajib: Firebase ESP Client by Mobizt

// --- TOKEN GENERATION & ADDONS ---
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// --- KONFIGURASI SENSOR ---
#define TRIG1 25
#define ECHO1 33
#define TRIG2 5
#define ECHO2 18
#define TRIG3 14
#define ECHO3 27
#define TRIG4 32
#define ECHO4 35

#define MAX_DISTANCE 50 
const int totalSlot = 4;

// --- WIFI & FIREBASE CONFIG ---
#define WIFI_SSID "Chaaa"
#define WIFI_PASSWORD "kka0y0lllllaa"

// Dapatkan di Firebase Console -> Project Settings -> General
#define API_KEY "AIzaSyAqFcSWRj7qyEFVyfhCAaIyTdU7xaoNyk0"
#define FIREBASE_PROJECT_ID "spoton-park" // Contoh: smart-parking-123

// Buat User baru di menu Authentication -> Sign-in method -> Email/Password
#define USER_EMAIL "sp@gmail.com"
#define USER_PASSWORD "admin123"

// --- OBJEK ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
NewPing sonar1(TRIG1, ECHO1, MAX_DISTANCE);
NewPing sonar2(TRIG2, ECHO2, MAX_DISTANCE);
NewPing sonar3(TRIG3, ECHO3, MAX_DISTANCE);
NewPing sonar4(TRIG4, ECHO4, MAX_DISTANCE);

// Objek Firebase
FirebaseData fbDO;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long lastFirebaseTime = 0;
const long firebaseInterval = 3000; // Kirim setiap 3 detik
bool taskCompleted = false;

void setup() {
    Serial.begin(115200);
    lcd.init();
    lcd.backlight();
    
    // 1. Koneksi WiFi
    lcd.print("Connect WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
    lcd.clear();
    lcd.print("WiFi OK!");

    // 2. Konfigurasi Firebase
    config.api_key = API_KEY;
    
    // Autentikasi User (Wajib untuk Firestore)
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    // Token config
    config.token_status_callback = tokenStatusCallback; 

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    delay(1000);
}

// Fungsi Helper untuk update dokumen Firestore
void updateSlotToFirestore(String docId, String status) {
    // Firestore Path: projects/{project_id}/databases/(default)/documents/{collection_id}/{document_id}
    
    FirebaseJson content;
    // Firestore membutuhkan format khusus: { "fields": { "key": { "stringValue": "value" } } }
    // Library ini menyederhanakannya dengan set("fields/nama_field/tipe_data", nilai)
    
    content.set("fields/status/stringValue", status);
    // Tambahkan timestamp server (opsional)

    String documentPath = "parking_slots/" + docId; // Collection: parking_slots, Doc: A1/A2...

    // Menggunakan patchDocument agar tidak menimpa field lain jika ada
    if (Firebase.Firestore.patchDocument(&fbDO, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "status")) {
        Serial.print("Update " + docId + ": OK | ");
    } else {
        Serial.print("Update " + docId + " Gagal: " + fbDO.errorReason() + " | ");
    }
}

void loop() {
    unsigned long currentMillis = millis();
    int slotTersedia = totalSlot;

    // --- BACA SENSOR ---
    float d1 = sonar1.ping_cm(); delay(30);
    float d2 = sonar2.ping_cm(); delay(30);
    float d3 = sonar3.ping_cm(); delay(30);
    float d4 = sonar4.ping_cm(); delay(30);

    // Logika: Jarak < 10cm dianggap TERISI
    bool a1 = (d1 > 0 && d1 < 10);
    bool a2 = (d2 > 0 && d2 < 10);
    bool b1 = (d3 > 0 && d3 < 10);
    bool b2 = (d4 > 0 && d4 < 10);

    // Hitung Slot
    if (a1) slotTersedia--;
    if (a2) slotTersedia--;
    if (b1) slotTersedia--;
    if (b2) slotTersedia--;

    // --- UPDATE LCD ---
    lcd.setCursor(0, 0);
    lcd.print("Slot: "); lcd.print(slotTersedia); lcd.print("/"); lcd.print(totalSlot); lcd.print("    ");
    lcd.setCursor(0, 1);
    lcd.print(a1?"[X]":"[ ]");
    lcd.print(a2?"[X]":"[ ]");
    lcd.print(b1?"[X]":"[ ]");
    lcd.print(b2?"[X]":"[ ]");

    // --- KIRIM KE FIRESTORE ---
    if (Firebase.ready() && (currentMillis - lastFirebaseTime >= firebaseInterval)) {
        lastFirebaseTime = currentMillis;
        Serial.println("\nMengirim data ke Firestore...");

        // Kirim per dokumen (sesuai struktur dashboard.js Anda)
        updateSlotToFirestore("A1", a1 ? "Terisi" : "Tersedia");
        updateSlotToFirestore("A2", a2 ? "Terisi" : "Tersedia");
        updateSlotToFirestore("B1", b1 ? "Terisi" : "Tersedia");
        updateSlotToFirestore("B2", b2 ? "Terisi" : "Tersedia");
        
        Serial.println("\nSelesai siklus kirim.");
    }
}