/*
 ==============================================================================
  PROYEK VORTEX-X // ESP32 #2: RECEIVER (4 SERVO BAWAH MELAMBAI + DISPLAY DATA)
  
  ALUR SISTEM:
  [Web Dashboard] <---(Wi-Fi HTTP)---> [ESP32 TRANSMITTER] <---(ESP-NOW)---> [ESP32 RECEIVER]

  PERANGKAT KERAS PADA ESP32 RECEIVER:
  1. 4 Servo SG90 (Posisi Bawah / Lower Joint — khusus untuk melambai-lambaikan kaki):
     - Servo [0] Belakang Kiri  (RL) -> GPIO 13 (D13)
     - Servo [1] Depan Kiri     (FL) -> GPIO 14 (D14)
     - Servo [2] Belakang Kanan (RR) -> GPIO 18 (D18)
     - Servo [3] Depan Kanan    (FR) -> GPIO 21 (D21)
  2. Layar LCD 16x2 I2C (Menampilkan data Suhu, RH, PIR, Mic & Status dari Transmitter):
     - SDA -> GPIO 19 (D19)  <-- PENTING! Bukan GPIO 21 karena D21 dipakai Servo FR!
     - SCL -> GPIO 22 (D22)
     - VCC -> 5V (VIN / Bus 5V) | GND -> Common GND
     *(Jika LCD belum dipasang, seluruh data juga otomatis tampil di Serial Monitor 115200)*

  FITUR ANTI-GETAR & AUTO-PAIRING:
  - Saat baru menyala (boot), ke-4 servo dalam keadaan RELAX (PWM mati).
    Tidak memaksa ke 90 derajat, tidak nyentak, dan 100% diam di posisi apa pun.
  - Setiap servo punya titik awal kalibrasi masing-masing (homeAngles[4]) yang bisa
    disesuaikan dari Web Dashboard (dikirim lewat Transmitter) dan tersimpan di Flash.
  - Otomatis menyamakan Channel Wi-Fi & Auto-Pairing dengan Transmitter (tanpa ketik MAC!).
  - Kompatibel dengan Arduino ESP32 Core v2.x maupun v3.x (ESP-IDF v5.x).
 ==============================================================================
*/

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Preferences.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// 1. NAMA WI-FI TARGET (HARUS SAMA DENGAN TRANSMITTER AGAR CHANNEL COCOK)
// ============================================================================
const char* TARGET_SSID_1 = "pribadi";
const char* TARGET_SSID_2 = "VORTEX-X_ROBOT";

// ============================================================================
// 2. PINOUT HARDWARE ESP32 RECEIVER (SESUAI PERMINTAAN!)
// ============================================================================
// Pin 4 Servo Bagian Bawah (untuk melambai-lambaikan kaki):
const int SERVO_PINS[4] = {
  13, // [0] Belakang Kiri  (RL - D13)
  14, // [1] Depan Kiri     (FL - D14)
  18, // [2] Belakang Kanan (RR - D18)
  21  // [3] Depan Kanan    (FR - D21)
};

// Pin I2C Kustom untuk LCD 16x2 (Menghindari GPIO 21 karena dipakai Servo Depan Kanan!)
const int PIN_LCD_SDA = 19; // D19 (GPIO19)
const int PIN_LCD_SCL = 22; // D22 (GPIO22)

LiquidCrystal_I2C* lcd = nullptr;
uint8_t lcdAddress = 0x27;
bool lcdReady = false;

Preferences prefs;
Servo servos[4];
bool servosAttached = false;

// Titik awal masing-masing servo (bukan wajib 90!) & sudut aktif
int homeAngles[4]         = { 90, 90, 90, 90 };
int currentServoAngles[4] = { 90, 90, 90, 90 };
int lastWrittenAngles[4]  = { -1, -1, -1, -1 };

