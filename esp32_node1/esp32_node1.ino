// ============================================================
//  PyroGuard System - ESP32 Node 1 Firmware (MASTER/GATEWAY)
//  VERSI: FIX SERVO VERTIKAL (STANDBY 15 DEG, SWEEP 15-35 DEG)
//  Board  : ESP32
// ============================================================

#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <PubSubClient.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============================================================
//  KONFIGURASI JARINGAN & MQTT
// ============================================================
const char* ssid       = "ServerNadhif";
const char* password   = "12345678";
const char* mqttServer = "broker.emqx.io";
const int mqttPort = 1883;
const char* mqttTopic = "pyroguard/sensor/node1";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ============================================================
//  KONFIGURASI PIN HARDWARE
// ============================================================
#define PIN_SERVO_H    27   // Servo Horizontal (Kiri-Kanan)
#define PIN_SERVO_V    33   // Servo Vertikal (Atas-Bawah)
#define PIN_DHT        15
#define DHTTYPE        DHT22
#define PIN_WATER      34   // Analog input

// Pin Sensor Api (Active HIGH/LOW sesuaikan dengan modul)
#define M1 13
#define M2 14
#define M3 25
#define M4 26
#define M5 32
#define API_AKTIF HIGH 

// Kalibrasi Sensor Air
#define WATER_MIN   200 
#define WATER_MAX  3800 

// ============================================================
//  ESP-NOW: Komunikasi ke Node 2 (Actuator)
// ============================================================
uint8_t macNode2[] = {0x70, 0x4B, 0xCA, 0x49, 0x12, 0x34};

typedef struct struct_message {
    bool perintahSistem;  // Node 1 -> Node 2 (true = BAHAYA, false = AMAN)
    bool statusPompa;     // Node 2 -> Node 1 (Feedback)
    bool statusSolenoid;  // Node 2 -> Node 1 (Feedback)
} struct_message;

struct_message dataKirim;    
struct_message dataTerima;   

bool feedbackPompa     = false;
bool feedbackSolenoid  = false;
bool espNowTerkirim    = false;  
bool node2Koneksi      = false;  
unsigned long lastNode2Response = 0; 
esp_now_peer_info_t peerInfo;

// ============================================================
//  VARIABEL KONTROL SERVO & DELAY FISIK
// ============================================================
Servo radarServoH;
Servo radarServoV;
DHT   dht(PIN_DHT, DHTTYPE);

// ── Sumbu X (Horizontal - Mengikuti Sensor Api) ──
int  sudutSekarangH  = 90; 
int  sudutTargetH    = 90; 

const unsigned long DELAY_FISIK_SERVO = 600; // ms menunggu servo sampai sebelum nyala pompa
unsigned long waktuTargetBerubah      = 0;   // Pencatat waktu kapan servo mulai gerak

// ── Sumbu Y (Vertikal - Standby 15°, Sweep 15° sampai 35°) ──
int  posisiAmanV     = 15;  // DIUBAH KE 15 DEG
int  sudutSekarangV  = -1;  // Dibuat -1 agar ESP32 memaksa write ke servo saat booting
int  sudutTargetV    = posisiAmanV;
int  batasBawahV     = 15;  // Titik terendah semprotan
int  batasAtasV      = 35;  // Titik tertinggi semprotan

unsigned long waktuToggleV = 0;
const unsigned long INTERVAL_TOGGLE_V = 500; // Kecepatan lompat atas-bawah

// Waypoint (Sweep Sumbu X jika ada banyak api)
int  waypointList[5]  = {90,90,90,90,90}; 
int  waypointCount    = 0;
int  waypointIdx      = 0;                
bool waypointMaju     = true;             
unsigned long waktuDwell = 0;             
const unsigned long WAKTU_SEMPROT = 600; // ms lamanya menyemprot di tiap titik saat sweep

unsigned long waktuApiTerakhir   = 0;
const unsigned long JEDA_TUNGGU = 3000; // Servo tahan posisi 3 detik jika api mati
int lastPerintahKirim = -1; 

// Variabel Global Suhu 
float suhuSaatIni = 0.0;
bool statusApi = false; 

// ============================================================
//  CALLBACK ESP-NOW
// ============================================================
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
    memcpy(&dataTerima, incomingData, sizeof(dataTerima));
    feedbackPompa    = dataTerima.statusPompa;
    feedbackSolenoid = dataTerima.statusSolenoid;
    lastNode2Response = millis();
    node2Koneksi      = true;
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
#else
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
#endif
    espNowTerkirim = (status == ESP_NOW_SEND_SUCCESS);
}

