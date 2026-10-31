/*
 ==============================================================================
  PROYEK VORTEX-X // FIRMWARE ESP32 UTAMA
  Quadruped Robot Controller dengan Integrasi:
  - 8 Servo SG90 (2 Servo Hip/Lift per Kaki)
  - Sensor Ultrasonik HC-SR04 (GPIO23 TRIG, GPIO32 ECHO)
  - Sensor Suhu & Kelembapan DHT11 (GPIO4)
  - Sensor Deteksi Gerak PIR (HC-SR501 / AM312) pada GPIO27
  - Buzzer Alarm (GPIO25)
  - Web Server REST API Lokal untuk Gamepad Dashboard & Python Bridge
 ==============================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <DHT.h>

// ============================================================================
// 1. KONFIGURASI WI-FI
// ============================================================================
// Pilih mode Access Point (ESP32 memancarkan Wi-Fi sendiri) atau Station (Koneksi ke Router)
const bool USE_ACCESS_POINT = true;

// Mode Access Point (Robot mandiri di lapangan)
const char* AP_SSID = "VORTEX-X_ROBOT";
const char* AP_PASS = "vortex12345";

// Mode Station (Jika ingin menghubungkan ke router Wi-Fi rumah/lab)
const char* STA_SSID = "NAMA_WIFI_ROUTER";
const char* STA_PASS = "PASSWORD_WIFI";

WebServer server(80);

// ============================================================================
// 2. PINOUT HARDWARE SESUAI DOKUMEN RANCANGAN LENGKAP
// ============================================================================
// Pin Servo (8 Servo SG90)
const int PIN_SERVO_FL_HIP  = 13; // Depan Kiri Ayun
const int PIN_SERVO_FL_LIFT = 14; // Depan Kiri Angkat
const int PIN_SERVO_FR_HIP  = 16; // Depan Kanan Ayun
const int PIN_SERVO_FR_LIFT = 17; // Depan Kanan Angkat
const int PIN_SERVO_RL_HIP  = 18; // Belakang Kiri Ayun
const int PIN_SERVO_RL_LIFT = 19; // Belakang Kiri Angkat
const int PIN_SERVO_RR_HIP  = 21; // Belakang Kanan Ayun
const int PIN_SERVO_RR_LIFT = 22; // Belakang Kanan Angkat

// Pin Sensor Lingkungan
const int PIN_TRIG = 23;
const int PIN_ECHO = 32; // Melalui pembagi tegangan 5V -> 3.3V
const int PIN_DHT  = 4;
const int PIN_PIR  = 27; // Sensor PIR HC-SR501 / AM312 (Digital OUT 3.3V)
const int PIN_BUZZER = 25;

// Instansiasi Servo
Servo sFL_Hip, sFL_Lift;
Servo sFR_Hip, sFR_Lift;
Servo sRL_Hip, sRL_Lift;
Servo sRR_Hip, sRR_Lift;

// Instansiasi DHT11
#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

// ============================================================================
// 3. VARIABEL STATE & TELEMETRI
// ============================================================================
String currentCommand = "STAND";
int currentSpeedLevel = 2; // 1: Lambat, 2: Normal, 3: Cepat
unsigned long lastCommandTime = 0;
const unsigned long TIMEOUT_SAFETY_MS = 2500; // Timeout henti otomatis jika hilang sinyal

// Pembacaan Sensor
float distanceCm = 50.0;
float temperatureC = 28.0;
float humidityPct = 65.0;
int pirState = 0; // 0 = Diam, 1 = Ada Gerakan Manusia

// Waktu interval pembacaan sensor
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 250; // Baca sensor tiap 250ms

// Langkah Gait
int gaitStep = 0;
unsigned long lastGaitStepTime = 0;

// ============================================================================
// 4. FUNGSI KENDALI KINEMATIK SERVO
// ============================================================================
void attachServos() {
  sFL_Hip.attach(PIN_SERVO_FL_HIP);
  sFL_Lift.attach(PIN_SERVO_FL_LIFT);
  sFR_Hip.attach(PIN_SERVO_FR_HIP);
  sFR_Lift.attach(PIN_SERVO_FR_LIFT);
  sRL_Hip.attach(PIN_SERVO_RL_HIP);
  sRL_Lift.attach(PIN_SERVO_RL_LIFT);
  sRR_Hip.attach(PIN_SERVO_RR_HIP);
  sRR_Lift.attach(PIN_SERVO_RR_LIFT);
}

void writeAllServos(int flH, int flL, int frH, int frL, int rlH, int rlL, int rrH, int rrL) {
  sFL_Hip.write(flH);
  sFL_Lift.write(flL);
  sFR_Hip.write(frH);
  sFR_Lift.write(frL);
  sRL_Hip.write(rlH);
  sRL_Lift.write(rlL);
  sRR_Hip.write(rrH);
  sRR_Lift.write(rrL);
}

// Postur Berdiri Netral
void stand() {
  writeAllServos(90, 90, 90, 90, 90, 90, 90, 90);
}

// Postur Merunduk
void crouch() {
  writeAllServos(90, 130, 90, 130, 90, 130, 90, 130);
}

// Henti Darurat
void stopRobot() {
  stand();
}

// Pola Gerak Maju (Trot Gait Dinamis)
void stepForward(int step) {
  if (step == 0) {
    // Pasangan 1 (FL + RR) Angkat & Ayun Maju
    sFL_Lift.write(70);
    sFL_Hip.write(105);
    sRR_Lift.write(70);
    sRR_Hip.write(105);
    // Pasangan 2 (FR + RL) Tumpu & Dorong Mundur
    sFR_Lift.write(90);
    sFR_Hip.write(75);
    sRL_Lift.write(90);
    sRL_Hip.write(75);
  } else if (step == 1) {
    // Pasangan 1 Menjejak tanah
    sFL_Lift.write(90);
    sRR_Lift.write(90);
  } else if (step == 2) {
    // Pasangan 2 (FR + RL) Angkat & Ayun Maju
    sFR_Lift.write(70);
    sFR_Hip.write(105);
    sRL_Lift.write(70);
    sRL_Hip.write(105);
    // Pasangan 1 (FL + RR) Tumpu & Dorong Mundur
    sFL_Lift.write(90);
    sFL_Hip.write(75);
    sRR_Lift.write(90);
    sRR_Hip.write(75);
  } else if (step == 3) {
    // Pasangan 2 Menjejak tanah
    sFR_Lift.write(90);
    sRL_Lift.write(90);
  }
}

// Pola Gerak Mundur
void stepBackward(int step) {
  if (step == 0) {
    sFL_Lift.write(70);  sFL_Hip.write(75);
    sRR_Lift.write(70);  sRR_Hip.write(75);
    sFR_Lift.write(90);  sFR_Hip.write(105);
    sRL_Lift.write(90);  sRL_Hip.write(105);
  } else if (step == 2) {
    sFR_Lift.write(70);  sFR_Hip.write(75);
    sRL_Lift.write(70);  sRL_Hip.write(75);
    sFL_Lift.write(90);  sFL_Hip.write(105);
    sRR_Lift.write(90);  sRR_Hip.write(105);
  }
}

// Pola Belok Kiri
void stepTurnLeft() {
  sFL_Hip.write(75);
  sFR_Hip.write(105);
  sRL_Hip.write(75);
  sRR_Hip.write(105);
}

// Pola Belok Kanan
void stepTurnRight() {
  sFL_Hip.write(105);
  sFR_Hip.write(75);
  sRL_Hip.write(105);
  sRR_Hip.write(75);
}

// Gerakan Melambai (Salam)
void waveAction(int step) {
  stand();
  if (step % 2 == 0) {
    sFR_Lift.write(40);
    sFR_Hip.write(120);
  } else {
    sFR_Lift.write(40);
    sFR_Hip.write(70);
  }
}

// ============================================================================
// 5. PEMBACAAN SENSOR & ATURAN KESELAMATAN LOKAL
// ============================================================================
float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 25000); // Timeout 25ms
  if (duration == 0) return 999.0; // Tidak ada halangan / timeout
  float dist = duration * 0.034 / 2.0;
  return (dist < 2.0 || dist > 400.0) ? 999.0 : dist;
}

void updateSensors() {
  // 1. Baca Jarak Ultrasonik
  distanceCm = readDistanceCm();

  // 2. Baca Sensor Gerak PIR (HC-SR501/AM312)
  pirState = digitalRead(PIN_PIR);

  // Jika PIR mendeteksi gerakan, beri bunyi beep pendek pada buzzer
  if (pirState == HIGH) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(500);
    digitalWrite(PIN_BUZZER, LOW);
  }

  // 3. Baca Suhu & Kelembapan DHT11 secara berkala
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperatureC = t;
  if (!isnan(h)) humidityPct = h;

  // 4. ATURAN KESELAMATAN LOKAL:
  // Jika robot diperintahkan MAJU tetapi jarak halangan <= 10.0 cm,
  // ESP32 secara otonom membatalkan gerakan maju dan berhenti!
  if (currentCommand == "FORWARD" && distanceCm <= 10.0) {
    currentCommand = "STOP";
    stopRobot();
    tone(PIN_BUZZER, 1000, 150); // Alarm bahaya halangan dekat
  }
}

// ============================================================================
// 6. REST API WEB SERVER
// ============================================================================
void handleCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

void handleCommand() {
  handleCORS();
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    cmd.toUpperCase();

    if (server.hasArg("speed")) {
      currentSpeedLevel = server.arg("speed").toInt();
      if (currentSpeedLevel < 1) currentSpeedLevel = 1;
      if (currentSpeedLevel > 3) currentSpeedLevel = 3;
    }

    // Interlock keselamatan jarak
    if (cmd == "FORWARD" && distanceCm <= 10.0) {
      server.send(403, "application/json", "{\"status\":\"BLOCKED\",\"reason\":\"OBSTACLE_TOO_CLOSE\"}");
      return;
    }

    currentCommand = cmd;
    lastCommandTime = millis();

    if (currentCommand == "STAND") stand();
    else if (currentCommand == "CROUCH") crouch();
    else if (currentCommand == "STOP") stopRobot();

    String jsonResponse = "{";
    jsonResponse += "\"status\":\"OK\",";
    jsonResponse += "\"command\":\"" + currentCommand + "\",";
    jsonResponse += "\"speed\":" + String(currentSpeedLevel) + ",";
    jsonResponse += "\"distance\":" + String(distanceCm, 1) + ",";
    jsonResponse += "\"pir\":" + String(pirState);
    jsonResponse += "}";

    server.send(200, "application/json", jsonResponse);
  } else {
    server.send(400, "application/json", "{\"error\":\"Parameter 'cmd' diperlukan\"}");
  }
}

void handleTelemetry() {
  handleCORS();
  String json = "{";
  json += "\"device\":\"VORTEX-X\",";
  json += "\"command\":\"" + currentCommand + "\",";
  json += "\"speed\":" + String(currentSpeedLevel) + ",";
  json += "\"distance\":" + String(distanceCm, 1) + ",";
  json += "\"temperature\":" + String(temperatureC, 1) + ",";
  json += "\"humidity\":" + String(humidityPct, 1) + ",";
  json += "\"pir\":" + String(pirState) + ",";
  json += "\"safety\":\"" + String(distanceCm <= 10.0 ? "OBSTACLE_ALERT" : "SAFE") + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ============================================================================
// 7. SETUP & MAIN LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] Menginisialisasi Robot Berkaki Empat VORTEX-X...");

  // Inisialisasi Pin Sensor
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_PIR, INPUT); // Pin Input untuk Sensor PIR HC-SR501/AM312
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Inisialisasi Sensor DHT11
  dht.begin();

  // Inisialisasi 8 Servo
  attachServos();
  stand(); // Posisi netral aman
  Serial.println("[OK] 8 Servo terpasang pada posisi STAND netral (90 deg).");

  // Inisialisasi Jaringan Wi-Fi
  if (USE_ACCESS_POINT) {
    WiFi.softAP(AP_SSID, AP_PASS);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("[WIFI] Mode Access Point Aktif. SSID: ");
    Serial.println(AP_SSID);
    Serial.print("[WIFI] IP Server ESP32: ");
    Serial.println(myIP);
  } else {
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.print("[WIFI] Menghubungkan ke Wi-Fi ");
    while (WiFi.status() != WL_CONNECTED) {
      delay(300);
      Serial.print(".");
    }
    Serial.println("\n[WIFI] Terhubung! IP ESP32: " + WiFi.localIP().toString());
  }

  // Daftarkan Endpoint Web Server REST API
  server.on("/command", HTTP_GET, handleCommand);
  server.on("/telemetry", HTTP_GET, handleTelemetry);
  server.on("/options", HTTP_OPTIONS, []() {
    handleCORS();
    server.send(204);
  });

  server.begin();
  Serial.println("[HTTP] REST API Server aktif pada port 80.");
  Serial.println("[READY] VORTEX-X Siap menerima kendali Gamepad!\n");

  // Bunyi beep selamat datang
  tone(PIN_BUZZER, 1200, 100);
  delay(120);
  tone(PIN_BUZZER, 1800, 150);
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  // 1. Eksekusi Pembacaan Sensor secara berkala
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    updateSensors();
  }

  // 2. Timeout Pengaman Koneksi (Bila perintah berhenti dikirim dari remote)
  if (currentCommand != "STOP" && currentCommand != "STAND" && currentCommand != "CROUCH") {
    if (now - lastCommandTime > TIMEOUT_SAFETY_MS) {
      Serial.println("[TIMEOUT] Hilang sinyal remote! Henti otomatis untuk keamanan.");
      currentCommand = "STOP";
      stopRobot();
    }
  }

  // 3. Mesin State Gait Robot (Non-blocking loop)
  int gaitDelay = (currentSpeedLevel == 3) ? 120 : ((currentSpeedLevel == 2) ? 180 : 250);

  if (now - lastGaitStepTime >= gaitDelay) {
    lastGaitStepTime = now;
    gaitStep = (gaitStep + 1) % 4;

    if (currentCommand == "FORWARD") {
      stepForward(gaitStep);
    } else if (currentCommand == "BACKWARD") {
      stepBackward(gaitStep);
    } else if (currentCommand == "LEFT") {
      stepTurnLeft();
    } else if (currentCommand == "RIGHT") {
      stepTurnRight();
    } else if (currentCommand == "WAVE") {
      waveAction(gaitStep);
    }
  }
}