// ============================================================================
// 3. STRUKTUR PAKET ESP-NOW (HARUS SAMA PERSIS DENGAN TRANSMITTER!)
// ============================================================================
#define PKT_MAGIC_TX 0xA55A
#define PKT_MAGIC_RX 0x5AA5

typedef struct __attribute__((packed)) {
  uint16_t magic;
  uint32_t seq;
  char command[16];
  uint8_t speed;
  float temperature;
  float humidity;
  uint8_t pir;
  uint8_t micLevel;
  uint8_t micAlert;
  int8_t calServoId;
  int16_t calServoAngle;
  uint8_t calAction;
} VortexTxPacket;

typedef struct __attribute__((packed)) {
  uint16_t magic;
  uint32_t ackSeq;
  char activeCmd[16];
  uint8_t attached;
  uint8_t lcdReady;
  int16_t servos[4];
  int16_t homes[4];
} VortexRxPacket;

VortexTxPacket pendingTxPkt;
volatile bool hasNewTxPacket = false;
uint8_t lastSenderMac[6] = {0};

uint8_t transmitterMac[6] = {0};
bool transmitterPaired = false;
uint8_t activeChannel = 1;
unsigned long lastTxRecvMs = 0;
unsigned long lastChannelHopMs = 0;

// ============================================================================
// 4. VARIABEL TELEMETRI & GERAKAN MELAMBAI
// ============================================================================
String currentCommand = "RELAX";
int currentSpeedLevel = 2;

float rxTemp     = 0.0;
float rxHumidity = 0.0;
uint8_t rxPir    = 0;
uint8_t rxMic    = 0;
uint8_t rxMicAlert = 0;
uint32_t lastSeq = 0;

String lcdRow0Cache = "";
String lcdRow1Cache = "";
unsigned long lastLcdUpdateMs = 0;
unsigned long lastCommandMs   = 0;
const unsigned long CMD_TIMEOUT_MS = 3500;

int waveStep = 0;
unsigned long lastWaveStepMs = 0;

// ============================================================================
// 5. KALIBRASI FLASH, ANTI-GETAR & GERAKAN MELAMBAI 4 SERVO BAWAH
// ============================================================================
void loadHomeAnglesFromFlash() {
  prefs.begin("vortex_rx4", true);
  for (int i = 0; i < 4; i++) {
    String key = "h" + String(i);
    int saved = prefs.getInt(key.c_str(), 90);
    homeAngles[i] = constrain(saved, 15, 165);
    currentServoAngles[i] = homeAngles[i];
  }
  prefs.end();
  Serial.println("[CAL] Titik awal 4 servo dimuat dari Flash.");
}

void saveHomeAnglesToFlash() {
  prefs.begin("vortex_rx4", false);
  for (int i = 0; i < 4; i++) {
    homeAngles[i] = constrain(currentServoAngles[i], 15, 165);
    String key = "h" + String(i);
    prefs.putInt(key.c_str(), homeAngles[i]);
  }
  prefs.end();
  Serial.println("[CAL] Titik awal 4 servo BERHASIL disimpan permanen!");
}

void resetHomeAnglesInFlash() {
  prefs.begin("vortex_rx4", false);
  for (int i = 0; i < 4; i++) {
    homeAngles[i] = 90;
    currentServoAngles[i] = 90;
    String key = "h" + String(i);
    prefs.putInt(key.c_str(), 90);
  }
  prefs.end();
  Serial.println("[CAL] Titik awal 4 servo di-reset ke 90 derajat.");
}

void initServoTimers() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
}

void ensureSingleServoAttached(int idx) {
  if (idx < 0 || idx >= 4) return;
  if (!servos[idx].attached()) {
    servos[idx].setPeriodHertz(50);
    servos[idx].attach(SERVO_PINS[idx], 500, 2400);
  }
}

void ensureAllServosAttached() {
  for (int i = 0; i < 4; i++) {
    if (!servos[i].attached()) {
      servos[i].setPeriodHertz(50);
      servos[i].attach(SERVO_PINS[i], 500, 2400);
      servos[i].write(currentServoAngles[i]);
      lastWrittenAngles[i] = currentServoAngles[i];
    }
  }
  servosAttached = true;
}

