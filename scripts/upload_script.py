# pyright: reportUndefinedVariable=false
Import("env")
import os
import subprocess

def get_client_ip():
    """
    Pobiera adres IP klienta (laptopa Windows) z zmiennej środowiskowej SSH_CONNECTION.
    """
    print("Sprawdzam zmienną SSH_CONNECTION...")
    ssh_conn = os.environ.get("SSH_CONNECTION")
    print(f"SSH_CONNECTION: {ssh_conn}")
    if ssh_conn:
        parts = ssh_conn.split()
        if parts:
            ip = parts[0]
            print(f"Wykryty adres IP klienta: {ip}")
            return ip
    print("Błąd: Nie udało się wykryć adresu IP klienta (SSH_CONNECTION).")
    env.Exit(1)

def get_com_port(laptop_ip):
    """
    Wykrywa port COM na laptopie Windows, na którym podłączone jest ESP.
    Uruchamia zdalnie polecenie Pythona, które wypisuje dostępne porty COM,
    a następnie testuje każdy z nich przy użyciu esptool.
    """
    print("Rozpoczynam wykrywanie portów COM na laptopie:", laptop_ip)
    # Polecenie, które działa lokalnie:
    # python -c "import serial.tools.list_ports; print('\n'.join([p.device for p in serial.tools.list_ports.comports()]))"
    # Aby przekazać je przez SSH, stosujemy odpowiednie escapowanie:
    remote_cmd = 'python -c "import serial.tools.list_ports; print(\\"\\\\n\\".join([p.device for p in serial.tools.list_ports.comports()]))"'
    ssh_cmd = f"ssh dot@{laptop_ip} '{remote_cmd}'"
    print("Polecenie do wykrycia portów COM:")
    print(ssh_cmd)
    
    result = subprocess.run(ssh_cmd, shell=True, capture_output=True, text=True)
    print("Wynik wykrywania portów:")
    print("STDOUT:", result.stdout)
    print("STDERR:", result.stderr)
    if result.returncode != 0:
        print("Błąd wykonywania polecenia do wykrycia portów.")
        env.Exit(1)
    
    ports = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    print("Znalezione porty:", ports)
    
    # Testujemy każdy port – sprawdzamy czy esptool zwraca komunikat charakterystyczny dla ESP
    for port in ports:
        test_cmd = f"ssh dot@{laptop_ip} 'python -m esptool --port {port} --after no_reset chip_id'"
        print(f"Testowanie portu {port} za pomocą polecenia:")
        print(test_cmd)
        test_result = subprocess.run(test_cmd, shell=True, capture_output=True, text=True)
        print(f"Wynik testu portu {port}:")
        print("STDOUT:", test_result.stdout)
        print("STDERR:", test_result.stderr)
        if "Chip is ESP8266" in test_result.stdout or "Chip is ESP32" in test_result.stdout:
            print(f"Znaleziono ESP na porcie: {port}")
            return port
    
    print("Błąd: Nie wykryto ESP na żadnym z dostępnych portów.")
    env.Exit(1)

def after_build(source, target, env):
    """
    Po kompilacji:
      1. Przesyłamy firmware (firmware.bin) z Linuxa na laptopa z Windows.
      2. Wykrywamy na laptopie port COM, na którym jest ESP.
      3. Wgrywamy firmware na ESP.
    """
    print("Proces po kompilacji rozpoczęty...")
    # Ścieżka do firmware skompilowanego na Linuxie
    firmware_path_remote = os.path.abspath(str(target[0]))
    # Docelowa ścieżka na laptopie (Windows)
    firmware_path_local = "C:/Compiled/Firmware/firmware.bin"
    
    print("Firmware (Linux):", firmware_path_remote)
    print("Firmware (Windows):", firmware_path_local)
    
    # Pobierz adres IP klienta (laptopa Windows) z SSH_CONNECTION
    laptop_ip = get_client_ip()
    print("Adres IP laptopa:", laptop_ip)
    
    # Utworzenie katalogu na laptopie (przez SSH na Windows)
    mkdir_cmd = f'ssh dot@{laptop_ip} "mkdir \\"C:/Compiled/Firmware\\" 2>nul"'
    print("Tworzę katalog na laptopie:")
    print(mkdir_cmd)
    subprocess.run(mkdir_cmd, shell=True)
    
    # Przesyłanie pliku firmware z Linuxa na laptopa Windows przez SCP
    scp_command = f'scp "{firmware_path_remote}" dot@{laptop_ip}:"{firmware_path_local}"'
    print("Wysyłam plik firmware:")
    print(scp_command)
    scp_result = subprocess.run(scp_command, shell=True, capture_output=True, text=True)
    print("Wynik przesyłania pliku:")
    print("STDOUT:", scp_result.stdout)
    print("STDERR:", scp_result.stderr)
    if scp_result.returncode != 0:
        print("Błąd: Nie udało się przesłać pliku przez SCP.")
        env.Exit(1)
    
    # Wykrycie portu COM, na którym podłączone jest ESP
    com_port = get_com_port(laptop_ip)
    print("Wykryty port COM:", com_port)
    
    # Flashowanie firmware na ESP przy użyciu esptool na laptopie Windows
    flash_cmd = f'ssh dot@{laptop_ip} "python -m esptool --port {com_port} write_flash 0x00000 \\"{firmware_path_local}\\""'
    print("Rozpoczynam flashowanie przy użyciu polecenia:")
    print(flash_cmd)
    flash_result = subprocess.run(flash_cmd, shell=True, capture_output=True, text=True)
    print("Wynik flashowania:")
    print("STDOUT:", flash_result.stdout)
    print("STDERR:", flash_result.stderr)
    if flash_result.returncode != 0:
        print("Błąd: Nie udało się wgrać firmware na ESP.")
        env.Exit(1)
    
    print("Flashowanie zakończone sukcesem!")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
