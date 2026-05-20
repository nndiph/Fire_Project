// ============================================================
//  PyroGuard System - ESP32 Node 2 Firmware (ACTUATOR)
//  Board  : ESP32 (DOIT DevKit V1 atau sejenisnya)
//  Fungsi : Menerima perintah dari Node 1 via ESP-NOW,
//           mengontrol Solenoid Valve dan Pompa Air (Relay 2 Ch).
//           Mengirim feedback status aktuator ke Node 1.
// ============================================================

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ============================================================
//  KONFIGURASI PIN HARDWARE (Relay 2 Channel)
// ============================================================
#define PIN_POMPA    19   // Relay Channel 2
#define PIN_SOLENOID 18   // Relay Channel 1

// Logika Relay 2 Channel (Active Low)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ============================================================
//  KONFIGURASI JARINGAN & PEER
// ============================================================
const char* targetSSID = "ServerNadhif"; // SSID yang dipakai Node 1
uint8_t macNode1[] = {0xB0, 0xCB, 0xD8, 0x8A, 0x81, 0x1C};

// ============================================================
//  STRUKTUR DATA ESP-NOW (Wajib identik dengan Node 1)
// ============================================================
typedef struct struct_message {
    bool perintahSistem;  // Node 1 -> Node 2
    bool statusPompa;     // Node 2 -> Node 1
    bool statusSolenoid;  // Node 2 -> Node 1
} struct_message;

struct_message dataKirim;
struct_message dataTerima;
esp_now_peer_info_t peerInfo;

unsigned long lastRecvTime = 0;
uint8_t currentChannel = 1;

// ============================================================
//  CALLBACK: Data Diterima dari Node 1
// ============================================================
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
#else
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
    lastRecvTime = millis(); // Reset watchdog timer koneksi
    memcpy(&dataTerima, incomingData, sizeof(dataTerima));

    Serial.println("\n================================");
    Serial.print("[SIGNAL] Perintah: "); 
    Serial.println(dataTerima.perintahSistem ? "!!! BAHAYA !!!" : "--- AMAN ---");

    bool currentPump = (digitalRead(PIN_POMPA) == RELAY_ON);
    bool currentSolenoid = (digitalRead(PIN_SOLENOID) == RELAY_ON);
    bool targetPump = dataTerima.statusPompa;
    bool targetSolenoid = dataTerima.statusSolenoid;

    if (targetPump != currentPump || targetSolenoid != currentSolenoid) {
        Serial.println("[ACTION] Perubahan Status Aktuator...");
        
        if (targetSolenoid != currentSolenoid) {
            digitalWrite(PIN_SOLENOID, targetSolenoid ? RELAY_ON : RELAY_OFF);
            Serial.printf("  > Solenoid: %s\n", targetSolenoid ? "TERBUKA (ON)" : "TERTUTUP (OFF)");
            if (targetSolenoid) delay(400); // Jeda stabilitas arus adaptor 12V 4A
        }
        
        if (targetPump != currentPump) {
            digitalWrite(PIN_POMPA, targetPump ? RELAY_ON : RELAY_OFF);
            Serial.printf("  > Pompa   : %s\n", targetPump ? "MENYEMPROT (ON)" : "BERHENTI (OFF)");
            if (!targetPump) delay(200);
        }
    } else {
        Serial.println("[HEARTBEAT] Sinyal diterima, status tetap.");
    }

    // Ambil status riil langsung dari hardware pins untuk feedback terpercaya
    dataKirim.statusPompa    = (digitalRead(PIN_POMPA) == RELAY_ON);
    dataKirim.statusSolenoid = (digitalRead(PIN_SOLENOID) == RELAY_ON);
    dataKirim.perintahSistem = dataTerima.perintahSistem;

    // Kirim Feedback Balik ke Node 1
    esp_now_send(macNode1, (uint8_t *)&dataKirim, sizeof(dataKirim));
    Serial.println("================================\n");
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
    Serial.begin(115200);

    // --- Inisialisasi Relay ---
    pinMode(PIN_POMPA, OUTPUT);
    pinMode(PIN_SOLENOID, OUTPUT);
    digitalWrite(PIN_POMPA, RELAY_OFF);
    digitalWrite(PIN_SOLENOID, RELAY_OFF);

    // --- Mode WiFi ---
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.println("\nScanning WiFi Channel untuk SSID: " + String(targetSSID));
    
    // --- Sinkronisasi Channel Otomatis ---
    int32_t channel = 1;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == targetSSID) {
            channel = WiFi.channel(i);
            break;
        }
    }
    currentChannel = channel;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    Serial.print("[OK] Menggunakan Channel: "); Serial.println(currentChannel);
    
    lastRecvTime = millis(); // Inisialisasi waktu terima pertama

    // --- Inisialisasi ESP-NOW ---
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] Gagal inisialisasi ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv);

    // --- Daftarkan Node 1 sebagai Peer ---
    memcpy(peerInfo.peer_addr, macNode1, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERROR] Gagal menambahkan peer Node 1");
    } else {
        Serial.println("[OK] Terhubung ke Node 1. Menunggu sinyal...");
    }
}