void detachAllServos() {
  for (int i = 0; i < 4; i++) {
    if (servos[i].attached()) {
      servos[i].detach();
    }
    lastWrittenAngles[i] = -1;
  }
  servosAttached = false;
  Serial.println("[SERVO] Mode RELAX: PWM dilepas -> 4 servo diam tenang tanpa getar.");
}

void writeServoRaw(int idx, int angle) {
  if (idx < 0 || idx >= 4) return;
  int safeAngle = constrain(angle, 15, 165);
  currentServoAngles[idx] = safeAngle;
  if (servos[idx].attached() && lastWrittenAngles[idx] != safeAngle) {
    servos[idx].write(safeAngle);
    lastWrittenAngles[idx] = safeAngle;
  }
}

void writeServoOffset(int idx, int offset) {
  writeServoRaw(idx, homeAngles[idx] + offset);
}

void poseStand() {
  ensureAllServosAttached();
  for (int i = 0; i < 4; i++) {
    writeServoOffset(i, 0);
  }
}

void poseCrouch() {
  ensureAllServosAttached();
  for (int i = 0; i < 4; i++) {
    writeServoOffset(i, 30);
  }
}

// Gerakan melambai-lambaikan khusus 4 servo kaki bagian bawah
// Index: [0] RL(G13), [1] FL(G14), [2] RR(G18), [3] FR(G21)
void runWaveMotion(String cmd, int step) {
  ensureAllServosAttached();

  if (cmd == "WAVE") {
    // Melambaikan kaki depan kiri & kanan bergantian dengan ceria
    writeServoOffset(0, 0); // Belakang Kiri tenang
    writeServoOffset(2, 0); // Belakang Kanan tenang
    if (step % 2 == 0) {
      writeServoOffset(3, -30); // Depan Kanan angkat/lambai
      writeServoOffset(1, 25);  // Depan Kiri turun berlawanan
    } else {
      writeServoOffset(3, 25);  // Depan Kanan turun
      writeServoOffset(1, -30); // Depan Kiri angkat/lambai
    }
  } else if (cmd == "FORWARD") {
    // Gelombang melambai berurutan ke 4 kaki (FL -> FR -> RR -> RL)
    for (int i = 0; i < 4; i++) writeServoOffset(i, 0);
    int order[4] = {1, 3, 2, 0}; // FL(14) -> FR(21) -> RR(18) -> RL(13)
    writeServoOffset(order[step % 4], -32);
  } else if (cmd == "BACKWARD") {
    // Gelombang mundur (RL -> RR -> FR -> FL)
    for (int i = 0; i < 4; i++) writeServoOffset(i, 0);
    int order[4] = {0, 2, 3, 1};
    writeServoOffset(order[step % 4], -32);
  } else if (cmd == "LEFT") {
    // Melambaikan kaki sisi kiri (RL=13 & FL=14)
    writeServoOffset(2, 0);
    writeServoOffset(3, 0);
    int amp = (step % 2 == 0) ? -30 : 25;
    writeServoOffset(0, amp);
    writeServoOffset(1, amp);
  } else if (cmd == "RIGHT") {
    // Melambaikan kaki sisi kanan (RR=18 & FR=21)
    writeServoOffset(0, 0);
    writeServoOffset(1, 0);
    int amp = (step % 2 == 0) ? -30 : 25;
    writeServoOffset(2, amp);
    writeServoOffset(3, amp);
  }
}

