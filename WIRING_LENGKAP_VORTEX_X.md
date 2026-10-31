# Panduan Wiring dan Skema Rangkaian Lengkap // VORTEX-X

Dokumen ini memuat panduan lengkap perkabelan (*wiring diagram*), pembagian jalur daya, rangkaian proteksi tegangan (*voltage divider*), serta diagram visual skematik untuk robot berkaki empat **VORTEX-X berbasis ESP32**.

---

## 1. Gambar Visual Rangkaian (Wiring Diagram)

Berikut adalah visualisasi pengkabelan lengkap di atas breadboard dan modul komponen:

![Diagram Pengkabelan Lengkap VORTEX-X](file:///d:/IOT_Vortex/wiring_diagram.jpg)

> 💡 **File Gambar Skematik:**
> - Format Gambar JPG Resolusi Tinggi: [wiring_diagram.jpg](file:///d:/IOT_Vortex/wiring_diagram.jpg)
> - Format Vektor Interaktif (Bisa diperbesar tanpa pecah): [docs/wiring_schematic.svg](file:///d:/IOT_Vortex/docs/wiring_schematic.svg)

---

## 2. Tabel Ringkasan Pinout ESP32 DevKit V1

| No | Modul / Komponen | Pin Komponen | Terhubung ke Pin ESP32 | Level Tegangan | Warna Kabel Standar | Catatan Khusus |
|:---:|---|---|---|:---:|:---:|---|
| **1** | **Servo FL Hip** (Depan Kiri Ayun) | Signal (Oranye/Kuning) | **GPIO13** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **2** | **Servo FL Lift** (Depan Kiri Angkat) | Signal (Oranye/Kuning) | **GPIO14** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **3** | **Servo FR Hip** (Depan Kanan Ayun) | Signal (Oranye/Kuning) | **GPIO16** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **4** | **Servo FR Lift** (Depan Kanan Angkat) | Signal (Oranye/Kuning) | **GPIO17** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **5** | **Servo RL Hip** (Belakang Kiri Ayun) | Signal (Oranye/Kuning) | **GPIO18** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **6** | **Servo RL Lift** (Belakang Kiri Angkat) | Signal (Oranye/Kuning) | **GPIO19** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **7** | **Servo RR Hip** (Belakang Kanan Ayun) | Signal (Oranye/Kuning) | **GPIO21** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **8** | **Servo RR Lift** (Belakang Kanan Angkat) | Signal (Oranye/Kuning) | **GPIO22** | 3,3 V PWM | Kuning / Oranye | VCC & GND ke jalur daya servo eksternal |
| **9** | **HC-SR04** (Ultrasonik) | **TRIG** | **GPIO23** | 3,3 V Logika | Biru | Sinyal trigger pulsa 10 µs |
| **10** | **HC-SR04** (Ultrasonik) | **ECHO** | **GPIO32** *(lewat Divider)* | **Turun ke 3,3 V** | Kuning ke Resistor | **WAJIB pakai pembagi tegangan R1=1kΩ & R2=2kΩ** |
| **11** | **Sensor DHT11** (Suhu & RH) | **DATA** | **GPIO4** | 3,3 V Digital | Biru / Hijau | Sudah ada pull-up internal modul |
| **12** | **Sensor PIR** (HC-SR501/AM312) | **OUT** | **GPIO27** | 3,3 V Digital | Ungu / Merah Muda | HIGH saat ada gerak, LOW saat diam |
| **13** | **Buzzer Alarm** | **I/O (+)** | **GPIO25** | 3,3 V Sinyal | Putih / Oranye | Alarm jarak dekat (<10 cm) & intruder |
| **14** | **Power Supply Servo** | **GND** | **GND ESP32** | 0 V Reference | **Hitam** | **WAJIB Common Ground** |

---

## 3. Arsitektur Daya & Aturan Common Ground (SANGAT PENTING!)

> [!CAUTION]
> **JANGAN PERNAH** memberi daya 8 servo SG90 langsung dari pin `3V3` atau pin `VIN/5V` ESP32 atau port USB laptop!
> Delapan servo saat bergerak serentak dapat menarik arus lonjakan (*stall current*) sebesar **3,5 A – 5 A**. Pin regulator ESP32 hanya mampu menyediakan maksimal 500 mA, yang dapat menyebabkan ESP32 me-reset (*brownout*), terbakar, atau merusak port USB laptop Anda.

```text
  +---------------------------------------------------------------+
  |                     ARSITEKTUR CATU DAYA                      |
  +---------------------------------------------------------------+

     [ BATERAI Li-Po 2S 7.4V / ADAPTOR 12V ]
                     |
                     v
   +------------------------------------+
   | Step-Down Buck Converter (5V 5A)   |
   | (Contoh: LM2596 / XL4015)          |
   +------------------------------------+
         |                        |
         | (+5V Eksternal)        | (GND Eksternal)
         v                        v
  ==================           =======================
  BUS DAYA +5V SERVO           BUS GROUND BERSAMA (GND)
  ==================           =======================
         |                                |
         +---> Seluruh VCC 8x Servo       +---> Seluruh GND 8x Servo
         +---> VCC HC-SR04 (5V)           +---> GND HC-SR04
         +---> VCC PIR HC-SR501 (5V)      +---> GND Sensor PIR
                                          +---> GND DHT11 & Buzzer
                                          |
                                          | (KABEL COMMON GROUND)
                                          v
                                   [ PIN GND ESP32 ]
                                          ^
                                          | (Daya Logika)
                                   [ KABEL USB LAPTOP / 5V LOGIKA ]
```

### Aturan Emas Kelistrikan:
1. **Common Ground (Ground Bersama):**
   Kabel **GND catu daya servo** dan kabel **GND ESP32** **HARUS disambung menjadi satu**. Jika ground tidak disatukan, sinyal PWM servo tidak memiliki titik referensi tegangan yang sama, mengakibatkan servo bergerak bergetar acak (*jitter*) dan tidak merespons perintah.
2. **Kapasitor Peredam (Opsional tapi Direkomendasikan):**
   Pasang kapasitor elektrolit **1000 µF / 16V** secara paralel pada bus 5V dan GND servo untuk meredam lonjakan arus mendadak saat robot mulai melangkah.

---

## 4. Rangkaian Proteksi Pembagi Tegangan (*Voltage Divider*) HC-SR04

Sensor HC-SR04 beroperasi pada tegangan 5 V dan pin **ECHO** mengeluarkan pulsa berlogika **5 V**. Sementara itu, pin GPIO ESP32 hanya memiliki toleransi tegangan maksimal **3,3 V**. Jika ECHO dihubungkan langsung, pin ESP32 dapat rusak secara permanen.

### Solusi Rangkaian Pembagi Tegangan:
Gunakan dua buah resistor:
- **$R_1$** = $1\text{ k}\Omega$ (atau $2\text{ k}\Omega$)
- **$R_2$** = $2\text{ k}\Omega$ (atau $3,3\text{ k}\Omega$)

### Rumus Perhitungan:
$$V_{\text{out}} = V_{\text{in}} \times \left(\frac{R_2}{R_1 + R_2}\right) = 5\text{ V} \times \left(\frac{2000}{1000 + 2000}\right) = 5\text{ V} \times 0,667 = \mathbf{3,33\text{ V}}\quad\text{(Sangat Aman)}$$

### Skema Sambungan Fisik:
```text
  [ Pin ECHO HC-SR04 (5V) ]
              |
              v
          +-------+
          | R1 1k |
          +-------+
              |
              +------------------> [ Ke Pin GPIO32 ESP32 (3.3V Aman!) ]
              |
          +-------+
          | R2 2k |
          +-------+
              |
              v
       [ GND Bersama ]
```

---

## 5. Rangkaian Sensor PIR (Deteksi Manusia / Intrusi)

Sensor PIR mendeteksi perubahan radiasi inframerah termal dari manusia atau makhluk hidup:

```text
    +----------------------------------+
    |   SENSOR PIR (HC-SR501 / AM312)  |
    +----------------------------------+
         |           |           |
        VCC         OUT         GND
       (5V)       (3.3V)        (0V)
         |           |           |
         v           v           v
    [Bus +5V]   [GPIO27 ESP32]  [Bus GND]
```

### Petunjuk Kalibrasi Trimpot HC-SR501:
Modul HC-SR501 memiliki dua potensiometer putar (trimpot):
1. **Trimpot Sensitivitas (Sensitivity):**
   - Mengatur jarak jangkauan deteksi (3 meter hingga 7 meter). Putar ke posisi tengah untuk pengujian ruangan.
2. **Trimpot Waktu Tunda (Time Delay):**
   - Mengatur durasi sinyal output tetap berada pada level HIGH setelah gerakan terdeteksi.
   - **PENTING:** Putar trimpot ini **berlawanan arah jarum jam secara penuh (minimum ~2–3 detik)**. Hal ini bertujuan agar saat orang berlalu, status robot segera kembali ke kondisi tenang (*idle*) dan tidak tertahan berlama-lama.
3. **Periode Stabilisasi (*Warm-up*):**
   - Saat robot pertama kali dinyalakan, sensor PIR memerlukan waktu stabilisasi sekitar 15–30 detik. Selama periode ini, abaikan pembacaan sensor.

---

## 6. Rangkaian Sensor Suhu & Kelembapan DHT11

Modul DHT11 3-pin (sudah dilengkapi board PCB kecil):
- **Pin VCC (+):** Hubungkan ke pin **3V3** ESP32 (atau 5V).
- **Pin DATA (Out/S):** Hubungkan ke **GPIO4** ESP32.
- **Pin GND (-):** Hubungkan ke **GND** ESP32.

---

## 7. Rangkaian Buzzer Alarm

- **Kaki Positif (+) / Sinyal:** Hubungkan ke **GPIO25** ESP32.
- **Kaki Negatif (-) / GND:** Hubungkan ke **GND** ESP32.
- *Catatan:* Buzzer akan otomatis berbunyi pendek saat PIR mendeteksi gerakan, dan berbunyi panjang saat sensor jarak mendeteksi objek $< 10\text{ cm}$.

---

## 8. Checklist Pengujian Sebelum Menyalakan Sakelar Daya

Sebelum menghubungkan baterai atau adaptor listrik, periksa kembali:
- [ ] **Polaritas Daya:** Pastikan kabel merah (+5V) dan hitam (GND) tidak terbalik di modul catu daya.
- [ ] **Common Ground:** Kabel GND ESP32 telah terhubung ke GND catu daya servo.
- [ ] **Pembagi Tegangan ECHO:** Pin ECHO HC-SR04 melewati resistor $1\text{ k}\Omega$ sebelum masuk ke GPIO32.
- [ ] **OUT PIR ke GPIO27:** Sinyal PIR tidak salah dicolok ke pin 5V.
- [ ] **Pemasangan Horn Servo:** Pastikan servo berada pada posisi 90° (program dijalankan) sebelum memasang baut kaki mekanik robot.

Dokumen ini melengkapi panduan penggunaan gamepad pada [PANDUAN_PENGGUNAAN_GAMEPAD.md](file:///d:/IOT_Vortex/PANDUAN_PENGGUNAAN_GAMEPAD.md) dan rancangan lengkap pada [VORTEX-X_Rancangan_Lengkap.md](file:///d:/IOT_Vortex/VORTEX-X_Rancangan_Lengkap.md).
