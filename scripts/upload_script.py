# *********************************************************************
# UWAGA!
# Aby ten skrypt działał poprawnie, na laptopie (komputerze, do którego
# łączysz się przez SSH) muszą być zainstalowane następujące pakiety:
#
#   esptool  - do wgrywania firmware na ESP
#   pyserial - do automatycznego wykrywania portu COM
#
# Możesz je zainstalować poleceniami:
#   python -m pip install esptool
#   python -m pip install pyserial
# *********************************************************************

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
    print("⚠️  Ostrzeżenie: Nie udało się wykryć adresu IP klienta (SSH_CONNECTION). Pomijam upload.")
    return None

def get_com_port_and_chip(laptop_ip):
    """
    Wykrywa port COM na laptopie Windows, na którym podłączone jest ESP, oraz określa typ układu.
    """
    print("Rozpoczynam wykrywanie portów COM na laptopie:", laptop_ip)
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
        return None, None
    
    ports = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    print("Znalezione porty:", ports)
    
    for port in ports:
        test_cmd = f"ssh dot@{laptop_ip} 'python -m esptool --port {port} --after no_reset chip_id'"
        print(f"Testowanie portu {port} za pomocą polecenia:")
        print(test_cmd)
        test_result = subprocess.run(test_cmd, shell=True, capture_output=True, text=True)
        print(f"Wynik testu portu {port}:")
        print("STDOUT:", test_result.stdout)
        print("STDERR:", test_result.stderr)
        chip_type = None
        if "Chip is ESP8266" in test_result.stdout:
            chip_type = "esp8266"
        elif "Chip is ESP32" in test_result.stdout:
            chip_type = "esp32"
        
        if chip_type:
            print(f"Znaleziono ESP na porcie: {port}, typ: {chip_type}")
            return port, chip_type

    print("Błąd: Nie wykryto ESP na żadnym z dostępnych portów.")
    return None, None

def after_build(source, target, env):
    """
    Po kompilacji:
      1. Przesyłamy firmware na laptopa.
      2. Wykrywamy port COM i typ chipu.
      3. Czyścimy flash.
      4. Flashujemy firmware.
    """

    if os.environ.get("GITHUB_ACTIONS"):
        print("Wykryto środowisko CI, pomijam post-build actions.")
        return

    print("Proces po kompilacji rozpoczęty...")
    firmware_path_remote = os.path.abspath(str(target[0]))
    firmware_path_local = "C:/Compiled/Firmware/firmware.bin"
    
    print("Firmware (Linux):", firmware_path_remote)
    print("Firmware (Windows):", firmware_path_local)
    
    laptop_ip = get_client_ip()
    if not laptop_ip:
        print("Kompilacja zakończona sukcesem, ale upload został pominięty.")
        return

    print("Adres IP laptopa:", laptop_ip)
    
    mkdir_cmd = f'ssh dot@{laptop_ip} "mkdir \\"C:/Compiled/Firmware\\" 2>nul"'
    print("Tworzę katalog na laptopie:")
    print(mkdir_cmd)
    subprocess.run(mkdir_cmd, shell=True)
    
    scp_command = f'scp "{firmware_path_remote}" dot@{laptop_ip}:"{firmware_path_local}"'
    print("Wysyłam plik firmware:")
    print(scp_command)
    scp_result = subprocess.run(scp_command, shell=True, capture_output=True, text=True)
    print("Wynik przesyłania pliku:")
    print("STDOUT:", scp_result.stdout)
    print("STDERR:", scp_result.stderr)
    if scp_result.returncode != 0:
        print("⚠️  Ostrzeżenie: Nie udało się przesłać pliku przez SCP. Upload pominięty.")
        return

    com_port, chip_type = get_com_port_and_chip(laptop_ip)
    if not com_port or not chip_type:
        print("⚠️  Ostrzeżenie: Nie udało się wykryć ESP. Upload pominięty.")
        return
    
    print("Wykryty port COM:", com_port)
    print("Wykryty typ chipu:", chip_type)
    
    baud_rate = 921600

    erase_cmd = f'ssh dot@{laptop_ip} "python -m esptool --chip {chip_type} --port {com_port} --baud {baud_rate} erase_flash"'
    print("Czyszczenie ESP (erase_flash):")
    print(erase_cmd)
    erase_result = subprocess.run(erase_cmd, shell=True)
    if erase_result.returncode != 0:
        print("⚠️  Ostrzeżenie: Nie udało się wyczyścić ESP. Upload pominięty.")
        return
    
    flash_cmd = f'ssh dot@{laptop_ip} "python -m esptool --chip {chip_type} --port {com_port} --baud {baud_rate} write_flash 0x00000 \\"{firmware_path_local}\\""'
    print("Rozpoczynam flashowanie przy użyciu polecenia:")
    print(flash_cmd)
    flash_result = subprocess.run(flash_cmd, shell=True)
    if flash_result.returncode != 0:
        print("⚠️  Ostrzeżenie: Nie udało się wgrać firmware na ESP. Upload pominięty.")
        return
    
    print("✅ Flashowanie zakończone sukcesem!")

# Wymusza wykonanie post-akcji za każdym razem
env.AlwaysBuild(".pio/build/esp01_1m/firmware.bin")
env.AlwaysBuild(".pio/build/nodemcuv2/firmware.bin")
env.AlwaysBuild(".pio/build/esp32dev/firmware.bin")

# Dodajemy post-akcje dla każdego środowiska
env.AddPostAction(".pio/build/esp01_1m/firmware.bin", after_build)
env.AddPostAction(".pio/build/nodemcuv2/firmware.bin", after_build)
env.AddPostAction(".pio/build/esp32dev/firmware.bin", after_build)
