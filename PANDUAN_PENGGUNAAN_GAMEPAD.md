# Panduan Lengkap Pengendalian Gamepad Interaktif & Integrasi Sensor PIR // VORTEX-X

Dokumen ini menjelaskan solusi teknis, arsitektur, dan langkah pemakaian dari sistem pengendalian **Gamepad Interaktif** serta pergantian sensor **MQ-2 ke Sensor PIR** pada robot berkaki empat **VORTEX-X**.

---

## 1. Perubahan Sensor: Mengapa PIR Lebih Unggul Dibanding MQ-2?

Pada rancangan sebelumnya, proyek menggunakan sensor gas MQ-2. Dalam pembaruan ini, MQ-2 telah resmi digantikan oleh **Sensor PIR (Passive Infrared)** seperti **HC-SR501** atau **Mini PIR AM312**.

### Keunggulan Teknis Sensor PIR untuk Robot Berkaki Empat:
| Faktor | Sensor MQ-2 (Lama) | Sensor PIR (Baru) | Keuntungan pada VORTEX-X |
|---|---|---|---|
| **Konsumsi Arus** | Sangat boros (150–200 mA) karena elemen pemanas (*heater filament*) | Sangat hemat (< 0,05 mA pada AM312 / < 50 µA) | Baterai robot bertahan jauh lebih lama |
| **Suhu Operasi** | Menghasilkan panas tinggi pada bodi robot | Dingin (tidak menghasilkan panas) | Struktur rangka plastik/akrilik aman dari panas |
| **Karakter Sinyal** | Analog ADC (butuh kalibrasi kurva gas yang rumit) | Digital murni (HIGH = ada gerakan, LOW = diam) | Pembacaan instan tanpa beban komputasi ADC |
| **Logika Tegangan** | Sering mengeluarkan output 5 V yang berisiko bagi ESP32 | Sinyal OUT bertingkat logika **3,3 V** | Aman dipasang langsung ke GPIO ESP32 tanpa *voltage divider* |
| **Fungsi Misi** | Deteksi gas/asap umum | **Deteksi keberadaan manusia / makhluk hidup** | Mengubah VORTEX-X menjadi robot patroli taktis, pengintai intrusi, dan operasi pencarian korban (SAR) |

### Skema Pemasangan Pin Sensor PIR:
- **VCC:** Hubungkan ke pin **5 V** (untuk HC-SR501) atau **3,3 V / 5 V** (untuk modul mini AM312).
- **GND:** Hubungkan ke **GND** ESP32 (ground bersama).
- **OUT:** Hubungkan langsung ke **GPIO27** ESP32.
- *Catatan HC-SR501:* Putar trimpot waktu tunda (*time delay*) ke posisi berlawanan jarum jam (minimum ~2 detik) agar respons deteksi cepat kembali ke kondisi normal saat robot bergerak.

---

## 2. Pilihan Pengendali Gamepad yang Disediakan

Kami telah membuatkan **2 solusi pengendalian gamepad interaktif** yang menarik, responsif, dan fleksibel:

```text
d:\IOT_Vortex\
├── dashboard/                     --> SOLUSI 1: Web Mission Control (Gamepad Browser)
│   ├── index.html                 --> Tampilan HUD Cyberpunk Futuristik
│   ├── style.css                  --> Animasi Radar, Glassmorphism & Visual Stik
│   └── app.js                     --> W3C Gamepad API, Synthesizer Audio & Kinematik
├── python_gamepad/                --> SOLUSI 2: Python Gamepad Controller Bridge
│   ├── gamepad_bridge.py          --> Program Python Pygame dengan Terminal HUD
│   └── requirements.txt           --> Dependensi (pygame, requests, colorama)
└── firmware/
    ├── vortex_x_transmitter/      --> Firmware ESP32 #1 (Web Gateway + Audio I2S + Sensor + OLED)
    └── vortex_x_receiver/         --> Firmware ESP32 #2 (4 Servo Kaki Bawah + LCD Data)
```

---

## 3. SOLUSI 1: Dashboard Web Gamepad Mission Control (Sangat Direkomendasikan!)

Solusi ini adalah aplikasi berbasis web dengan desain **Cyberpunk / Sci-Fi Tactical HUD** yang dapat dibuka di browser apa pun (Google Chrome, Microsoft Edge, Opera, Safari) di laptop, tablet, atau smartphone.