// ============================================================================
// 6. TAMPILAN LCD 16x2 & SERIAL MONITOR PADA RECEIVER
// ============================================================================
void updateLcdText(String row0, String row1) {
  if (!lcdReady || lcd == nullptr) return;
  while (row0.length() < 16) row0 += " ";
  while (row1.length() < 16) row1 += " ";
  if (row0.length() > 16) row0 = row0.substring(0, 16);
  if (row1.length() > 16) row1 = row1.substring(0, 16);

  if (row0 != lcdRow0Cache) {
    lcd->setCursor(0, 0);
    lcd->print(row0);
    lcdRow0Cache = row0;
  }
  if (row1 != lcdRow1Cache) {
    lcd->setCursor(0, 1);
    lcd->print(row1);
    lcdRow1Cache = row1;
  }
}

void initReceiverLcd() {
  // PENTING: Panggil Wire.begin(19, 22) SEBELUM lcd->init()
  // agar I2C tidak menimpa pin GPIO21 yang dipakai Servo Depan Kanan!
  Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL);
  delay(40);

  uint8_t lcdAddrs[] = {0x27, 0x3F};
  for (uint8_t addr : lcdAddrs) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      lcdAddress = addr;
      lcd = new LiquidCrystal_I2C(lcdAddress, 16, 2);
      lcd->init();
      lcd->backlight();
      lcdReady = true;
      updateLcdText("VORTEX-X RX MODE", "Menunggu TX...  ");
      Serial.print("[OK] LCD 16x2 Receiver aktif di 0x");
      Serial.print(lcdAddress, HEX);
      Serial.println(" (SDA=G19, SCL=G22)");
      break;
    }
  }
  if (!lcdReady) {
    Serial.println("[INFO] LCD 16x2 tidak terpasang di Receiver (Data tetap tampil di Serial Monitor).");
  }
}

void refreshReceiverDisplay() {
  bool txOnline = (lastTxRecvMs > 0) && (millis() - lastTxRecvMs < 3500);

  if (lcdReady) {
    String r0, r1;
    if (!txOnline) {
      r0 = "MENCARI DATA TX ";
      r1 = "Scan Ch:" + String(activeChannel) + " [" + currentCommand + "]";
    } else {
      // Baris 1: Suhu, Kelembapan, Mic dari Transmitter
      r0 = "T:" + String(rxTemp, 1) + "C H:" + String((int)rxHumidity) + "% M:" + String(rxMic);
      // Baris 2: Peringatan PIR / Mic / Status Pose
      if (rxPir == 1) {
        r1 = "!! GERAKAN PIR !";
      } else if (rxMicAlert == 1) {
        r1 = "!! SUARA KERAS !";
      } else if ((millis() / 4000) % 2 == 0) {
        r1 = "PIR:0 [" + currentCommand + "]";
      } else {
        r1 = "RX:OK [" + String(currentServoAngles[0]) + "," + String(currentServoAngles[1]) + "," + String(currentServoAngles[2]) + "," + String(currentServoAngles[3]) + "]";
      }
    }
    updateLcdText(r0, r1);
  }
}

// ============================================================================
// 7. ESP-NOW WIRELESS RECEIVER & AUTO-CHANNEL SYNC
// ============================================================================
void sendAckToTransmitter() {
  if (!transmitterPaired) return;
  VortexRxPacket ack;
  memset(&ack, 0, sizeof(ack));
  ack.magic = PKT_MAGIC_RX;
  ack.ackSeq = lastSeq;
  strncpy(ack.activeCmd, currentCommand.c_str(), sizeof(ack.activeCmd) - 1);
  ack.attached = servosAttached ? 1 : 0;
  ack.lcdReady = lcdReady ? 1 : 0;
  for (int i = 0; i < 4; i++) {
    ack.servos[i] = (int16_t)currentServoAngles[i];
    ack.homes[i]  = (int16_t)homeAngles[i];
  }
  esp_now_send(transmitterMac, (uint8_t*)&ack, sizeof(ack));
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len == sizeof(VortexTxPacket)) {
    VortexTxPacket incoming;
    memcpy(&incoming, data, sizeof(incoming));
    if (incoming.magic == PKT_MAGIC_TX) {
      memcpy((void*)&pendingTxPkt, &incoming, sizeof(incoming));
      memcpy(lastSenderMac, mac, 6);
      hasNewTxPacket = true;
    }
  }
}

