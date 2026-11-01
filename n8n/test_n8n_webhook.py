"""
=============================================================================
VORTEX-X // TELEGRAM BOT & N8N WEBHOOK TEST SUITE
Menguji pipeline otomatisasi Telegram Sentinel dengan Bot Token & Chat ID asli
=============================================================================
"""

import sys
import time
import json
import argparse
from datetime import datetime

try:
    import requests
except ImportError:
    print("[ERROR] Library 'requests' belum terpasang. Jalankan: pip install requests")
    sys.exit(1)

TELEGRAM_BOT_TOKEN = "8281785582:AAGWNwFMIRD6kRjw9-QrVcM8WuXmPMP6UMU"
TELEGRAM_CHAT_ID = "8707654343"

DEFAULT_WEBHOOK_URL = "http://localhost:5678/webhook/vortex-telemetry"
DEFAULT_TEST_WEBHOOK_URL = "http://localhost:5678/webhook-test/vortex-telemetry"

def send_telegram_direct(title, msg_body, keyboard=None):
    url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"
    payload = {
        "chat_id": TELEGRAM_CHAT_ID,
        "text": f"<b>{title}</b>\n━━━━━━━━━━━━━━━━━━━━━━━━━━\n{msg_body}",
        "parse_mode": "HTML"
    }
    if keyboard:
        payload["reply_markup"] = keyboard

    try:
        res = requests.post(url, json=payload, timeout=8.0)
        if res.status_code == 200:
            print(f"[TELEGRAM 200 OK] Pesan berhasil masuk ke akun Telegram Anda ({TELEGRAM_CHAT_ID})!")
        else:
            print(f"[TELEGRAM ERR {res.status_code}] {res.text}")
    except Exception as e:
        print(f"[ERROR] Gagal kirim langsung ke Telegram: {e}")

def send_webhook(url, payload, label):
    print(f"\n[N8N WEBHOOK] Mengirim {label} ke: {url}")
    start = time.time()
    try:
        res = requests.post(
            url,
            json=payload,
            headers={"Content-Type": "application/json"},
            timeout=6.0
        )
        latency = (time.time() - start) * 1000
        print(f"[STATUS] HTTP {res.status_code} ({latency:.1f}ms)")
        try:
            print("[RESPONSE]", json.dumps(res.json(), indent=2))
        except Exception:
            print("[RAW RESPONSE]", res.text)
    except requests.exceptions.ConnectionError:
        print(f"[FAILED] Tidak dapat terhubung ke n8n di {url}.")
        print("Pastikan n8n sudah aktif di localhost:5678 dan workflow dalam status ACTIVE.")
    except Exception as e:
        print(f"[ERROR] {e}")

