# Panduan Setup n8n All-in-One: Voice AI Agent, Telemetry & Bot Telegram (VORTEX-X)

Semua alur otomasi **VORTEX-X** kini digabung menjadi **1 file workflow lengkap**:
**`d:\IOT_Vortex\n8n\vortex_x_n8n_workflow.json`** (`VORTEX-X // Master AI Agent (Voice AI + Telemetry + Telegram Bot)` — `32` node):

1. **Jalur Voice AI Agent (`POST /webhook/vortex-voice`)**:
   - **Mic INMP441 (ESP32 TX)** → `Groq Whisper STT` (`whisper-large-v3-turbo`) → `AI Agent Brain` (`qwen/qwen3.8-27b`) → `TTS WAV (Groq Orpheus)` / `Ensure Valid WAV Output` → **`Respond Audio WAV ke Speaker ESP32`** (WAV + header `X-Vortex-Cmd` & `X-Vortex-Face`) → **Switch 4 cabang** → **Bot Telegram** (`chat_id` `8707654343`) → `Audit Log`.
2. **Jalur Telemetry Sentinel (`POST /webhook/vortex-telemetry`)**:
   - **Dashboard / PIR ESP32** → `Sanitize & Classifier` → `Groq AI Tactical Brain (Qwen3.8)` → **Switch 5 cabang** (`PIR Intrusion`, `Overheat`, `AI Anomaly`, `System Event`, `Routine Digest`) → **Bot Telegram** → `Respond to Robot/Dashboard`.

---

## Status Kunci & Konfigurasi (Sudah Siap Pakai)
- **IP Laptop Target:** `10.63.8.214:5678` (sudah terpasang di firmware `vortex_x_transmitter.ino`, Dashboard HTML, dan `docker-compose.yml`).
- **Bot Telegram** (`chat_id` `8707654343`) tertanam di `vortex_x_n8n_workflow.json`. Untuk **Groq API Key**, sudah terpasang aktif di database Docker `n8n` lokal Anda *(sedangkan di file JSON Git memakai placeholder `MASUKKAN_GROQ_API_KEY_ANDA_DISINI` pada node `Prepare WAV Binary & Sensor` dan `Groq AI Tactical Brain (Qwen3.8)` agar aman dari GitHub Push Protection)*.

> **Catatan Suara Bicara Groq Orpheus (`canopylabs/orpheus-v1-english`):**
> Buka sekali saja `https://console.groq.com/playground?model=canopylabs%2Forpheus-v1-english` sambil login ke akun Groq Anda lalu klik **Accept Terms** agar TTS Orpheus membacakan kalimat penuh. Sebelum terms diklik, workflow otomatis memakai nada konfirmasi WAV `16 kHz` agar speaker & Telegram tetap berjalan lancar.
> *(Untuk TTS **Bahasa Indonesia**, Anda dapat menyambungkan node opsional `[OPSIONAL Indo] TTS VoiceRSS (id-id WAV)` dengan key gratis dari `https://www.voicerss.org/`.)*

---

## Cara Import Manual (Jika Suatu Saat Pindah Komputer)
1. Pastikan Docker n8n berjalan (`docker compose up -d` di `d:\IOT_Vortex\n8n`).
2. Di n8n (`http://localhost:5678`), import **1 file saja**: **`d:\IOT_Vortex\n8n\vortex_x_n8n_workflow.json`**.
3. Klik **Save** lalu aktifkan **Publish / Active**.
4. Pastikan port TCP `5678` diizinkan di Windows Firewall (`Vortex n8n Webhook 5678`).
5. Upload [vortex_x_transmitter.ino](file:///d:/IOT_Vortex/firmware/vortex_x_transmitter/vortex_x_transmitter.ino) lewat Arduino IDE, cek IP ESP32 TX di Serial Monitor (`115200` baud), lalu hubungkan dari Web Dashboard (`JALANKAN_DASHBOARD.bat`).