uint8_t scanWifiChannelForTransmitter() {
  Serial.println("[SCAN] Mencari channel Wi-Fi Transmitter...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid == TARGET_SSID_1 || ssid == TARGET_SSID_2) {
      uint8_t ch = WiFi.channel(i);
      Serial.print("[OK] Ditemukan jaringan '");
      Serial.print(ssid);
      Serial.print("' pada Channel ");
      Serial.println(ch);
      return ch;
    }
  }
  Serial.println("[INFO] SSID tidak terlihat saat scan awal, mulai auto-hop Channel 1..13.");
  return 1;
}

void setReceiverChannel(uint8_t ch) {
  activeChannel = ch;
  esp_wifi_set_channel(activeChannel, WIFI_SECOND_CHAN_NONE);
}

void processIncomingPacket() {
  if (!hasNewTxPacket) return;
  hasNewTxPacket = false;

  VortexTxPacket pkt;
  memcpy(&pkt, (void*)&pendingTxPkt, sizeof(pkt));
  lastTxRecvMs = millis();

  // Pairing otomatis dengan MAC Transmitter
  if (!transmitterPaired) {
    memcpy(transmitterMac, lastSenderMac, 6);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, transmitterMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(transmitterMac)) {
      esp_now_add_peer(&peerInfo);
    }
    transmitterPaired = true;
    Serial.print("[ESP-NOW] Terkunci dengan Transmitter di Channel ");
    Serial.println(activeChannel);
  }

  // Simpan seluruh data sensor dari Transmitter untuk ditampilkan!
  lastSeq     = pkt.seq;
  rxTemp      = pkt.temperature;
  rxHumidity  = pkt.humidity;
  rxPir       = pkt.pir;
  rxMic       = pkt.micLevel;
  rxMicAlert  = pkt.micAlert;
  currentSpeedLevel = constrain(pkt.speed, 1, 3);

  // Proses aksi kalibrasi khusus jika ada
  if (pkt.calAction == 1) {
    saveHomeAnglesToFlash();
  } else if (pkt.calAction == 2) {
    resetHomeAnglesInFlash();
    if (servosAttached) poseStand();
  } else if (pkt.calAction == 3) {
    currentCommand = "RELAX";
    detachAllServos();
  }

  // Proses slider kalibrasi per-servo (hanya menggerakkan 1 servo yang digeser!)
  if (pkt.calServoId >= 0 && pkt.calServoId < 4) {
    int id = pkt.calServoId;
    ensureSingleServoAttached(id);
    servosAttached = true;
    currentCommand = "CAL";
    writeServoRaw(id, pkt.calServoAngle);
    lastCommandMs = millis();
  } else if (pkt.calAction == 0) {
    // Cek perintah mode dari Web -> Transmitter
    String newCmd = String(pkt.command);
    newCmd.trim();
    newCmd.toUpperCase();

    if (newCmd.length() > 0 && newCmd != currentCommand) {
      currentCommand = newCmd;
      waveStep = 0;
      lastWaveStepMs = 0;
      if (currentCommand == "STAND") {
        poseStand();
      } else if (currentCommand == "CROUCH") {
        poseCrouch();
      } else if (currentCommand == "RELAX" || currentCommand == "STOP") {
        currentCommand = "RELAX";
        detachAllServos();
      }
    }
    if (currentCommand == "FORWARD" || currentCommand == "BACKWARD" ||
        currentCommand == "LEFT" || currentCommand == "RIGHT" || currentCommand == "WAVE") {
      lastCommandMs = millis();
    }
  }

  // Kirim konfirmasi & sudut 4 servo kembali ke Transmitter -> Web Dashboard
  sendAckToTransmitter();
}