void loop() {
    unsigned long now = millis();

    // ── 1. FAIL-SAFE MECHANISM (Matikan pompa/solenoid jika putus koneksi saat aktif) ──
    bool pumpActive = (digitalRead(PIN_POMPA) == RELAY_ON);
    bool solenoidActive = (digitalRead(PIN_SOLENOID) == RELAY_ON);
    
    if ((pumpActive || solenoidActive) && (now - lastRecvTime > 8000)) {
        Serial.println("\n[WARNING] [FAIL-SAFE] Kehilangan sinyal dari Node 1 selama lebih dari 8 detik!");
        Serial.println("[ACTION] Mematikan Pompa dan Solenoid demi Keamanan!");
        
        digitalWrite(PIN_POMPA, RELAY_OFF);
        delay(200);
        digitalWrite(PIN_SOLENOID, RELAY_OFF);
        
        dataKirim.statusPompa = false;
        dataKirim.statusSolenoid = false;
        dataKirim.perintahSistem = false;
    }

    // ── 2. DYNAMIC CHANNEL RESCANNING (Pencarian channel WiFi otomatis jika offline) ──
    if (now - lastRecvTime > 10000) {
        static unsigned long lastScanAttempt = 0;
        if (now - lastScanAttempt > 15000) {
            lastScanAttempt = now;
            Serial.println("\n[WIFI-SCAN] Kehilangan sinyal > 10 detik. Memulai pencarian channel...");
            int n = WiFi.scanNetworks();
            int32_t newChannel = -1;
            for (int i = 0; i < n; i++) {
                if (WiFi.SSID(i) == targetSSID) {
                    newChannel = WiFi.channel(i);
                    break;
                }
            }
            WiFi.scanDelete(); // Bebaskan memori hasil scan
            
            if (newChannel != -1) {
                Serial.printf("[WIFI-SCAN] SSID ditemukan di Channel %d (Channel lama: %d)\n", newChannel, currentChannel);
                if (newChannel != currentChannel) {
                    currentChannel = newChannel;
                    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
                    Serial.println("[OK] Channel ESP-NOW diperbarui!");
                } else {
                    Serial.println("[WIFI-SCAN] Channel tidak berubah. Sinyal mungkin lemah atau terhalang.");
                }
            } else {
                Serial.println("[WIFI-SCAN] SSID target 'ServerNadhif' tidak ditemukan!");
            }
        }
    }

    // Heartbeat Serial setiap 5 detik agar tahu alat masih hidup
    static unsigned long lastHeartbeat = 0;
    if (now - lastHeartbeat > 5000) {
        lastHeartbeat = now;
        Serial.print("Status: Standby - Last signal received ");
        Serial.print((now - lastRecvTime) / 1000.0);
        Serial.println("s ago.");
    }
    
    delay(10);
}