### Fitur-Fitur Menarik:
1. **Dukungan Penuh W3C Gamepad API:**
   - Cukup colokkan gamepad USB atau hubungkan gamepad Bluetooth (Xbox One/Series, PlayStation DualShock 4/DualSense, Logitech F310/F710, Ipega, atau gamepad USB generik).
   - Dashboard secara otomatis mendeteksi gamepad, menampilkan namanya, dan memetakan tombolnya secara real-time.
2. **Visual Gamepad HUD Real-Time:**
   - Terdapat diagram visual gamepad interaktif di mana setiap tombol yang ditekan (A, B, X, Y, D-Pad, Bumper, Trigger) dan gerakan stik analog akan menyala dan bergerak di layar!
3. **Dual Virtual Analog Joysticks (Touch / Mouse):**
   - Jika Anda tidak sedang membawa gamepad fisik, Anda bisa menggunakan stik analog virtual di layar dengan mouse atau jari (layar sentuh).
4. **Visualisasi Postur Robot 8-Servo & Animasi Gait:**
   - Diagram top-down robot VORTEX-X dengan kaki bersendi yang bergerak secara realistis (*trot gait kinematics*) sesuai perintah maju, mundur, atau belok, lengkap dengan derajat sudut servo (FL, FR, RL, RR).
5. **Dual Display Mirror (OLED + LCD) & Deteksi Intrusi PIR:**
   - Cermin virtual real-time untuk animasi ekspresi mata OLED 0.96" dan teks 2 baris LCD 16×2 robot.
   - Indikator peringatan sensor PIR yang akan berkedip merah dan memicu mata waspada (`ALERT`) saat mendeteksi manusia.
6. **Efek Suara Sintetis (Web Audio API):**
   - Suara klik antarmuka futuristik, dengung servo, dan sirine bahaya saat ada manusia terdeteksi oleh sensor PIR.
7. **Getaran Haptic (Rumble Feedback):**
   - Gamepad fisik Anda akan bergetar saat Anda menekan tombol perintah atau saat sensor PIR memicu peringatan!
8. **Export Log CSV:**
   - Tombol satu klik untuk mengunduh log telemetri dan anomali ke format CSV untuk analisis model machine learning AI.

### Cara Pemakaian Solusi 1:
1. Buka file `dashboard/index.html` langsung di browser Anda (cukup klik ganda atau drag file ke browser Chrome/Edge).
2. **Uji Coba Langsung (Mode Simulasi):**
   - Secara default, **Mode Simulasi Aktif**. Anda dapat langsung menggerakkan joystick, menekan tombol keyboard (WASD / Panah / Spasi), atau menghubungkan gamepad fisik Anda.
3. **Hubungkan ke Robot ESP32 Nyata:**
   - Hubungkan Wi-Fi laptop/HP Anda ke Wi-Fi ESP32:
     - **SSID:** `VORTEX-X_ROBOT`
     - **Password:** `vortex12345`
   - Masukkan IP ESP32 pada kolom host: `192.168.4.1` (atau IP lokal jika menggunakan router).
   - Klik tombol **HUBUNGKAN**.
   - Matikan "Mode Simulasi". Dashboard kini langsung mengontrol robot fisik Anda dengan latensi sangat rendah (< 10 ms)!

---

## 4. SOLUSI 2: Program Python Gamepad Bridge (`gamepad_bridge.py`)

Bagi Anda yang lebih menyukai kendali langsung melalui skrip Python pada terminal atau laptop, kami telah membuatkan program `python_gamepad/gamepad_bridge.py`.

### Cara Menjalankan:
1. Buka terminal PowerShell atau Command Prompt di folder proyek:
   ```powershell
   cd d:\IOT_Vortex\python_gamepad
   ```
2. Pasang pustaka yang diperlukan:
   ```bash
   pip install -r requirements.txt
   ```
3. Hubungkan gamepad USB/Bluetooth ke laptop Anda.
4. **Uji dalam Mode Simulasi (tanpa robot):**
   ```bash
   python gamepad_bridge.py --sim
   ```
   Gerakkan stik analog atau tekan tombol; terminal akan menampilkan status arah gerak, kecepatan, dan waktu secara real-time dengan warna ANSI interaktif!
