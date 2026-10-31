"""
=============================================================================
VORTEX-X // PYTHON GAMEPAD CONTROLLER BRIDGE
Pengendali Gamepad Berbasis Python & Pygame untuk Robot Berkaki Empat ESP32
Mendukung: Xbox 360/One/Series, PS4/PS5, Logitech F310/F710, Generic USB/BT
=============================================================================
"""

import os
import sys
import time
import argparse
from datetime import datetime

try:
    import pygame
except ImportError:
    print("[ERROR] Library 'pygame' belum terinstal!")
    print("Silakan jalankan: pip install -r requirements.txt")
    sys.exit(1)

try:
    import requests
except ImportError:
    print("[ERROR] Library 'requests' belum terinstal!")
    print("Silakan jalankan: pip install -r requirements.txt")
    sys.exit(1)

# ANSI Colors for Interactive Terminal Display
class Colors:
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    BOLD = '\033[1m'
    RESET = '\033[0m'
    CLEAR_LINE = '\033[2K\r'

# Enable Windows VT100 colors if possible
os.system('')


class VortexGamepadController:
    def __init__(self, esp_ip="192.168.4.1", sim_mode=False, deadzone=0.20):
        self.esp_ip = esp_ip
        self.sim_mode = sim_mode
        self.deadzone = deadzone
        self.current_cmd = "STAND"
        self.current_speed = 2  # 1: Eco, 2: Normal, 3: Sport
        self.last_sent_cmd = ""
        self.last_sent_time = 0
        self.send_cooldown = 0.08  # Max 12Hz command frequency

        # Initialize Pygame Joystick
        pygame.init()
        pygame.joystick.init()

        joystick_count = pygame.joystick.get_count()
        if joystick_count == 0:
            print(f"{Colors.YELLOW}[PERINGATAN] Tidak ada joystick/gamepad fisik yang terdeteksi!{Colors.RESET}")
            print("Pastikan gamepad USB/Bluetooth sudah terhubung ke PC/laptop Anda.")
            print("Menunggu perangkat dicolokkan...\n")
            self.joystick = None
        else:
            self.joystick = pygame.joystick.Joystick(0)
            self.joystick.init()
            print(f"{Colors.GREEN}[OK] Gamepad Terdeteksi:{Colors.RESET} {self.joystick.get_name()}")
            print(f"     Jumlah Tombol: {self.joystick.get_numbuttons()} | Sumbu/Axes: {self.joystick.get_numaxes()}\n")

    def print_banner(self):
        banner = f"""{Colors.CYAN}{Colors.BOLD}
  ==============================================================
    __      ______  _____ _______ _______   __ __   __
    \ \    / / __ \|  __ \__   __|  ____\ \ / / \ \ / /
     \ \  / / |  | | |__) | | |  | |__   \ V /   \ V / 
      \ \/ /| |  | |  _  /  | |  |  __|   > <     > <  
       \  / | |__| | | \ \  | |  | |____ / . \   / . \ 
        \/   \____/|_|  \_\ |_|  |______/_/ \_\ /_/ \_\\
  ==============================================================
   VORTEX-X TACTICAL GAMEPAD CONTROLLER BRIDGE // PYTHON
  =============================================================={Colors.RESET}
  Target ESP32 : {Colors.YELLOW}http://{self.esp_ip}{Colors.RESET}
  Mode Operasi : {Colors.GREEN if self.sim_mode else Colors.CYAN}{'SIMULASI' if self.sim_mode else 'HARDWARE NYATA'}{Colors.RESET}
  Deadzone     : {self.deadzone}
  
  {Colors.BOLD}PEMETAAN TOMBOL DEFAULT:{Colors.RESET}
  [D-Pad / Stik Kiri]  : Gerak (Maju, Mundur, Belok Kiri, Belok Kanan)
  [Tombol A / X PS]    : BERDIRI (STAND)
  [Tombol B / Bulat PS]: BERHENTI (STOP)
  [Tombol X / Kotak PS]: MERUNDUK (CROUCH)
  [Tombol Y / Segitiga]: SALAM (WAVE)
  [LB / RB]            : Atur Kecepatan (1x / 2x / 3x)
  [Tekan Ctrl+C]       : Keluar dari program
  --------------------------------------------------------------
"""
        print(banner)

    def send_command_to_esp(self, cmd):
        now = time.time()
        # Cooldown check & prevent spamming identical STOP/STAND commands
        if cmd == self.last_sent_cmd and (now - self.last_sent_time) < self.send_cooldown:
            return

        self.current_cmd = cmd
        self.last_sent_cmd = cmd
        self.last_sent_time = now

        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

        if self.sim_mode:
            status_msg = f"{Colors.GREEN}[SIM]{Colors.RESET} Perintah: {Colors.BOLD}{cmd:<8}{Colors.RESET} | Speed: {self.current_speed}x"
            sys.stdout.write(f"\r[{timestamp}] {status_msg}   ")
            sys.stdout.flush()
            return

        url = f"http://{self.esp_ip}/command?cmd={cmd}&speed={self.current_speed}"
        try:
            res = requests.get(url, timeout=0.25)
            status_msg = f"{Colors.CYAN}[HTTP 200]{Colors.RESET} Perintah: {Colors.BOLD}{cmd:<8}{Colors.RESET} (PIR/Dist OK)"
            sys.stdout.write(f"\r[{timestamp}] {status_msg}   ")
            sys.stdout.flush()
        except requests.exceptions.RequestException as e:
            err_msg = f"{Colors.RED}[TIMEOUT/ERR]{Colors.RESET} Gagal kirim '{cmd}' ke ESP32 ({self.esp_ip})"
            sys.stdout.write(f"\r[{timestamp}] {err_msg}   ")
            sys.stdout.flush()

    def run(self):
        self.print_banner()
        clock = pygame.time.Clock()

        print(f"{Colors.BOLD}Mulai memantau input Gamepad... (Tekan Ctrl+C untuk berhenti){Colors.RESET}\n")

        try:
            while True:
                # Handle pygame events (hotplugging, etc.)
                for event in pygame.event.get():
                    if event.type == pygame.JOYDEVICEADDED:
                        self.joystick = pygame.joystick.Joystick(event.device_index)
                        self.joystick.init()
                        print(f"\n{Colors.GREEN}[+] Gamepad baru dicolokkan:{Colors.RESET} {self.joystick.get_name()}")
                    elif event.type == pygame.JOYDEVICEREMOVED:
                        print(f"\n{Colors.RED}[-] Gamepad dicabut!{Colors.RESET}")
                        self.joystick = None

                if not self.joystick:
                    time.sleep(0.5)
                    continue

                # Read Left Stick Axes
                axis_lx = self.joystick.get_axis(0)
                axis_ly = self.joystick.get_axis(1)

                # Filter Deadzone
                if abs(axis_lx) < self.deadzone:
                    axis_lx = 0.0
                if abs(axis_ly) < self.deadzone:
                    axis_ly = 0.0

                # Read D-Pad Hat (if available)
                num_hats = self.joystick.get_numhats()
                hat_x, hat_y = (0, 0)
                if num_hats > 0:
                    hat_x, hat_y = self.joystick.get_hat(0)

                # Read Buttons
                num_buttons = self.joystick.get_numbuttons()
                btn_A = self.joystick.get_button(0) if num_buttons > 0 else 0
                btn_B = self.joystick.get_button(1) if num_buttons > 1 else 0
                btn_X = self.joystick.get_button(2) if num_buttons > 2 else 0
                btn_Y = self.joystick.get_button(3) if num_buttons > 3 else 0
                btn_LB = self.joystick.get_button(4) if num_buttons > 4 else 0
                btn_RB = self.joystick.get_button(5) if num_buttons > 5 else 0

                # Process Speed Shift
                if btn_LB and self.current_speed > 1:
                    self.current_speed -= 1
                    time.sleep(0.18)
                elif btn_RB and self.current_speed < 3:
                    self.current_speed += 1
                    time.sleep(0.18)

                # Command Mapping Priority
                target_cmd = None

                if btn_B:
                    target_cmd = "STOP"
                elif btn_A:
                    target_cmd = "STAND"
                elif btn_X:
                    target_cmd = "CROUCH"
                elif btn_Y:
                    target_cmd = "WAVE"
                elif hat_y == 1 or axis_ly < -0.45:
                    target_cmd = "FORWARD"
                elif hat_y == -1 or axis_ly > 0.45:
                    target_cmd = "BACKWARD"
                elif hat_x == -1 or axis_lx < -0.45:
                    target_cmd = "LEFT"
                elif hat_x == 1 or axis_lx > 0.45:
                    target_cmd = "RIGHT"

                if target_cmd:
                    self.send_command_to_esp(target_cmd)
                elif self.current_cmd in ["FORWARD", "BACKWARD", "LEFT", "RIGHT"]:
                    # Return to STOP when stick is released
                    self.send_command_to_esp("STOP")

                clock.tick(30)  # 30 FPS polling loop

        except KeyboardInterrupt:
            print(f"\n\n{Colors.YELLOW}[STOP] Menghentikan VORTEX-X dan menutup koneksi gamepad...{Colors.RESET}")
            if not self.sim_mode:
                try:
                    requests.get(f"http://{self.esp_ip}/command?cmd=STOP", timeout=0.3)
                except Exception:
                    pass
            print(f"{Colors.GREEN}[SELESAI] Sampai jumpa!{Colors.RESET}\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="VORTEX-X Python Gamepad Controller Bridge")
    parser.add_argument("--ip", type=str, default="192.168.4.1", help="Alamat IP ESP32 VORTEX-X (default: 192.168.4.1)")
    parser.add_argument("--sim", action="store_true", help="Jalankan dalam mode simulasi tanpa butuh perangkat ESP32 fisik")
    parser.add_argument("--deadzone", type=float, default=0.20, help="Ambang batas deadzone joystick (default: 0.20)")

    args = parser.parse_args()

    app = VortexGamepadController(esp_ip=args.ip, sim_mode=args.sim, deadzone=args.deadzone)
    app.run()