// ============================================================================
// 8. SETUP & MAIN LOOP RECEIVER
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=======================================================");
  Serial.println("  VORTEX-X // ESP32 #2 RECEIVER (4 SERVO BAWAH + LCD)");
  Serial.println("  Pin Servo: RL=13 | FL=14 | RR=18 | FR=21 (Mode RELAX)");
  Serial.println("=======================================================");

  // 1. Muat titik awal kalibrasi & tahan servo dalam mode RELAX (anti-nyentak & anti-getar)
  loadHomeAnglesFromFlash();
  initServoTimers();
  servosAttached = false;
  currentCommand = "RELAX";

  // 2. Inisialisasi LCD 16x2 I2C (pin kustom SDA=19, SCL=22 agar GPIO 21 aman!)
  initReceiverLcd();

  // 3. Inisialisasi Wi-Fi Station (tanpa konek router, cukup kunci Channel untuk ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  uint8_t detectedCh = scanWifiChannelForTransmitter();
  setReceiverChannel(detectedCh);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERR] Gagal inisialisasi ESP-NOW Receiver!");
    return;
  }
  esp_now_register_recv_cb(onEspNowRecv);

  Serial.println("[READY] Receiver siap menerima perintah Web & data sensor dari Transmitter!");
}

void loop() {
  unsigned long now = millis();

  // 1. Proses data & perintah yang baru masuk dari Transmitter
  processIncomingPacket();

  // 2. Auto-hop channel 1..13 & safety auto-relax jika sinyal TX hilang
  if (now - lastTxRecvMs > 2500) {
    if (servosAttached && lastTxRecvMs > 0 && (now - lastTxRecvMs > 5000)) {
      currentCommand = "RELAX";
      detachAllServos();
    }
    if (now - lastChannelHopMs >= 450) {
      lastChannelHopMs = now;
      uint8_t nextCh = (activeChannel % 13) + 1;
      setReceiverChannel(nextCh);
    }
  }

  // 3. Jalankan animasi melambai-lambaikan 4 servo bawah
  int stepDelay = (currentSpeedLevel == 3) ? 160 : ((currentSpeedLevel == 2) ? 240 : 340);
  if (now - lastWaveStepMs >= stepDelay) {
    lastWaveStepMs = now;
    if (currentCommand == "WAVE" || currentCommand == "FORWARD" ||
        currentCommand == "BACKWARD" || currentCommand == "LEFT" || currentCommand == "RIGHT") {
      runWaveMotion(currentCommand, waveStep);
      waveStep++;
    }
  }

  // 4. Refresh tampilan LCD 16x2 setiap 350 ms
  if (now - lastLcdUpdateMs >= 350) {
    lastLcdUpdateMs = now;
    refreshReceiverDisplay();
  }

  // 5. Log data yang diterima dari Transmitter ke Serial Monitor tiap 1 detik
  static unsigned long lastRxPrintMs = 0;
  if (now - lastRxPrintMs >= 1000) {
    lastRxPrintMs = now;
    bool txOnline = (lastTxRecvMs > 0) && (now - lastTxRecvMs < 3500);
    if (txOnline) {
      Serial.print("[RX DATA] Mode: ");
      Serial.print(currentCommand);
      Serial.print(" | Suhu: ");
      Serial.print(rxTemp, 1);
      Serial.print("C | RH: ");
      Serial.print(rxHumidity, 1);
      Serial.print("% | PIR: ");
      Serial.print(rxPir ? "GERAK!" : "Diam");
      Serial.print(" | Mic: ");
      Serial.print(rxMic);
      Serial.print("% | Servo(RL13/FL14/RR18/FR21): [");
      Serial.print(currentServoAngles[0]); Serial.print(", ");
      Serial.print(currentServoAngles[1]); Serial.print(", ");
      Serial.print(currentServoAngles[2]); Serial.print(", ");
      Serial.print(currentServoAngles[3]); Serial.println("]");
    } else {
      Serial.print("[RX SCAN] Menunggu sinyal Transmitter... (mencoba Channel ");
      Serial.print(activeChannel);
      Serial.println(")");
    }
  }
}
