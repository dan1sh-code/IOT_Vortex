# Panduan Perakitan Mekanik Spider Quadruped // VORTEX-X

Panduan ini menjelaskan langkah demi langkah merakit komponen kelistrikan dan suku cadang mekanik menjadi bentuk fisik utuh robot berkaki empat (*spider quadruped*) **VORTEX-X**, dilengkapi gambar wujud jadi dan diagram ledak perakitan (*exploded assembly diagram*).

---

## 1. Bentuk Jadi Robot VORTEX-X (Fully Assembled)

Berikut adalah wujud fisik dasar robot berkaki empat **VORTEX-X** setelah seluruh rangka, 8 servo SG90, layar OLED 0.96" (mata digital), panel LCD 16×2 (panel badan), sensor PIR (hidung), dan modul ESP32 dirakit secara presisi:

![Bentuk Jadi Robot VORTEX-X](file:///d:/IOT_Vortex/docs/vortex_spider_assembled.jpg)

> 💡 **File Gambar Bentuk Jadi:** [docs/vortex_spider_assembled.jpg](file:///d:/IOT_Vortex/docs/vortex_spider_assembled.jpg)

### Anatomi Fisik Robot Jadi:
1. **Kepala Sensor & Wajah Depan:**
   - Bagian atas: Layar **OLED 0.96" SSD1306** yang menampilkan animasi dua mata digital ekspresif robot (pada unit Transmitter).
   - Bagian bawah: Kubah putih **Sensor PIR (HC-SR501/AM312)** menghadap lurus ke depan sebagai "radar" pendeteksi radiasi termal tubuh manusia/makhluk hidup.
2. **Dek Sasis Atas (Upper Deck):**
   - Menampung mikrokontroler **ESP32 DevKit V1**, layar **LCD 16×2 I2C** penampil data suhu/kelembapan/status, modul step-down buck converter 5V, dan paket baterai **Li-Po 2S 7.4V**.
3. **4 Kaki Artikulasi Laba-laba (Spider Legs):**
   - Terdiri dari 4 sudut simetris: **Depan Kiri (FL)**, **Depan Kanan (FR)**, **Belakang Kiri (RL)**, dan **Belakang Kanan (RR)**.
   - Pada konfigurasi 4-servo bawah (Receiver), tiap kaki menggunakan satu servo pada engsel bawah untuk gerakan melambai.
4. **Telapak Kaki (Foot Pads):**
   - Ujung kaki dilengkapi bantalan karet bulat anti-slip.

---

## 2. Diagram Panduan Perakitan Langkah demi Langkah

Gambar diagram teknik berikut menunjukkan urutan perakitan sendi, dudukan servo, baut M2, dan pemasangan sasis utama:

![Diagram Perakitan Langkah demi Langkah VORTEX-X](file:///d:/IOT_Vortex/docs/vortex_assembly_steps.jpg)

> 💡 **File Diagram Teknik Perakitan:** [docs/vortex_assembly_steps.jpg](file:///d:/IOT_Vortex/docs/vortex_assembly_steps.jpg)

---

## 3. Langkah-Langkah Perakitan dari Nol Menjadi Spider

### TAHAP 0: Kalibrasi Netral Servo
> [!IMPORTANT]
> Saat pertama kali dinyalakan, firmware **VORTEX-X Receiver** langsung berada pada mode **RELAX (diam bebas getar)**. Anda dapat mengatur sudut awal masing-masing servo secara halus menggunakan slider di Dashboard Web sebelum menekan **`💾 SIMPAN TITIK AWAL`**.

#### Cara Kalibrasi Cepat:
1. Upload program firmware [vortex_x_receiver.ino](file:///d:/IOT_Vortex/firmware/vortex_x_receiver/vortex_x_receiver.ino) ke ESP32 Receiver.
2. Hubungkan pin sinyal ke-4 servo kaki bawah ke GPIO yang sesuai (`13 = Belakang Kiri`, `14 = Depan Kiri`, `18 = Belakang Kanan`, `21 = Depan Kanan`) dan beri daya 5V.
3. Hubungkan Dashboard ke Transmitter, lalu geser slider setiap kaki sampai pas dan klik **`💾 SIMPAN TITIK AWAL`**.

---

### LANGKAH 1: Pemasangan Dudukan Servo Paha ke Sasis Bawah (Step 1)
*(Lihat Kotak 1 pada Gambar Diagram Perakitan)*
1. Ambil pelat sasis bawah (*bottom acrylic chassis*).
2. Pasang 4 buah braket servo paha (*3D printed / acrylic mounting bracket*) pada keempat sudut sasis:
   - Sudut Depan Kiri (FL)
   - Sudut Depan Kanan (FR)
   - Sudut Belakang Kiri (RL)
   - Sudut Belakang Kanan (RR)
3. Masukkan 4 buah servo SG90 untuk sendi **Hip (Ayun)** ke dalam masing-masing lubang braket.
4. Kunci bodi servo ke braket menggunakan sekrup kecil bawaan servo (M2 × 8 mm) dan mur pengunci.
5. Pastikan as/poros putar servo menghadap ke arah luar bodi robot.

---

### LANGKAH 2: Perakitan Ruas Paha (Femur) ke Servo Angkat (Step 2)
*(Lihat Kotak 2 pada Gambar Diagram Perakitan)*
1. Ambil lengan paha atas (*upper leg link / femur*).
2. Pasang tanduk servo (*servo horn single-arm*) ke dudukan paha menggunakan dua sekrup mikro M2.
3. Pasangkan servo kedua pada setiap kaki, yaitu servo **Lift (Angkat/Lutut)** ke dalam kompartemen paha.
4. Tancapkan tanduk paha ke as gerigi servo Hip yang sudah terkalibrasi 90° pada Langkah 1.
5. Posisikan lengan paha membentuk sudut **45° mengarah ke luar bodi**.
6. Kencangkan sekrup poros tengah horn ke as servo.

---

### LANGKAH 3: Perakitan Ruas Betis / Kaki Bawah (Tibia Foot Leg) (Step 3)
*(Lihat Kotak 3 pada Gambar Diagram Perakitan)*
1. Ambil ruas betis (*tibia foot leg*).
2. Pasangkan horn servo angkat ke bagian pangkal ruas betis.
3. Hubungkan ruas betis ke as gerigi servo Lift (Lutut) yang berada di paha.
4. Pada sisi engsel sebaliknya, pasang baut M2 yang dilengkapi **bushing / ring plastik (spacer)** sebelum dikencangkan dengan mur nyloc/mur pengunci.
   > 💡 **Tips:** Bushing plastik memastikan sendi kaki dapat berputar bebas dan kokoh tanpa macet (*free articulation*) dan tidak goyang (*wobbly*).
5. Pada ujung telapak kaki, pasang bantalan karet bundar (*rubber foot pad*) menggunakan satu sekrup pengunci di bagian bawah.

---

### LANGKAH 4: Perakitan Dek Sasis Atas & Kompartemen Elektronik (Step 4)
*(Lihat Kotak 4 pada Gambar Diagram Perakitan)*
1. Pasang 4 atau 6 tiang spacer kuningan/nilon (standoff M3 × 25 mm) di atas sasis bawah.
2. Tempatkan pelat sasis atas (*upper deck chassis*) dan kunci dengan baut M3.
3. Pasang dudukan mikrokontroler **ESP32 DevKit V1** di bagian tengah atas dek menggunakan selotip bolak-balik busa 3M atau sekrup M2.5.
4. Letakkan modul step-down buck converter 5V di dekat jalur kabel servo.
5. Pasang baterai Li-Po 2S (7.4V) di bagian belakang dek atas dan kencangkan menggunakan sabuk velcro agar mudah dilepas saat di-cas.

---

### LANGKAH 5: Pemasangan Layar Wajah OLED, LCD 16x2 & Sensor Depan
1. Pasang layar **OLED 0.96" I2C** di bagian kepala depan (*front faceplate*) agar berfungsi sebagai wajah/mata robot.
2. Pasang layar **LCD 16×2 I2C** pada bagian atas badan/sasis agar mudah membaca suhu, kelembapan, dan status IP secara langsung.
3. Pasang kubah putih **Sensor PIR (HC-SR501/AM312)** di lubang bagian bawah kepala, menghadap lurus ke depan.
4. Pasang modul **DHT11** di sisi samping bodi yang bebas dari hembusan panas regulator/baterai.
5. Tempelkan modul **Buzzer** di sasis bagian dalam.

---

### LANGKAH 6: Manajemen Pengkabelan (*Cable Management*)
Kerapian kabel sangat krusial pada robot berkaki bergerak agar tidak tersangkut gear mekanik:
1. Kelompokkan kabel ketiga pin dari setiap servo (Signal, VCC, GND).
2. Berikan sedikit kelonggaran (*slack loop* ~3 cm) pada setiap sendi lutut dan paha agar saat kaki melangkah maju maksimal, kabel tidak tertarik tegang.
3. Satukan kabel-kabel servo di sepanjang rangka bodi menggunakan selongsong spiral (*spiral wire wrap*) atau zip-ties mini.
4. Sambungkan seluruh kabel VCC dan GND servo ke bus daya 5V pada breadboard/PCB distribusi sesuai panduan di [WIRING_LENGKAP_VORTEX_X.md](file:///d:/IOT_Vortex/WIRING_LENGKAP_VORTEX_X.md).
5. Pastikan kabel **Common Ground** dari pin GND ESP32 telah tersambung kuat ke GND catu daya servo.

---

### LANGKAH 7: Uji Coba Berdiri (*Zero-Load Stance Test*)
1. Tempatkan robot di atas balok penopang kecil (misal kotak kardus atau buku) sehingga keempat kaki menggantung bebas di udara.
2. Nyalakan sakelar daya.
3. Buka dashboard di browser [dashboard/index.html](file:///d:/IOT_Vortex/dashboard/index.html) atau jalankan program python [gamepad_bridge.py](file:///d:/IOT_Vortex/python_gamepad/gamepad_bridge.py).
4. Tekan tombol **A / STAND** (Berdiri).
5. Amati posisi keempat kaki:
   - Keempat paha harus mengarah simetris 45° ke luar.
   - Keempat betis harus tegak lurus membentuk postur berdiri yang kokoh.
6. Jika ada kaki yang sedikit miring, lepaskan sekrup poros horn dan putar 1 gerigi hingga simetris, lalu kencangkan kembali sekrupnya.

---

### LANGKAH 8: Uji Coba Melangkah Pertama Kali di Lantai Datar
1. Turunkan robot ke lantai yang datar dan bersih (hindari karpet tebal berbulu tinggi pada uji coba pertama).
2. Tekan tombol **D-Pad Atas** atau dorong **Stik Kiri ke Depan** pada gamepad untuk perintah `FORWARD`.
3. Robot akan mengeksekusi pola langkah **Trot Gait** (pasangan kaki FL+RR melangkah maju bersamaan, disusul pasangan FR+RL).
4. Uji fitur keselamatan ultrasonik: Letakkan tangan Anda berjarak 8 cm di depan robot saat melangkah maju. Robot akan **secara otomatis berhenti (STOP)** dan membunyikan alarm karena rintangan berada di bawah batas ambang 10 cm!
5. Lambaikan tangan di depan sensor PIR untuk memastikan status deteksi manusia berubah menjadi `1 (GERAKAN TERDETEKSI)` pada dashboard misi Anda.

Selamat merakit! Bentuk jadi robot laba-laba VORTEX-X Anda kini siap berpatroli dan dikendalikan dengan gamepad interaktif.
