# 🧠 PANDUAN INTEGRASI N8N + GROQ AI (LLAMA-3.3) & TELEGRAM BOT // VORTEX-X

Alur kerja **n8n Workflow Automation** VORTEX-X kini telah dilengkapi dengan **Otak Taktis Kecerdasan Buatan (Groq AI Llama-3.3 70B)** yang memproses data sensor robot secara real-time dan memberikan analisis situasi serta rekomendasi taktis langsung ke Telegram Anda.

---

## 🌟 Arsitektur Pipeline n8n (14 Nodes)

```
[Webhook ESP32/Dashboard]
          │
[Sanitasi & Format Telemetri]
          │
[Anti-Flood Debounce Filter]
          │
[🧠 GROQ AI TACTICAL BRAIN (Llama-3.3 70B)]
          │
[Parse AI Tactical Insight]
          │
  ┌───────┴───────┬──────────────┬───────────────┬────────────────┐
[PIR Alarm]  [Halangan]   [Anomali AI]   [Suhu Panas]    [Status Rutin]
  └───────┬───────┴──────────────┴───────────────┴────────────────┘
          │
[Kirim Pesan Telegram Bot API (Chat ID: 8707654343)]
          │
[Audit Incident Logger]
          │
[Respond 200 OK ke Robot]
```

---

## 🔑 Cara Memasukkan API Key Groq Anda

Anda dapat memasukkan API Key Groq Anda dengan **salah satu dari 2 cara mudah** berikut:

### Opsi 1: Lewat Antarmuka n8n (Setelah Import)
1. Buka n8n di browser (`http://localhost:5678`).
2. Di kanvas alur kerja, klik ganda node **`Groq AI Tactical Brain (Llama-3.3)`**.
3. Di panel kanan, buka bagian **Headers** -> **Authorization**.
4. Ganti:
   ```
   Bearer MASUKKAN_GROQ_API_KEY_ANDA_DISINI
   ```
   menjadi:
   ```
   Bearer gsk_kunci_api_groq_anda
   ```
5. Klik **Save** (Ctrl+S) di pojok kanan atas kanvas.

### Opsi 2: Edit Langsung di File JSON Sebelum Import
Buka file [vortex_x_n8n_workflow.json](file:///d:/IOT_Vortex/n8n/vortex_x_n8n_workflow.json), cari baris `MASUKKAN_GROQ_API_KEY_ANDA_DISINI`, lalu gantikan dengan kunci Groq Anda (`gsk_...`).

> 💡 **Belum punya akun Groq?**  
> Dapatkan API key gratis dan super cepat di: **https://console.groq.com/keys** (Cukup daftar dengan Google/GitHub -> klik *Create API Key*).

---

## 🛡️ Fitur Fallback Otomatis
Node Groq AI telah dilengkapi dengan fitur **`continueOnFail: true`** dan modul fallback cerdas.  
Artinya, **jika Anda belum memasukkan API Key Groq**, alur kerja **TIDAK AKAN ERROR atau macet**; sistem akan otomatis menggunakan analisis lokal cerdas terlebih dahulu hingga Anda memasukkan API Key Groq Anda.

---

## 🤖 Kredensial Telegram yang Telah Aktif

- **Bot Token**: `8281785582:AAGWNwFMIRD6kRjw9-QrVcM8WuXmPMP6UMU`
- **Chat ID**: `8707654343`

Semua 5 node pemformat pesan dan pengirim Telegram telah terhubung secara otomatis ke akun Telegram Anda.