void kirimPerintahNode2(bool bahaya) {
    dataKirim.perintahSistem = bahaya;
    dataKirim.statusPompa    = false;  
    dataKirim.statusSolenoid = false;
    esp_now_send(macNode2, (uint8_t *)&dataKirim, sizeof(dataKirim));
}

void reconnect() {
  while (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    String clientId = "PyroGuard-ESP32-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      // Terhubung
    } else {
      delay(3000);
    }
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  // Inisialisasi Servo
  radarServoH.attach(PIN_SERVO_H, 500, 2500);
  radarServoV.attach(PIN_SERVO_V, 500, 2500);
  radarServoH.write(90);          // Horizontal istirahat di 90
  radarServoV.write(posisiAmanV); // Vertikal istirahat di posisi aman (15)

  // Inisialisasi Sensor Api
  uint8_t modePin = (API_AKTIF == LOW) ? INPUT_PULLUP : INPUT_PULLDOWN;
  pinMode(M1, modePin); pinMode(M2, modePin);
  pinMode(M3, modePin); pinMode(M4, modePin); pinMode(M5, modePin);

  // Inisialisasi DHT
  dht.begin();

  // Inisialisasi WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }

  // Inisialisasi ESP-NOW
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);
    memcpy(peerInfo.peer_addr, macNode2, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  mqttClient.setServer(mqttServer, mqttPort);
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) reconnect();
    mqttClient.loop();
  }

  // ── 1. BACA SENSOR SUHU ───────────────────────────────────────
  static unsigned long waktuDhtTerakhir = 0;
  if (millis() - waktuDhtTerakhir >= 2000) {
    waktuDhtTerakhir = millis();
    float bacaSuhu = dht.readTemperature();
    if (!isnan(bacaSuhu)) {
      suhuSaatIni = bacaSuhu;
    }
  }

  // ── 2. BACA LEVEL AIR ────────────────────────────────────────
  int rawWater      = analogRead(PIN_WATER);
  int persentaseAir = map(rawWater, WATER_MIN, WATER_MAX, 0, 100);
  persentaseAir     = constrain(persentaseAir, 0, 100);

  // ── 3. BACA SENSOR API ───────────────────────────────────────
  int pinSensor[5] = {M1, M2, M3, M4, M5};
  int hasilSensor[5] = {0, 0, 0, 0, 0};

  for (int i = 0; i < 5; i++) {
    int hitungAktif = 0;
    for (int j = 0; j < 5; j++) {
      if (digitalRead(pinSensor[i]) == API_AKTIF) hitungAktif++;
      delayMicroseconds(200);
    }
    hasilSensor[i] = (hitungAktif >= 3) ? 1 : 0;
  }

  int totalSudut = 0;
  int jumlahSensorAktif = 0;
  
  if (hasilSensor[0]) { totalSudut += 0;   jumlahSensorAktif++; }
  if (hasilSensor[1]) { totalSudut += 45;  jumlahSensorAktif++; }
  if (hasilSensor[2]) { totalSudut += 90;  jumlahSensorAktif++; }
  if (hasilSensor[3]) { totalSudut += 135; jumlahSensorAktif++; }
  if (hasilSensor[4]) { totalSudut += 180; jumlahSensorAktif++; }

  statusApi = (jumlahSensorAktif > 0);

  // ── 4. TENTUKAN ARAH (TARGET) SERVO HORIZONTAL ───────────────
  if (jumlahSensorAktif == 1) {
    waktuApiTerakhir = millis();
    sudutTargetH = totalSudut; 
    waypointCount = 0;
  } 
  else if (jumlahSensorAktif >= 2) {
    waktuApiTerakhir = millis();

    int newWP[5]; int nWP = 0;
    if (hasilSensor[0]) newWP[nWP++] = 0;
    if (hasilSensor[1]) newWP[nWP++] = 45;
    if (hasilSensor[2]) newWP[nWP++] = 90;
    if (hasilSensor[3]) newWP[nWP++] = 135;
    if (hasilSensor[4]) newWP[nWP++] = 180;

    if (nWP != waypointCount) {
      waypointCount = nWP;
      for (int i = 0; i < nWP; i++) waypointList[i] = newWP[i];
      waypointIdx   = 0;
      waypointMaju  = true;
      sudutTargetH  = waypointList[0];
      waktuDwell    = millis();
    }

    if (millis() - waktuDwell >= (DELAY_FISIK_SERVO + WAKTU_SEMPROT)) {
      waktuDwell = millis();
      if (waypointMaju) {
        waypointIdx++;
        if (waypointIdx >= waypointCount) { waypointIdx = waypointCount - 2; waypointMaju = false; }
      } else {
        waypointIdx--;
        if (waypointIdx < 0) { waypointIdx = 1; waypointMaju = true; }
      }
      if (waypointIdx < 0) waypointIdx = 0;
      if (waypointIdx >= waypointCount) waypointIdx = waypointCount - 1;
      
      sudutTargetH = waypointList[waypointIdx];
    }
  } 
  else {
    waypointCount = 0;
    if (millis() - waktuApiTerakhir <= JEDA_TUNGGU) {
      sudutTargetH = sudutSekarangH; 
    } else {
      sudutTargetH = 90; 
    }
  }

  // ── 5. EKSEKUSI GERAK SERVO & HITUNG DELAY ───────────────────
  // Perbarui Sumbu X (Horizontal)
  if (sudutSekarangH != sudutTargetH) {
    sudutSekarangH = sudutTargetH;
    radarServoH.write(sudutSekarangH);
    waktuTargetBerubah = millis(); 
  }

  // LOGIKA UTAMA POMPA AIR (PERINTAH BAHAYA)
  bool servoSiap = (millis() - waktuTargetBerubah >= DELAY_FISIK_SERVO);
  bool perintahBahaya = false;

  if (jumlahSensorAktif > 0 && servoSiap && persentaseAir > 5) {
    perintahBahaya = true;
  }

  // Perbarui Sumbu Y (Vertikal - Sweep 15-35 Derajat)
  if (perintahBahaya) {
    if (millis() - waktuToggleV > INTERVAL_TOGGLE_V) {
      waktuToggleV = millis();
      // Lompat antara 15 dan 35 derajat
      if (sudutTargetV == batasBawahV) sudutTargetV = batasAtasV;
      else sudutTargetV = batasBawahV;
    }
  } else {
    // Kalau aman, servo istirahat di posisi 15 derajat
    sudutTargetV = posisiAmanV;
  }

  // Eksekusi Tulis ke Servo Vertikal
  if (sudutSekarangV != sudutTargetV) {
    sudutSekarangV = sudutTargetV;
    radarServoV.write(sudutSekarangV);
  }

  // ── 6. ESP-NOW KOMUNIKASI NODE 2 ─────────────────────────────
  if (millis() - lastNode2Response > 8000) {
    if (node2Koneksi) {
      node2Koneksi = false;
      feedbackPompa = false;
      feedbackSolenoid = false;
    }
  }

  static unsigned long waktuHeartbeatTerakhir = 0;
  bool stateChanged = (perintahBahaya != lastPerintahKirim);
  bool heartbeatDue = (millis() - waktuHeartbeatTerakhir > 3000);

  if (stateChanged || heartbeatDue) {
    lastPerintahKirim = perintahBahaya;
    waktuHeartbeatTerakhir = millis();
    kirimPerintahNode2(perintahBahaya);
  }

  // ── 7. SERIAL MONITOR ────────────────────────────────────────
  static unsigned long waktuKirimTerakhir = 0;
  if (millis() - waktuKirimTerakhir > 2000) {
    waktuKirimTerakhir = millis();
    Serial.println("========== STATUS UPDATE ==========");
    Serial.print("Sensor : Aktif "); Serial.print(jumlahSensorAktif); Serial.println(" titik");
    Serial.print("Servo  : Pan(H)="); Serial.print(sudutSekarangH); 
    Serial.print("° | Tilt(V)="); Serial.print(sudutSekarangV); Serial.println("°");
    Serial.print("Suhu   : "); Serial.print(suhuSaatIni, 1); Serial.println(" C");
    Serial.print("Air    : "); Serial.print(persentaseAir); Serial.println("%");
    Serial.print("Pompa  : "); Serial.println(perintahBahaya ? "SEMPROT AKTIF" : "MATI");
    Serial.println("===================================\n");
  }

  // ── 8. MQTT PUBLISH ──────────────────────────────────────────
  static unsigned long waktuMqttTerakhir = 0;
  if (millis() - waktuMqttTerakhir > 1000) {
    waktuMqttTerakhir = millis();
    String dataJson = "{\"node_id\":\"Node1\","
                      "\"suhu\":" + String(suhuSaatIni, 1) + ","
                      "\"status_api\":" + String(statusApi ? 1 : 0) + ","
                      "\"water_level\":" + String(persentaseAir) + ","
                      "\"servo_h\":" + String(sudutSekarangH) + ","
                      "\"servo_v\":" + String(sudutSekarangV) + "}";
    if (mqttClient.connected()) {
      mqttClient.publish(mqttTopic, dataJson.c_str());
    }
  }
}