def main():
    parser = argparse.ArgumentParser(description="VORTEX-X Telegram & n8n Test Simulator")
    parser.add_argument("--url", default=DEFAULT_WEBHOOK_URL, help="n8n webhook URL")
    parser.add_argument("--test-mode", action="store_true", help="Gunakan n8n /webhook-test/ mode")
    args = parser.parse_args()

    target_url = DEFAULT_TEST_WEBHOOK_URL if args.test_mode else args.url

    print("==============================================================")
    print("  VORTEX-X // Telegram Sentinel & n8n Pipeline Tester")
    print(f"  Bot Token : {TELEGRAM_BOT_TOKEN[:14]}... (AKTIF)")
    print(f"  Chat ID   : {TELEGRAM_CHAT_ID}")
    print(f"  Target n8n: {target_url}")
    print("==============================================================")
    print("1. Kirim Uji Langsung ke Telegram Bot (Verifikasi Bot)")
    print("2. Kirim Alarm Deteksi Gerak PIR via n8n (CRITICAL)")
    print("3. Kirim Bahaya Halangan Dekat (< 10cm) via n8n")
    print("4. Kirim Diagnosa Anomali AI (Isolation Forest) via n8n")
    print("5. Kirim Peringatan Suhu Panas ESP32 (> 35°C) via n8n")
    print("6. Kirim Telemetri Normal Rutin via n8n")
    print("7. Jalankan Semua Skenario Pengujian Berurutan")
    print("0. Keluar")

    choice = input("\nPilih menu [0-7]: ").strip()
    now = datetime.now().isoformat()

    if choice == "1":
        keyboard = {
            "inline_keyboard": [
                [
                    {"text": "🛑 E-STOP DARURAT", "callback_data": "cmd_stop"},
                    {"text": "🧎 MERUNDUK", "callback_data": "cmd_crouch"}
                ],
                [
                    {"text": "🔊 BUNYIKAN ALARM", "callback_data": "cmd_alarm"},
                    {"text": "🛡️ PATROLI", "callback_data": "cmd_patrol"}
                ]
            ]
        }
        body = (
            "🤖 Robot <b>VORTEX-X</b> telah terhubung dengan Telegram Sentinel Anda.\n\n"
            "<code>• Status : SIAGA OPTIMAL\n"
            "• Baterai: 7.4V (2S Li-ion)\n"
            "• Jarak  : 42.5 cm\n"
            "• Suhu   : 28.4 °C</code>\n\n"
            "<i>Tombol kontrol cepat di bawah ini siap digunakan.</i>"
        )
        send_telegram_direct("🚨 [UJI VERIFIKASI] TELEGRAM SENTINEL SIAGA", body, keyboard)

    elif choice == "2":
        payload = {
            "device": "VORTEX-X",
            "timestamp": now,
            "distance_cm": 35.0,
            "temperature_c": 28.8,
            "humidity_pct": 65.0,
            "pir_state": 1,
            "anomaly_score": 0.61,
            "gaitState": "PATROL",
            "message": "PIR Sensor mendeteksi gerakan manusia/objek di depan robot!"
        }
        send_webhook(target_url, payload, "Alarm Deteksi Gerak PIR")

    elif choice == "3":
        payload = {
            "device": "VORTEX-X",
            "timestamp": now,
            "distance_cm": 7.2,
            "temperature_c": 29.0,
            "humidity_pct": 64.0,
            "pir_state": 0,
            "anomaly_score": 0.88,
            "gaitState": "STOP",
            "message": "Halangan sangat dekat terdeteksi! Rintangan < 10 cm."
        }
        send_webhook(target_url, payload, "Bahaya Halangan Dekat")

    elif choice == "4":
        payload = {
            "device": "VORTEX-X",
            "timestamp": now,
            "distance_cm": 25.0,
            "temperature_c": 31.0,
            "humidity_pct": 72.0,
            "pir_state": 0,
            "anomaly_score": 0.42,
            "gaitState": "FORWARD",
            "message": "Pola servo/vibrasi abnormal terdeteksi oleh AI Isolation Forest."
        }
        send_webhook(target_url, payload, "Anomali AI Isolation Forest")

    elif choice == "5":
        payload = {
            "device": "VORTEX-X",
            "timestamp": now,
            "distance_cm": 45.0,
            "temperature_c": 37.5,
            "humidity_pct": 58.0,
            "pir_state": 0,
            "anomaly_score": 0.72,
            "gaitState": "STAND",
            "message": "Suhu mikrokontroler melebihi 35°C."
        }
        send_webhook(target_url, payload, "Peringatan Suhu Panas ESP32")

    elif choice == "6":
        payload = {
            "device": "VORTEX-X",
            "timestamp": now,
            "distance_cm": 42.5,
            "temperature_c": 28.4,
            "humidity_pct": 65.0,
            "pir_state": 0,
            "anomaly_score": 0.94,
            "gaitState": "STAND",
            "message": "Telemetri berkala normal."
        }
        send_webhook(target_url, payload, "Telemetri Rutin Normal")

    elif choice == "7":
        for option, label in [("2", "PIR"), ("3", "Obstacle"), ("4", "AI"), ("5", "Temp"), ("6", "Routine")]:
            print(f"\n--- Mengirim {label} ---")
            time.sleep(1.2)
    else:
        print("Selesai.")

if __name__ == "__main__":
    main()