5. **Jalankan dengan Robot ESP32 Nyata:**
   ```bash
   python gamepad_bridge.py --ip 192.168.4.1
   ```

---

## 5. Pemetaan Tombol Gamepad (Standard Mapping)

| Kontrol Gamepad | Aksi pada Robot VORTEX-X | Perilaku Kinematik |
|---|---|---|
| **Stik Kiri (Ke Depan / Atas)** / **D-Pad Atas** | **FORWARD (Maju)** | Trot Gait maju dinamis (FL+RR bergantian dengan FR+RL) |
| **Stik Kiri (Ke Belakang / Bawah)** / **D-Pad Bawah** | **BACKWARD (Mundur)** | Trot Gait mundur |
| **Stik Kanan (Kiri)** / **D-Pad Kiri** | **LEFT (Belok Kiri)** | Sudut yaw servo hip membelokkan bodi ke kiri |
| **Stik Kanan (Kanan)** / **D-Pad Kanan** | **RIGHT (Belok Kanan)** | Sudut yaw servo hip membelokkan bodi ke kanan |
| **Tombol A** (Silang pada PlayStation) | **STAND (Berdiri)** | Seluruh 8 servo kembali ke posisi netral 90° |
| **Tombol B** (Lingkaran pada PlayStation) | **STOP (Berhenti)** | Mengunci gerakan seketika |
| **Tombol X** (Kotak pada PlayStation) | **CROUCH (Merunduk)** | Servo angkat ditekuk ke 130° untuk merendahkan pusat massa |
| **Tombol Y** (Segitiga pada PlayStation) | **WAVE (Salam Demo)** | Robot mengangkat kaki depan kanan dan melambai |
| **Bumper Kiri (LB)** | **Turunkan Kecepatan** | Beralih ke mode ECO (1x) |
| **Bumper Kanan (RB)** | **Naikkan Kecepatan** | Beralih ke mode SPORT (3x) |
| **Pelepasan Stik Analog** | **STOP Otomatis** | Begitu stik dilepas ke posisi tengah, robot otomatis berhenti |

---

## 6. Fitur Keselamatan Otonom (*Safety Interlock*)

1. **Zero-Snap Startup & Mode RELAX (Anti-Getar):**
   - Saat ESP32 Receiver baru menyala, seluruh 4 servo langsung dalam kondisi `RELAX` (PWM dilepas) sehingga tidak menyentak ke 90° dan tidak bergetar.
2. **Koneksi Timeout (Dead Man's Switch):**
   - Jika sinyal ESP-NOW dari Transmitter terputus lebih dari **5 detik** saat robot aktif, Receiver otomatis menghentikan gerak dan masuk mode `RELAX`.
3. **Peringatan Deteksi Manusia PIR & Audio I2S:**
   - Begitu sensor PIR mendeteksi gerakan, Transmitter memicu rekaman mic INMP441 $\rightarrow$ Speaker MAX98357A, mengubah ekspresi OLED ke `ALERT`, dan memancarkan status ke LCD Receiver serta Dashboard Web.

---

## 7. Langkah Flashing Firmware ESP32

1. Buka aplikasi **Arduino IDE**.
2. Pasang board `esp32` oleh Espressif Systems melalui *Boards Manager*.
3. Pasang pustaka pendukung dari *Library Manager*:
   - `ESP32Servo` oleh Kevin Harrington (untuk Receiver).
   - `DHT sensor library` + `Adafruit SSD1306` + `Adafruit GFX Library` (untuk Transmitter).
   - `LiquidCrystal I2C` oleh Frank de Brabander.
4. Upload [`vortex_x_transmitter.ino`](file:///d:/IOT_Vortex/firmware/vortex_x_transmitter/vortex_x_transmitter.ino) ke ESP32 #1 (Transmitter).
5. Upload [`vortex_x_receiver.ino`](file:///d:/IOT_Vortex/firmware/vortex_x_receiver/vortex_x_receiver.ino) ke ESP32 #2 (Receiver).
6. Buka **Serial Monitor** pada baudrate `115200` untuk melihat IP Transmitter & status Auto-Pairing ESP-NOW Receiver!
