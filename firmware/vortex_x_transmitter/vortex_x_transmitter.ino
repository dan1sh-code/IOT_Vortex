/*
 ==============================================================================
  PROYEK VORTEX-X // ESP32 #1: TRANSMITTER (I2S MIC INMP441 + SPEAKER MAX98357A
                                            + PIR + DHT11 + OLED/LCD + WI-FI WEB)

  ALUR SISTEM:
  [Web Dashboard] <---(Wi-Fi HTTP)---> [ESP32 TRANSMITTER] <---(ESP-NOW)---> [ESP32 RECEIVER]

  PINOUT HARDWARE SESUAI RANGKAIAN ANDA:
  - Sensor PIR               -> GPIO 19 (D19)
  - Microphone I2S INMP441   -> BCLK: 32 | WS: 33 | SD: 34  (VCC: 3V3)
  - Speaker I2S MAX98357A    -> BCLK: 26 | LRC: 25 | DIN: 22 (VCC: 5V/VIN)
  - Layar I2C (OLED & LCD)   -> SDA: 21  | SCL: 23 (Wire.begin(21, 23))
  - Sensor DHT11 (DATA)      -> GPIO 18 (D18)              (VCC: 3V3)

  FITUR TERINTEGRASI:
  1. Web Server REST API untuk menerima perintah Web Dashboard & mengirim ke Receiver.
  2. Membaca suhu & kelembapan DHT11, deteksi gerak PIR, dan level suara I2S INMP441.
  3. Merekam suara 3 detik dari Mic INMP441 dan memutarnya ke Speaker MAX98357A saat
     PIR mendeteksi gerakan (cooldown 12 detik) ATAU saat ditekan tombol REKAM dari Web.
  4. Menampilkan status lengkap ke OLED (0x3C) dan LCD 16x2 (0x27) jika terpasang di TX,
     sekaligus memancarkan data & perintah via ESP-NOW ke ESP32 Receiver.
 ==============================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LiquidCrystal_I2C.h>

// ============================================================================
// 1. KONFIGURASI WI-FI & URL N8N VOICE AI AGENT
// ============================================================================
const bool USE_ACCESS_POINT = false;

const char* AP_SSID = "VORTEX-X_ROBOT";
const char* AP_PASS = "vortex12345";

const char* STA_SSID = "pribadi";
const char* STA_PASS = "rqfa1234";

// Ganti IP di bawah dengan IP Laptop Anda (cek di CMD: ipconfig), atau ubah langsung dari Web!
String n8nVoiceUrl = "http://10.63.8.214:5678/webhook/vortex-voice";

WebServer server(80);
Preferences prefs;

// ============================================================================
// 2. PINOUT TRANSMITTER (DISESUAIKAN DENGAN SKETCH AUDIO ANDA)
// ============================================================================
#define PIR_PIN  19
#define DHT_PIN  18  // Pin bebas aman untuk DHT11 DATA (tepat di sebelah PIR D19)

// Microphone INMP441 (I2S Digital Mic)
#define MIC_BCLK 32
#define MIC_WS   33
#define MIC_SD   34

// Speaker MAX98357A (I2S Amplifier)
#define SPK_BCLK 26
#define SPK_LRC  25
#define SPK_DIN  22

// Bus I2C Kustom (OLED 0x3C & LCD 0x27 berbagi di SDA=21, SCL=23)
#define I2C_SDA  21
#define I2C_SCL  23

// Konfigurasi Audio I2S
#define SAMPLE_RATE    16000
#define RECORD_SECONDS 3
int numAudioSamples = SAMPLE_RATE * RECORD_SECONDS;

I2SClass I2S;
uint8_t* wavBuffer   = nullptr; // 44 byte header WAV + data PCM
int16_t* audioBuffer = nullptr; // Menunjuk tepat ke wavBuffer + 44
bool recording = false;
bool micStreamActive = false;
bool autoRecordOnPir = true;
unsigned long lastAutoRecordMs = 0;
const unsigned long PIR_RECORD_COOLDOWN_MS = 12000;

// Layar I2C (OLED 0.96" & LCD 16x2 Auto-Detect)
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledReady = false;

LiquidCrystal_I2C* lcd = nullptr;
bool lcdReady = false;
String lcdRow0Cache = "";
String lcdRow1Cache = "";

// Sensor DHT11
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ============================================================================
// 3. STRUKTUR PAKET ESP-NOW (HARUS SAMA PERSIS DENGAN RECEIVER!)
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
  uint8_t micLevel;      // 0..100 %
  uint8_t micAlert;      // 1 jika suara keras / sedang rekam
  int8_t calServoId;     // -1 normal, 0..3 jika kalibrasi
  int16_t calServoAngle; // 15..165
  uint8_t calAction;     // 0: normal, 1: save_home, 2: reset_home, 3: relax
} VortexTxPacket;

typedef struct __attribute__((packed)) {
  uint16_t magic;
  uint32_t ackSeq;
  char activeCmd[16];
  uint8_t attached;      // 1 aktif, 0 RELAX
  uint8_t lcdReady;      // Status LCD di Receiver
  int16_t servos[4];     // [0]:RL(13), [1]:FL(14), [2]:RR(18), [3]:FR(21)
  int16_t homes[4];
} VortexRxPacket;

uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t receiverMac[6]  = {0};
bool receiverPaired     = false;
unsigned long lastRxAckMs = 0;

// ============================================================================
// 4. VARIABEL STATE & TELEMETRI
// ============================================================================
String currentCommand = "RELAX";
int currentSpeedLevel = 2;
String currentIpStr   = "0.0.0.0";
String lastAiReply    = "Siap menerima suara";

float temperatureC = 28.0;
float humidityPct  = 65.0;
int pirState       = 0;
int micLevelPct    = 0;
int micAlertState  = 0;
const int MIC_ALERT_THRESHOLD = 45;
unsigned long lastMicAlertMs = 0;

int rxServos[4] = {90, 90, 90, 90};
int rxHomes[4]  = {90, 90, 90, 90};
bool rxAttached = false;
bool rxLcdReady = false;
uint32_t txSeqCounter = 0;

unsigned long lastSensorReadMs = 0;
unsigned long lastDhtReadMs    = 0;
unsigned long lastTxSendMs     = 0;
unsigned long lastOledAnimMs   = 0;
unsigned long lastLcdUpdateMs  = 0;

// Variabel Mesin Animasi Mata OLED (Smooth 18 FPS)
String oledSelectedMode = "AUTO"; // AUTO, NORMAL, LOOK_AROUND, HAPPY, LOVE, ANGRY, SLEEPY, SCANNER, MUSIC
String oledFaceState    = "SLEEPY";
float curEyeX = 0.0f, curEyeY = 0.0f;
float targetEyeX = 0.0f, targetEyeY = 0.0f;
unsigned long nextLookChangeMs = 0;
unsigned long nextBlinkMs      = 2500;
bool isBlinking = false;
unsigned long blinkStartMs     = 0;

// Prototipe
void sendPacketToReceiver(int8_t calId = -1, int16_t calAngle = 90, uint8_t calAct = 0);

// ============================================================================
// 5. FUNGSI DISPLAY & MESIN ANIMASI MATA OLED SSD1306
// ============================================================================
void updateTxLcd(String row0, String row1) {
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

void initTxDisplays() {
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(40);

  uint8_t oledAddrs[] = {0x3C, 0x3D};
  for (uint8_t addr : oledAddrs) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (oled.begin(SSD1306_SWITCHCAPVCC, addr)) {
        oledReady = true;
        oled.clearDisplay();
        oled.setTextColor(SSD1306_WHITE);
        oled.setTextSize(2);
        oled.setCursor(10, 12);
        oled.println("VORTEX-TX");
        oled.setTextSize(1);
        oled.setCursor(10, 40);
        oled.println("Animated Eyes Ready");
        oled.display();
        Serial.print("[OK] OLED SSD1306 aktif di 0x");
        Serial.println(addr, HEX);
      }
      break;
    }
  }

  uint8_t lcdAddrs[] = {0x27, 0x3F};
  for (uint8_t addr : lcdAddrs) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      lcd = new LiquidCrystal_I2C(addr, 16, 2);
      lcd->init();
      lcd->backlight();
      lcdReady = true;
      updateTxLcd("VORTEX-X VoiceTX", "Initializing... ");
      Serial.print("[OK] LCD 16x2 TX aktif di 0x");
      Serial.println(addr, HEX);
      break;
    }
  }

  // Jika hanya OLED di jalur I2C TX, naikkan ke 400kHz agar animasi super mulus!
  if (oledReady && !lcdReady) {
    Wire.setClock(400000);
  }
}

void determineOledFace() {
  // Jika user memilih ekspresi manual di Web (selain AUTO), pakai pilihan user!
  if (oledSelectedMode != "AUTO") {
    oledFaceState = oledSelectedMode;
    return;
  }

  // Mode AUTO: bereaksi otomatis terhadap sensor PIR, Mic & gerakan robot
  if (recording) {
    oledFaceState = "HAPPY";
  } else if (pirState == HIGH || micAlertState == 1) {
    oledFaceState = "ANGRY";
  } else if (currentCommand == "WAVE") {
    oledFaceState = "HAPPY";
  } else if (currentCommand == "CROUCH" || currentCommand == "RELAX") {
    oledFaceState = "LOOK_AROUND"; // Saat diam/relax mata tetap hidup melirik ramah!
  } else if (currentCommand == "LEFT" || currentCommand == "RIGHT") {
    oledFaceState = "LOOK_AROUND";
  } else if (currentCommand == "FORWARD" || currentCommand == "BACKWARD") {
    oledFaceState = "SCANNER";
  } else {
    oledFaceState = "NORMAL";
  }
}

void drawHeart(int cx, int cy, int r) {
  oled.fillCircle(cx - r / 2, cy - r / 4, r / 2 + 1, SSD1306_WHITE);
  oled.fillCircle(cx + r / 2, cy - r / 4, r / 2 + 1, SSD1306_WHITE);
  oled.fillTriangle(cx - r - 1, cy - 1, cx + r + 1, cy - 1, cx, cy + r + 2, SSD1306_WHITE);
}

void renderOledAnimationFrame() {
  if (!oledReady || recording) return;
  determineOledFace();

  unsigned long now = millis();
  bool rxOnline = (lastRxAckMs > 0) && (now - lastRxAckMs < 3000);

  // Update kedip mata lembut (~tiap 3.2 detik)
  if (!isBlinking && now > nextBlinkMs) {
    isBlinking = true;
    blinkStartMs = now;
    nextBlinkMs = now + 2600 + (now % 1800);
  }
  int blinkClosedPx = 0; // 0 = terbuka penuh
  if (isBlinking) {
    unsigned long elapsed = now - blinkStartMs;
    if (elapsed < 90) blinkClosedPx = map((long)elapsed, 0, 90, 0, 20);
    else if (elapsed < 180) blinkClosedPx = map((long)elapsed, 90, 180, 20, 0);
    else isBlinking = false;
  }

  // Update target gerakan melirik halus
  if (oledFaceState == "LOOK_AROUND") {
    if (now > nextLookChangeMs) {
      nextLookChangeMs = now + 1100 + (now % 900);
      int pattern = (now / 1000) % 5;
      if (pattern == 0)      { targetEyeX = -15.0f; targetEyeY = 0.0f; }
      else if (pattern == 1) { targetEyeX = 15.0f;  targetEyeY = -2.0f; }
      else if (pattern == 2) { targetEyeX = 0.0f;   targetEyeY = -6.0f; }
      else if (pattern == 3) { targetEyeX = -10.0f; targetEyeY = 4.0f; }
      else                   { targetEyeX = 0.0f;   targetEyeY = 0.0f; }
    }
  } else if (currentCommand == "LEFT") {
    targetEyeX = -14.0f; targetEyeY = 0.0f;
  } else if (currentCommand == "RIGHT") {
    targetEyeX = 14.0f; targetEyeY = 0.0f;
  } else {
    targetEyeX = 0.0f; targetEyeY = 0.0f;
  }

  // Smooth interpolation (ease-out) agar mata meluncur mulus!
  curEyeX += (targetEyeX - curEyeX) * 0.30f;
  curEyeY += (targetEyeY - curEyeY) * 0.30f;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);

  // Header Bar (y = 0..9)
  oled.setCursor(2, 1);
  oled.print("RX:");
  oled.print(rxOnline ? "OK" : "..");
  oled.setCursor(44, 1);
  oled.print(oledFaceState);
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  int baseLX = 25 + (int)curEyeX;
  int baseRX = 73 + (int)curEyeX;
  int baseY  = 17 + (int)curEyeY;
  int ew = 30;
  int eh = 24;

  if (oledFaceState == "LOVE") {
    // Dua Hati berdenyut sesuai detak!
    int pulseR = ((now / 150) % 4 < 2) ? 10 : 8;
    drawHeart(40, 29, pulseR);
    drawHeart(88, 29, pulseR);
  } else if (oledFaceState == "HAPPY") {
    // Mata melengkung ceria memantul + kilauan bintang
    int bounce = (int)(sinf(now * 0.01f) * 3.0f);
    oled.fillCircle(40, 32 + bounce, 14, SSD1306_WHITE);
    oled.fillCircle(40, 38 + bounce, 12, SSD1306_BLACK);
    oled.fillRect(24, 34 + bounce, 32, 16, SSD1306_BLACK);

    oled.fillCircle(88, 32 + bounce, 14, SSD1306_WHITE);
    oled.fillCircle(88, 38 + bounce, 12, SSD1306_BLACK);
    oled.fillRect(72, 34 + bounce, 32, 16, SSD1306_BLACK);

    // Kilauan bintang kecil bergantian
    if ((now / 250) % 2 == 0) {
      oled.drawLine(10, 18, 16, 18, SSD1306_WHITE); oled.drawLine(13, 15, 13, 21, SSD1306_WHITE);
      oled.drawLine(112, 22, 118, 22, SSD1306_WHITE); oled.drawLine(115, 19, 115, 25, SSD1306_WHITE);
    }
  } else if (oledFaceState == "ANGRY") {
    // Mata kotak dengan alis tajam miring & sedikit getar agresif
    int shake = ((now / 80) % 2 == 0) ? 1 : -1;
    oled.fillRoundRect(25 + shake, 18, 30, 23, 5, SSD1306_WHITE);
    oled.fillRoundRect(73 + shake, 18, 30, 23, 5, SSD1306_WHITE);
    // Potong miring bagian atas dalam untuk alis marah
    oled.fillTriangle(23, 15, 58, 15, 58, 28, SSD1306_BLACK);
    oled.fillTriangle(105, 15, 70, 15, 70, 28, SSD1306_BLACK);
  } else if (oledFaceState == "SLEEPY") {
    // Kelopak setengah turun + huruf Zzz melayang naik
    oled.fillRoundRect(25, 28, 30, 6, 2, SSD1306_WHITE);
    oled.fillRoundRect(73, 28, 30, 6, 2, SSD1306_WHITE);
    int zStep = (now / 140) % 18;
    oled.setCursor(105, 34 - zStep);
    oled.print("z");
    oled.setCursor(113, 26 - (zStep / 2));
    oled.print("Z");
  } else if (oledFaceState == "SCANNER") {
    // Visor Cyber bergerak kanan-kiri seperti radar!
    oled.drawRoundRect(16, 18, 96, 20, 6, SSD1306_WHITE);
    float norm = (sinf(now * 0.007f) + 1.0f) * 0.5f; // 0..1
    int scanX = 20 + (int)(norm * 68.0f);
    oled.fillRoundRect(scanX, 21, 22, 14, 4, SSD1306_WHITE);
  } else if (oledFaceState == "MUSIC") {
    // Mata menari membesar sesuai level suara Mic INMP441 + 8 Bar Equalizer!
    int danceH = constrain(8 + (micLevelPct / 5), 6, 24);
    int danceY = 29 - (danceH / 2);
    oled.fillRoundRect(24, danceY, 26, danceH, 5, SSD1306_WHITE);
    oled.fillRoundRect(78, danceY, 26, danceH, 5, SSD1306_WHITE);
    for (int b = 0; b < 5; b++) {
      int barH = constrain(4 + ((micLevelPct + (b * 17) + (now / 70)) % 18), 3, 20);
      oled.fillRect(54 + b * 4, 41 - barH, 3, barH, SSD1306_WHITE);
    }
  } else {
    // NORMAL atau LOOK_AROUND: Mata membulat halus dengan efek kedip natural
    int curH = max(4, eh - blinkClosedPx);
    int curY = baseY + (eh - curH) / 2;
    oled.fillRoundRect(baseLX, curY, ew, curH, curH > 10 ? 7 : 2, SSD1306_WHITE);
    oled.fillRoundRect(baseRX, curY, ew, curH, curH > 10 ? 7 : 2, SSD1306_WHITE);

    // Kilau pupil kecil ketika mata terbuka lebar
    if (curH > 14) {
      oled.fillCircle(baseLX + 10 + (int)(curEyeX * 0.25f), curY + 7, 2, SSD1306_BLACK);
      oled.fillCircle(baseRX + 10 + (int)(curEyeX * 0.25f), curY + 7, 2, SSD1306_BLACK);
    }
  }

  // Footer Bar (Mic bar + DHT + IP)
  oled.drawLine(0, 46, 127, 46, SSD1306_WHITE);
  oled.setCursor(2, 48);
  oled.print("MIC:");
  int barW = map(micLevelPct, 0, 100, 0, 44);
  oled.drawRect(28, 48, 46, 6, SSD1306_WHITE);
  oled.fillRect(29, 49, barW, 4, SSD1306_WHITE);
  oled.setCursor(80, 48);
  oled.print((int)temperatureC);
  oled.print("C ");
  oled.print((int)humidityPct);
  oled.print("%");

  oled.setCursor(2, 56);
  oled.print("IP:");
  oled.print(currentIpStr);
  oled.display();
}

void renderTxLcdOnly() {
  if (!lcdReady || recording) return;
  bool rxOnline = (lastRxAckMs > 0) && (millis() - lastRxAckMs < 3000);
  String r0 = "T:" + String(temperatureC, 1) + "C H:" + String((int)humidityPct) + "% M:" + String(micLevelPct);
  String r1;
  if (pirState == HIGH) r1 = "PIR:GERAK [" + currentCommand + "]";
  else if ((millis() / 4000) % 2 == 0) r1 = "IP:" + currentIpStr;
  else r1 = "RX:" + String(rxOnline ? "OK" : "..") + " [" + oledFaceState + "]";
  updateTxLcd(r0, r1);
}

// ============================================================================
// 6. KENDALI AUDIO I2S (INMP441 MIC + MAX98357A SPEAKER)
// ============================================================================
bool startMicrophone() {
  if (micStreamActive) return true;
  I2S.setPins(MIC_BCLK, MIC_WS, -1, MIC_SD);
  micStreamActive = I2S.begin(
    I2S_MODE_STD,
    SAMPLE_RATE,
    I2S_DATA_BIT_WIDTH_32BIT,
    I2S_SLOT_MODE_MONO
  );
  return micStreamActive;
}

void stopI2SStream() {
  I2S.end();
  micStreamActive = false;
}

bool startSpeakerWithRate(uint32_t rateHz, uint8_t channels = 1) {
  if (micStreamActive) stopI2SStream();
  I2S.setPins(SPK_BCLK, SPK_LRC, SPK_DIN, -1, -1);
  return I2S.begin(
    I2S_MODE_STD,
    rateHz,
    I2S_DATA_BIT_WIDTH_16BIT,
    channels == 2 ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO
  );
}

bool startSpeaker() {
  return startSpeakerWithRate(SAMPLE_RATE, 1);
}

// Memainkan nada gelombang sinus murni ke Speaker MAX98357A (pengganti tone!)
void playSpeakerToneI2S(int freqHz, int durationMs) {
  if (recording) return;
  stopI2SStream();
  if (!startSpeaker()) {
    Serial.println("[ERR] Speaker MAX98357A gagal inisialisasi.");
    startMicrophone();
    return;
  }

  const int chunkSamples = 256;
  int16_t buf[chunkSamples];
  int totalSamples = (SAMPLE_RATE * durationMs) / 1000;
  int written = 0;
  float phaseStep = (2.0f * PI * freqHz) / SAMPLE_RATE;
  float phase = 0.0f;

  while (written < totalSamples) {
    int count = min(chunkSamples, totalSamples - written);
    for (int i = 0; i < count; i++) {
      buf[i] = (int16_t)(sinf(phase) * 7500.0f);
      phase += phaseStep;
      if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    }
    I2S.write((uint8_t*)buf, count * sizeof(int16_t));
    written += count;
  }

  I2S.end();
  startMicrophone();
}

// Membaca level suara non-blocking dari I2S INMP441 agar OLED & Web tampil live dB bar
void sampleMicLevelQuick() {
  if (recording || !micStreamActive) return;
  int avail = I2S.available();
  if (avail < (int)sizeof(int32_t)) return;

  int samplesAvail = avail / (int)sizeof(int32_t);
  int samplesToRead = min(samplesAvail, 64);
  int32_t maxAbs = 0;

  for (int i = 0; i < samplesToRead; i++) {
    int32_t sample = 0;
    if (I2S.readBytes((char*)&sample, sizeof(sample)) == sizeof(sample)) {
      int16_t pcm = (int16_t)(sample >> 16);
      int32_t a = abs((int32_t)pcm);
      if (a > maxAbs) maxAbs = a;
    }
  }
  int mapped = map(constrain((int)maxAbs, 0, 12000), 0, 12000, 0, 100);
  micLevelPct = (mapped * 7 + micLevelPct * 3) / 10;

  if (micLevelPct >= MIC_ALERT_THRESHOLD) {
    micAlertState = 1;
    lastMicAlertMs = millis();
  } else if (millis() - lastMicAlertMs > 1800) {
    micAlertState = 0;
  }
}

// Menulis 44-byte Header WAV PCM 16-bit Mono agar diterima langsung oleh Groq Whisper STT di n8n
void writeWavHeader(uint8_t* hdr, uint32_t pcmDataBytes, uint32_t sampleRate) {
  uint32_t fileSizeMinus8 = pcmDataBytes + 36;
  uint32_t byteRate = sampleRate * 2; // 16-bit mono = 2 byte per sampel

  memcpy(hdr + 0,  "RIFF", 4);
  hdr[4]  = (uint8_t)(fileSizeMinus8 & 0xFF);
  hdr[5]  = (uint8_t)((fileSizeMinus8 >> 8) & 0xFF);
  hdr[6]  = (uint8_t)((fileSizeMinus8 >> 16) & 0xFF);
  hdr[7]  = (uint8_t)((fileSizeMinus8 >> 24) & 0xFF);
  memcpy(hdr + 8,  "WAVE", 4);
  memcpy(hdr + 12, "fmt ", 4);
  hdr[16] = 16; hdr[17] = 0; hdr[18] = 0; hdr[19] = 0; // Subchunk1Size = 16
  hdr[20] = 1;  hdr[21] = 0;                           // AudioFormat = 1 (PCM)
  hdr[22] = 1;  hdr[23] = 0;                           // NumChannels = 1 (Mono)
  hdr[24] = (uint8_t)(sampleRate & 0xFF);
  hdr[25] = (uint8_t)((sampleRate >> 8) & 0xFF);
  hdr[26] = (uint8_t)((sampleRate >> 16) & 0xFF);
  hdr[27] = (uint8_t)((sampleRate >> 24) & 0xFF);
  hdr[28] = (uint8_t)(byteRate & 0xFF);
  hdr[29] = (uint8_t)((byteRate >> 8) & 0xFF);
  hdr[30] = (uint8_t)((byteRate >> 16) & 0xFF);
  hdr[31] = (uint8_t)((byteRate >> 24) & 0xFF);
  hdr[32] = 2;  hdr[33] = 0;                           // BlockAlign = 2
  hdr[34] = 16; hdr[35] = 0;                           // BitsPerSample = 16
  memcpy(hdr + 36, "data", 4);
  hdr[40] = (uint8_t)(pcmDataBytes & 0xFF);
  hdr[41] = (uint8_t)((pcmDataBytes >> 8) & 0xFF);
  hdr[42] = (uint8_t)((pcmDataBytes >> 16) & 0xFF);
  hdr[43] = (uint8_t)((pcmDataBytes >> 24) & 0xFF);
}

// Sink Stream untuk HTTPClient::writeToStream() agar Transfer-Encoding: chunked otomatis terurai
class WavI2SSink : public Stream {
public:
  enum ParseState { HDR_RIFF, SUB_HDR, SUB_FMT, SUB_SKIP, STREAM_PCM, DONE_WAV };
  ParseState state = HDR_RIFF;
  uint8_t  hdrBuf[36];
  size_t   hdrPos = 0;
  uint32_t subRem = 0;
  uint32_t fmtReadLimit = 0;
  uint32_t dataRem = 0xFFFFFFFF;
  uint16_t numChannels = 1;
  uint32_t wavSampleRate = 24000;
  bool     headerValid = false;
  bool     speakerOk = false;
  bool     hasOddByte = false;
  uint8_t  oddByte = 0;
  uint32_t bytesPlayed = 0;
  uint32_t totalBytesSeen = 0;

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

  size_t write(uint8_t b) override {
    return write(&b, 1);
  }

  size_t write(const uint8_t* buf, size_t len) override {
    if (buf == nullptr || len == 0) return 0;
    size_t pos = 0;
    totalBytesSeen += (uint32_t)len;

    while (pos < len) {
      if (state == DONE_WAV) {
        return len; // Konsumsi sisa byte agar koneksi HTTP selesai rapi
      }

      if (state == HDR_RIFF) {
        size_t need = 12 - hdrPos;
        size_t take = min(need, len - pos);
        memcpy(hdrBuf + hdrPos, buf + pos, take);
        hdrPos += take;
        pos += take;
        if (hdrPos == 12) {
          if (memcmp(hdrBuf, "RIFF", 4) != 0 || memcmp(hdrBuf + 8, "WAVE", 4) != 0) {
            Serial.printf("[AI ERR] Header bukan RIFF/WAVE: %02X %02X %02X %02X\n",
                          hdrBuf[0], hdrBuf[1], hdrBuf[2], hdrBuf[3]);
            state = DONE_WAV;
          } else {
            headerValid = true;
            hdrPos = 0;
            state = SUB_HDR;
          }
        }
      } else if (state == SUB_HDR) {
        size_t need = 8 - hdrPos;
        size_t take = min(need, len - pos);
        memcpy(hdrBuf + hdrPos, buf + pos, take);
        hdrPos += take;
        pos += take;
        if (hdrPos == 8) {
          uint32_t chunkSize = (uint32_t)hdrBuf[4] | ((uint32_t)hdrBuf[5] << 8) |
                               ((uint32_t)hdrBuf[6] << 16) | ((uint32_t)hdrBuf[7] << 24);
          if (memcmp(hdrBuf, "fmt ", 4) == 0 && chunkSize >= 16) {
            subRem = chunkSize;
            fmtReadLimit = min((uint32_t)sizeof(hdrBuf), chunkSize);
            hdrPos = 0;
            state = SUB_FMT;
          } else if (memcmp(hdrBuf, "data", 4) == 0) {
            dataRem = (chunkSize == 0) ? 0xFFFFFFFF : chunkSize;
            Serial.printf("[AI VOICE] Mulai Speaker %lu Hz, ch=%u, data=%lu B\n",
                          (unsigned long)wavSampleRate, numChannels, (unsigned long)dataRem);
            speakerOk = startSpeakerWithRate(wavSampleRate, (uint8_t)numChannels);
            state = speakerOk ? STREAM_PCM : DONE_WAV;
          } else {
            subRem = chunkSize;
            state = (subRem == 0) ? SUB_HDR : SUB_SKIP;
            hdrPos = 0;
          }
        }
      } else if (state == SUB_FMT) {
        while (pos < len && subRem > 0) {
          if (hdrPos < fmtReadLimit) {
            hdrBuf[hdrPos++] = buf[pos];
          }
          pos++;
          subRem--;
        }
        if (subRem == 0) {
          numChannels = (uint16_t)hdrBuf[2] | ((uint16_t)hdrBuf[3] << 8);
          wavSampleRate = (uint32_t)hdrBuf[4] | ((uint32_t)hdrBuf[5] << 8) |
                          ((uint32_t)hdrBuf[6] << 16) | ((uint32_t)hdrBuf[7] << 24);
          if (wavSampleRate < 8000 || wavSampleRate > 48000) wavSampleRate = 24000;
          if (numChannels < 1 || numChannels > 2) numChannels = 1;
          hdrPos = 0;
          state = SUB_HDR;
        }
      } else if (state == SUB_SKIP) {
        size_t skip = min((size_t)subRem, len - pos);
        pos += skip;
        subRem -= (uint32_t)skip;
        if (subRem == 0) {
          hdrPos = 0;
          state = SUB_HDR;
        }
      } else if (state == STREAM_PCM) {
        size_t availInBuf = len - pos;
        if (dataRem != 0xFFFFFFFF && availInBuf > dataRem) {
          availInBuf = dataRem;
        }
        if (availInBuf == 0) {
          state = DONE_WAV;
          continue;
        }

        // Pastikan setiap blok I2S.write kelipatan 2 byte (sampel 16-bit utuh)
        if (hasOddByte) {
          uint8_t pair[2] = { oddByte, buf[pos] };
          I2S.write(pair, 2);
          bytesPlayed += 2;
          hasOddByte = false;
          pos++;
          availInBuf--;
          if (dataRem != 0xFFFFFFFF) dataRem--;
        }

        size_t evenLen = availInBuf & ~((size_t)1);
        size_t offset = 0;
        while (offset < evenLen) {
          size_t chunk = min((size_t)512, evenLen - offset);
          I2S.write((uint8_t*)(buf + pos + offset), chunk);
          bytesPlayed += (uint32_t)chunk;
          offset += chunk;
        }
        pos += evenLen;
        if (dataRem != 0xFFFFFFFF) dataRem -= (uint32_t)evenLen;

        if ((availInBuf & 1) != 0) {
          oddByte = buf[pos++];
          hasOddByte = true;
          if (dataRem != 0xFFFFFFFF) dataRem--;
        }
        if (dataRem == 0) {
          state = DONE_WAV;
        }
      }
    }
    return len;
  }

  void finish() {
    if (speakerOk) {
      uint8_t silence[256] = {0};
      I2S.write(silence, sizeof(silence));
      I2S.end();
    }
  }
};

// Rekam dari Mic INMP441 -> Kirim WAV ke AI Agent n8n -> Putar Jawaban AI ke Speaker MAX98357A!
void recordAndPlayAudio() {
  if (audioBuffer == nullptr || wavBuffer == nullptr || recording) return;
  recording = true;
  lastAutoRecordMs = millis();

  String prevCmd = currentCommand;
  currentCommand = "REC";
  sendPacketToReceiver();

  if (lcdReady) updateTxLcd("Recording...", "Speak now (3s)");
  if (oledReady) {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(10, 16);
    oled.println("MEREKAM...");
    oled.setTextSize(1);
    oled.setCursor(12, 44);
    oled.println("Mic INMP441 Aktif");
    oled.display();
  }
  Serial.println("[AUDIO] Mulai merekam suara dari INMP441...");

  if (!startMicrophone()) {
    if (lcdReady) updateTxLcd("Mic Error", "Check wiring");
    Serial.println("[ERR] Microphone INMP441 gagal inisialisasi.");
    recording = false;
    currentCommand = prevCmd;
    return;
  }

  for (int i = 0; i < numAudioSamples; i++) {
    int32_t sample = 0;
    if (I2S.readBytes((char*)&sample, sizeof(sample)) != sizeof(sample)) {
      Serial.println("[ERR] Gagal baca data I2S Mic.");
      break;
    }
    // Gain digital 4x agar suara jarak 30-60cm jelas terbaca Whisper STT tanpa clipping
    int32_t amplified = sample >> 14;
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32767) amplified = -32767;
    audioBuffer[i] = (int16_t)amplified;
  }
  stopI2SStream();

  // Siapkan header WAV 44-byte tepat di depan audioBuffer
  uint32_t pcmBytes = (uint32_t)numAudioSamples * sizeof(int16_t);
  writeWavHeader(wavBuffer, pcmBytes, SAMPLE_RATE);

  Serial.print("[AI] Mengirim ");
  Serial.print(pcmBytes + 44);
  Serial.print(" byte WAV ke n8n AI Agent: ");
  Serial.println(n8nVoiceUrl);

  currentCommand = "THINK";
  sendPacketToReceiver();
  if (lcdReady) updateTxLcd("Tanya AI Agent..", "Menunggu n8n...");
  if (oledReady) {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(6, 16);
    oled.println("AI MIKIR..");
    oled.setTextSize(1);
    oled.setCursor(8, 44);
    oled.println("Groq Agent + TTS");
    oled.display();
  }

  bool aiSpokeOk = false;
  if (WiFi.status() == WL_CONNECTED && n8nVoiceUrl.startsWith("http")) {
    HTTPClient http;
    String reqUrl = n8nVoiceUrl + "?temp=" + String(temperatureC, 1) +
                    "&hum=" + String(humidityPct, 1) + "&pir=" + String(pirState);
    http.begin(reqUrl);
    http.setTimeout(25000);
    http.addHeader("Content-Type", "audio/wav");

    const char* collectHdrs[] = {"X-Vortex-Cmd", "X-Vortex-Face", "X-Vortex-Text"};
    http.collectHeaders(collectHdrs, 3);

    int httpCode = http.POST(wavBuffer, pcmBytes + 44);
    if (httpCode == 200) {
      String aiCmd  = http.header("X-Vortex-Cmd");
      String aiFace = http.header("X-Vortex-Face");
      String aiText = http.header("X-Vortex-Text");

      aiCmd.trim();  aiCmd.toUpperCase();
      aiFace.trim(); aiFace.toUpperCase();
      aiText.trim();

      if (aiText.length() > 0) {
        lastAiReply = aiText;
        Serial.print("[AI JAWAB] ");
        Serial.println(lastAiReply);
      }

      if (aiFace.length() > 0 && oledSelectedMode == "AUTO") {
        oledFaceState = aiFace;
      }

      // Jika AI memerintahkan robot bergerak (misal WAVE / STAND), langsung kirim ke Receiver!
      if (aiCmd == "WAVE" || aiCmd == "FORWARD" || aiCmd == "BACKWARD" ||
          aiCmd == "LEFT" || aiCmd == "RIGHT" || aiCmd == "STAND" ||
          aiCmd == "CROUCH" || aiCmd == "RELAX") {
        prevCmd = aiCmd;
        currentCommand = aiCmd;
        sendPacketToReceiver(-1, 90, (aiCmd == "RELAX") ? 3 : 0);
        Serial.print("[AI AKSI] Mengirim perintah ke Receiver: ");
        Serial.println(aiCmd);
      } else {
        currentCommand = "PLAY";
        sendPacketToReceiver();
      }

      if (lcdReady) updateTxLcd("AI Menjawab...", aiText.length() > 0 ? aiText.substring(0, 16) : "Playing Voice");
      if (oledReady) {
        oled.clearDisplay();
        oled.setTextSize(2);
        oled.setCursor(6, 16);
        oled.println("AI BICARA");
        oled.setTextSize(1);
        oled.setCursor(6, 44);
        oled.println(lastAiReply.substring(0, 20));
        oled.display();
      }

      WavI2SSink wavSink;
      int streamRes = http.writeToStream(&wavSink);
      wavSink.finish();
      Serial.printf("[AI VOICE] writeToStream=%d totalSeen=%lu headerOk=%d spkOk=%d played=%lu\n",
                    streamRes, (unsigned long)wavSink.totalBytesSeen,
                    wavSink.headerValid ? 1 : 0, wavSink.speakerOk ? 1 : 0,
                    (unsigned long)wavSink.bytesPlayed);
      aiSpokeOk = (streamRes >= 0 || wavSink.bytesPlayed > 0) && wavSink.headerValid && wavSink.speakerOk && (wavSink.bytesPlayed > 0);
    } else {
      Serial.print("[AI ERR] Gagal hubungi n8n HTTP ");
      Serial.print(httpCode);
      Serial.print(" (");
      Serial.print(http.errorToString(httpCode));
      Serial.println("). Cek IP Laptop, n8n Active & Windows Firewall port 5678!");
    }
    http.end();
  }

  // Fallback jika n8n sedang offline: putar rekaman lokal sendiri agar tetap responsif
  if (!aiSpokeOk) {
    Serial.println("[AUDIO] n8n offline -> Fallback memutar rekaman Mic lokal di MAX98357A.");
    currentCommand = "PLAY";
    sendPacketToReceiver();
    if (lcdReady) updateTxLcd("n8n Offline!", "Echo Lokal MAX..");
    if (startSpeaker()) {
      I2S.write((uint8_t*)audioBuffer, pcmBytes);
      I2S.end();
    }
  }

  currentCommand = prevCmd;
  sendPacketToReceiver();
  startMicrophone();
  recording = false;
}

// ============================================================================
// 7. ESP-NOW WIRELESS LINK (TRANSMITTER <-> RECEIVER)
// ============================================================================
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t *mac = info->src_addr;
#else
void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len == sizeof(VortexRxPacket)) {
    VortexRxPacket rx;
    memcpy(&rx, data, sizeof(rx));
    if (rx.magic == PKT_MAGIC_RX) {
      lastRxAckMs = millis();
      rxAttached = (rx.attached == 1);
      rxLcdReady = (rx.lcdReady == 1);
      for (int i = 0; i < 4; i++) {
        rxServos[i] = rx.servos[i];
        rxHomes[i]  = rx.homes[i];
      }
      if (!receiverPaired) {
        memcpy(receiverMac, mac, 6);
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, receiverMac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        if (!esp_now_is_peer_exist(receiverMac)) {
          esp_now_add_peer(&peerInfo);
        }
        receiverPaired = true;
        Serial.println("[ESP-NOW] Receiver terkunci & siap!");
      }
    }
  }
}

void initEspNowTx() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERR] Gagal inisialisasi ESP-NOW!");
    return;
  }
  esp_now_register_recv_cb(onEspNowRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (!esp_now_is_peer_exist(broadcastMac)) {
    esp_now_add_peer(&peerInfo);
  }
  Serial.println("[ESP-NOW] Transmitter aktif (Auto-Pairing).");
}

void sendPacketToReceiver(int8_t calId, int16_t calAngle, uint8_t calAct) {
  VortexTxPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.magic = PKT_MAGIC_TX;
  pkt.seq = ++txSeqCounter;
  strncpy(pkt.command, currentCommand.c_str(), sizeof(pkt.command) - 1);
  pkt.speed = (uint8_t)currentSpeedLevel;
  pkt.temperature = temperatureC;
  pkt.humidity = humidityPct;
  pkt.pir = (uint8_t)pirState;
  pkt.micLevel = (uint8_t)micLevelPct;
  pkt.micAlert = (uint8_t)(micAlertState || recording);
  pkt.calServoId = calId;
  pkt.calServoAngle = calAngle;
  pkt.calAction = calAct;

  const uint8_t* target = receiverPaired ? receiverMac : broadcastMac;
  esp_now_send(target, (uint8_t*)&pkt, sizeof(pkt));
}

// ============================================================================
// 8. WEB SERVER REST API (PINTU MASUK DARI WEB DASHBOARD)
// ============================================================================
void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

String buildTelemetryJson() {
  determineOledFace();
  bool rxOnline = (lastRxAckMs > 0) && (millis() - lastRxAckMs < 3000);

  String json = "{";
  json += "\"device\":\"VORTEX-X-TX-VOICE\",";
  json += "\"status\":\"OK\",";
  json += "\"command\":\"" + currentCommand + "\",";
  json += "\"speed\":" + String(currentSpeedLevel) + ",";
  json += "\"temperature\":" + String(temperatureC, 1) + ",";
  json += "\"humidity\":" + String(humidityPct, 1) + ",";
  json += "\"pir\":" + String(pirState) + ",";
  json += "\"micLevel\":" + String(micLevelPct) + ",";
  json += "\"micAlert\":" + String(micAlertState) + ",";
  json += "\"oled\":" + String(oledReady ? "true" : "false") + ",";
  json += "\"lcd\":" + String((rxLcdReady || lcdReady) ? "true" : "false") + ",";
  json += "\"rxConnected\":" + String(rxOnline ? "true" : "false") + ",";
  json += "\"attached\":" + String(rxAttached ? "true" : "false") + ",";
  String safeReply = lastAiReply;
  safeReply.replace("\"", "'");
  safeReply.replace("\n", " ");

  json += "\"oledMode\":\"" + oledSelectedMode + "\",";
  json += "\"oledFace\":\"" + oledFaceState + "\",";
  json += "\"ip\":\"" + currentIpStr + "\",";
  json += "\"n8nVoiceUrl\":\"" + n8nVoiceUrl + "\",";
  json += "\"lastAiReply\":\"" + safeReply + "\",";

  json += "\"servos\":[";
  for (int i = 0; i < 4; i++) {
    json += String(rxServos[i]);
    if (i < 3) json += ",";
  }
  json += "],";

  json += "\"homes\":[";
  for (int i = 0; i < 4; i++) {
    json += String(rxHomes[i]);
    if (i < 3) json += ",";
  }
  json += "],";

  json += "\"safety\":\"" + String((pirState || micAlertState) ? "ALERT" : "SAFE") + "\"";
  json += "}";
  return json;
}

void handleCommand() {
  handleCORS();
  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }

  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    cmd.toUpperCase();

    if (server.hasArg("speed")) {
      currentSpeedLevel = constrain(server.arg("speed").toInt(), 1, 3);
    }

    if (cmd == "BEEP") {
      // Uji suara jernih lewat MAX98357A
      playSpeakerToneI2S(1046, 120);
      delay(30);
      playSpeakerToneI2S(1568, 150);
      server.send(200, "application/json", buildTelemetryJson());
      return;
    }

    if (cmd == "RECORD") {
      server.send(200, "application/json", buildTelemetryJson());
      recordAndPlayAudio();
      return;
    }

    if (cmd == "AUTOREC_ON") { autoRecordOnPir = true; server.send(200, "application/json", buildTelemetryJson()); return; }
    if (cmd == "AUTOREC_OFF") { autoRecordOnPir = false; server.send(200, "application/json", buildTelemetryJson()); return; }

    if (cmd == "STOP" || cmd == "RELAX") {
      currentCommand = "RELAX";
    } else {
      currentCommand = cmd;
    }

    sendPacketToReceiver(-1, 90, (currentCommand == "RELAX") ? 3 : 0);
    server.send(200, "application/json", buildTelemetryJson());
  } else {
    server.send(400, "application/json", "{\"error\":\"Parameter 'cmd' diperlukan\"}");
  }
}

void handleServoCalibration() {
  handleCORS();
  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }

  if (server.hasArg("action")) {
    String act = server.arg("action");
    if (act == "save_home") sendPacketToReceiver(-1, 90, 1);
    else if (act == "reset_home") sendPacketToReceiver(-1, 90, 2);
    else if (act == "relax") {
      currentCommand = "RELAX";
      sendPacketToReceiver(-1, 90, 3);
    }
    server.send(200, "application/json", buildTelemetryJson());
    return;
  }

  if (server.hasArg("id") && server.hasArg("angle")) {
    int id = server.arg("id").toInt();
    int angle = constrain(server.arg("angle").toInt(), 15, 165);
    if (id >= 0 && id < 4) {
      currentCommand = "CAL";
      rxServos[id] = angle;
      sendPacketToReceiver((int8_t)id, (int16_t)angle, 0);
      server.send(200, "application/json", buildTelemetryJson());
    } else {
      server.send(400, "application/json", "{\"error\":\"ID servo Receiver harus 0-3\"}");
    }
    return;
  }

  server.send(400, "application/json", "{\"error\":\"Parameter tidak valid\"}");
}

void handleTelemetry() {
  handleCORS();
  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }
  server.send(200, "application/json", buildTelemetryJson());
}

// Endpoint pengubah animasi & ekspresi mata OLED dari Web (/face?mode=LOVE)
void handleFace() {
  handleCORS();
  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    mode.toUpperCase();
    oledSelectedMode = mode;
    Serial.print("[OLED] Mode animasi mata diubah ke: ");
    Serial.println(oledSelectedMode);
    renderOledAnimationFrame();
    server.send(200, "application/json", buildTelemetryJson());
  } else {
    server.send(400, "application/json", "{\"error\":\"Parameter 'mode' diperlukan\"}");
  }
}

// Endpoint pengubah URL Webhook Voice AI n8n secara dinamis dari Web (/config?n8n=http://...)
void handleConfig() {
  handleCORS();
  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }
  if (server.hasArg("n8n")) {
    String url = server.arg("n8n");
    url.trim();
    if (url.startsWith("http")) {
      n8nVoiceUrl = url;
      prefs.begin("vortex_tx", false);
      prefs.putString("n8n_url", n8nVoiceUrl);
      prefs.end();
      Serial.print("[CFG] URL Voice AI n8n disimpan: ");
      Serial.println(n8nVoiceUrl);
      server.send(200, "application/json", buildTelemetryJson());
      return;
    }
  }
  server.send(400, "application/json", "{\"error\":\"Parameter 'n8n' harus diawali http://\"}");
}

// ============================================================================
// 9. SETUP & LOOP UTAMA TRANSMITTER
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=======================================================");
  Serial.println("  VORTEX-X // ESP32 #1 TRANSMITTER (VOICE AI N8N + WEB)");
  Serial.println("  PIR=19 | DHT=18 | I2C=21,23 | MIC=32,33,34 | SPK=26,25,22");
  Serial.println("=======================================================");

  pinMode(PIR_PIN, INPUT);

  // Muat URL Voice AI n8n yang tersimpan di Flash
  prefs.begin("vortex_tx", true);
  String savedUrl = prefs.getString("n8n_url", "");
  prefs.end();
  if (savedUrl.startsWith("http")) {
    n8nVoiceUrl = savedUrl;
  }

  // Alokasikan buffer rekaman WAV (44B header + PCM) terlebih dahulu agar memori lega
  wavBuffer = (uint8_t*)malloc(44 + numAudioSamples * sizeof(int16_t));
  if (wavBuffer == nullptr) {
    numAudioSamples = SAMPLE_RATE * 2; // Cadangan 2 detik
    wavBuffer = (uint8_t*)malloc(44 + numAudioSamples * sizeof(int16_t));
  }
  if (wavBuffer != nullptr) {
    audioBuffer = (int16_t*)(wavBuffer + 44);
    Serial.print("[OK] Buffer WAV dialokasikan (");
    Serial.print((44 + numAudioSamples * sizeof(int16_t)) / 1024);
    Serial.println(" KB).");
  } else {
    Serial.println("[WARN] Memori penuh! Fitur rekam dinonaktifkan agar Wi-Fi tetap jalan.");
  }

  initTxDisplays();
  dht.begin();

  // Wi-Fi untuk Web Dashboard
  if (USE_ACCESS_POINT) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.softAP(AP_SSID, AP_PASS);
    currentIpStr = WiFi.softAPIP().toString();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.print("[WIFI] Menghubungkan ke Wi-Fi: ");
    Serial.println(STA_SSID);
    while (WiFi.status() != WL_CONNECTED) {
      delay(400);
      Serial.print(".");
    }
    currentIpStr = WiFi.localIP().toString();
    Serial.println("\n========================================================");
    Serial.println("  [SUKSES] TRANSMITTER TERHUBUNG KE WI-FI!");
    Serial.print("  >> ALAMAT IP DASHBOARD : ");
    Serial.println(currentIpStr);
    Serial.print("  >> TARGET VOICE N8N    : ");
    Serial.println(n8nVoiceUrl);
    Serial.println("========================================================\n");
  }

  initEspNowTx();

  server.on("/command", HTTP_ANY, handleCommand);
  server.on("/servo", HTTP_ANY, handleServoCalibration);
  server.on("/face", HTTP_ANY, handleFace);
  server.on("/config", HTTP_ANY, handleConfig);
  server.on("/telemetry", HTTP_ANY, handleTelemetry);
  server.on("/", HTTP_ANY, []() {
    handleCORS();
    if (server.method() == HTTP_OPTIONS) { server.send(204); return; }
    server.send(200, "application/json", "{\"device\":\"VORTEX-X-TX\",\"status\":\"READY\"}");
  });
  server.onNotFound([]() {
    handleCORS();
    if (server.method() == HTTP_OPTIONS) server.send(204);
    else server.send(404, "application/json", "{\"error\":\"Endpoint tidak ditemukan\"}");
  });

  server.begin();

  // Bunyi nada sambutan di Speaker MAX98357A lalu nyalakan standby Mic INMP441
  playSpeakerToneI2S(1046, 80);
  playSpeakerToneI2S(1568, 100);
  startMicrophone();

  if (lcdReady) updateTxLcd("Waiting Motion", "System Ready    ");
  renderOledAnimationFrame();
  renderTxLcdOnly();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  if (now - lastSensorReadMs >= 160) {
    lastSensorReadMs = now;
    int prevPir = pirState;
    pirState = digitalRead(PIR_PIN);

    sampleMicLevelQuick();

    if (now - lastDhtReadMs >= 2000) {
      lastDhtReadMs = now;
      float t = dht.readTemperature();
      float h = dht.readHumidity();
      if (!isnan(t) && !isnan(h)) {
        temperatureC = t;
        humidityPct = h;
      }
    }

    // Jika PIR mendeteksi gerakan baru (dengan cooldown 12 detik agar tidak ganggu kontrol Web!)
    if (autoRecordOnPir && pirState == HIGH && prevPir == LOW && !recording &&
        (now - lastAutoRecordMs > PIR_RECORD_COOLDOWN_MS)) {
      Serial.println("[PIR] Gerakan terdeteksi -> Menjalankan rekam & putar suara!");
      delay(300);
      recordAndPlayAudio();
    }
  }

  // Kirim paket data & state ke Receiver secara rutin tiap 350 ms
  if (now - lastTxSendMs >= 350) {
    lastTxSendMs = now;
    sendPacketToReceiver();
  }

  // Animasi mata OLED super mulus (~18 FPS jika hanya OLED, ~10 FPS jika LCD ikut di I2C yang sama)
  unsigned long oledInterval = lcdReady ? 95 : 55;
  if (now - lastOledAnimMs >= oledInterval) {
    lastOledAnimMs = now;
    renderOledAnimationFrame();
  }

  // Update panel LCD TX tiap 500 ms tanpa mengganggu kecepatan animasi OLED
  if (lcdReady && (now - lastLcdUpdateMs >= 500)) {
    lastLcdUpdateMs = now;
    renderTxLcdOnly();
  }
}
