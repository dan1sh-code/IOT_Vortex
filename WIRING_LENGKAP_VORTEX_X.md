# Panduan Wiring Lengkap // VORTEX-X (Arsitektur 2 ESP32: Transmitter + Receiver)

Dokumen ini memuat panduan pengkabelan lengkap untuk arsitektur dua ESP32:
`[Web Dashboard]` $\longleftrightarrow$ **`ESP32 #1 TRANSMITTER`** $\xrightarrow{\text{ESP-NOW Wireless}}$ **`ESP32 #2 RECEIVER`**

---

## 1. Pinout ESP32 #1 — TRANSMITTER (I2S Voice Audio, Web Gateway & Sensor)

Transmitter bertugas menerima perintah dari Web Dashboard lewat Wi-Fi, membaca DHT11, mendeteksi PIR, merekam suara dari Mic **INMP441** dan memutarnya lewat Amplifier Speaker **MAX98357A**, menampilkan status di OLED/LCD I2C, lalu mengirim perintah serta data sensor ke Receiver lewat **ESP-NOW**.

| Modul / Komponen | Pin Modul | Pin ESP32 TRANSMITTER | Tegangan | Catatan Penting |
|---|---|---|---:|---|
| **Sensor PIR** | `OUT` | **GPIO 19 (`D19`)** | `5V / VIN` | Memicu perekaman audio & alert |
| **Mic I2S INMP441** | `SCK (BCLK)` / `WS` / `SD` | **GPIO 32** / **GPIO 33** / **GPIO 34** | `3V3` | `L/R` ke `GND` (Mono Kiri), VCC wajib **`3V3`** |
| **Speaker MAX98357A** | `BCLK` / `LRC` / `DIN` | **GPIO 26** / **GPIO 25** / **GPIO 22** | `5V / VIN` | `VCC` ke `VIN (5V)`, GND bersama |
| **OLED / LCD I2C** | `SDA` / `SCL` | **GPIO 21 (`D21`)** & **GPIO 23 (`D23`)** | `3V3` / `5V` | Sesuai `Wire.begin(21, 23)` — OLED (`0x3C`) & LCD (`0x27`) |
| **Sensor DHT11** | `DATA` / `OUT` | **GPIO 18 (`D18`)** | `3V3` | Di sebelah `D19`, aman saat upload |

---

## 2. Pinout ESP32 #2 — RECEIVER (4 Servo Kaki Bawah + Layar LCD Data)

Receiver menerima data sensor (Suhu, RH, PIR, Mic) dan perintah gerak dari Transmitter lewat ESP-NOW tanpa kabel, menggerakkan 4 servo melambai di sendi kaki bawah, dan menampilkan data tersebut di **LCD 16×2** serta **Serial Monitor**.

| Modul / Komponen | Pin Modul | Pin ESP32 RECEIVER | Tegangan | Catatan Penting |
|---|---|---|---:|---|
| **Servo Belakang Kiri (RL)** | Signal (Oranye) | **GPIO 13 (`D13`)** | PWM 50Hz | Daya VCC servo **WAJIB 5V eksternal** + Common GND |
| **Servo Depan Kiri (FL)** | Signal (Oranye) | **GPIO 14 (`D14`)** | PWM 50Hz | Posisi servo di engsel kaki bagian bawah (melambai) |
| **Servo Belakang Kanan (RR)** | Signal (Oranye) | **GPIO 18 (`D18`)** | PWM 50Hz | Daya VCC servo **WAJIB 5V eksternal** + Common GND |
| **Servo Depan Kanan (FR)** | Signal (Oranye) | **GPIO 21 (`D21`)** | PWM 50Hz | GPIO 21 dipakai servo FR sehingga pin LCD dipindah ke G19 |
| **LCD 16×2 I2C (PCF8574)** | `SDA` / `SCL` | **GPIO 19 (`D19`)** & **GPIO 22 (`D22`)** | **`5V (VIN)`** | **`SDA` wajib ke `D19`**, bukan `D21`! VCC wajib `5V` agar tulisan terang |

---

## 3. Aturan Daya & Anti-Getar (Wajib Diperhatikan!)

1. **Catu Daya 4 Servo Terpisah:**
   Sambungkan kabel merah (`+`) ke-4 servo ke output **Step-Down 5V / PSU Eksternal / Baterai**. Sambungkan kabel cokelat/hitam (`GND`) servo bersama-sama ke **pin `GND` ESP32 Receiver** (*Common Ground*). Jangan mengambil daya 4 servo langsung dari pin `3V3` atau port USB ESP32.
2. **Mode Awal RELAX (Diam Tenang):**
   Saat baru menyala, keempat servo di Receiver tidak dipaksa ke 90°, melainkan tetap diam tenang di sudut terakhirnya. Gunakan slider pada Dashboard untuk menyesuaikan masing-masing servo lalu tekan **`💾 SIMPAN TITIK AWAL`**.
3. **Auto-Pairing Tanpa Ketik MAC Address:**
   Pastikan nama Wi-Fi `STA_SSID` (`"pribadi"`) di file Transmitter sama dengan `TARGET_SSID_1` di file Receiver. Receiver akan otomatis mendeteksi channel Wi-Fi dan mengunci komunikasi ESP-NOW ke Transmitter dalam hitungan detik.
