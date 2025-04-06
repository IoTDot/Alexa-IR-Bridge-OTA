# pyright: reportUndefinedVariable=false
Import("env")
import os
import subprocess

def get_client_ip():
    """
    Pobiera adres IP klienta (laptopa Windows) z zmiennych środowiskowych.
    Próbuje: SSH_CONNECTION, SSH_CLIENT, a jeśli nic nie znajdzie, sprawdza zmienną CLIENT_IP.
    """
    print("Sprawdzam zmienne SSH_CONNECTION, SSH_CLIENT oraz CLIENT_IP...")
    ssh_conn = os.environ.get("SSH_CONNECTION") or os.environ.get("SSH_CLIENT")
    if ssh_conn:
        parts = ssh_conn.split()
        if parts:
            ip = parts[0]
            print(f"Wykryty adres IP klienta: {ip}")
            return ip
    client_ip = os.environ.get("CLIENT_IP")
    if client_ip:
        print(f"Używam ręcznie ustawionego adresu IP (CLIENT_IP): {client_ip}")
        return client_ip
    print("⚠️  Ostrzeżenie: Nie udało się wykryć adresu IP klienta. Upload zostanie wykonany tylko do pliku na kliencie.")
    return None

def get_com_port_and_chip(laptop_ip, expected_chip=None, expected_token=None):
    """
    Wykrywa port COM oraz typ układu.
    Jeśli expected_chip (np. "esp32" lub "esp8266") jest podany, zwraca tylko port,
    na którym wykryty chip odpowiada oczekiwanemu typowi.
    Jeśli expected_token (fragment nazwy, np. "esp01" lub "nodemcuv2") jest podany,
    dodatkowo wykonuje komendę flash_id i sprawdza, czy wynik zawiera ten token.
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
            if expected_chip and chip_type != expected_chip:
                print(f"Ostrzeżenie: wykryty chip '{chip_type}' nie odpowiada oczekiwanemu '{expected_chip}'. Pomijam ten port.")
                continue
            
            # Dodatkowa weryfikacja flash_id na podstawie expected_token
            if expected_token and chip_type == "esp8266":
                flash_cmd = f"ssh dot@{laptop_ip} 'python -m esptool --port {port} flash_id'"
                print(f"Testowanie flash_id dla portu {port}:")
                flash_result = subprocess.run(flash_cmd, shell=True, capture_output=True, text=True)
                print(f"Wynik flash_id dla portu {port}:")
                print("STDOUT:", flash_result.stdout)
                print("STDERR:", flash_result.stderr)
                if expected_token.lower() not in flash_result.stdout.lower():
                    print(f"Ostrzeżenie: flash_id nie zawiera oczekiwanego tokenu '{expected_token}'. Pomijam ten port.")
                    continue
            
            return port, chip_type

    print("Błąd: Nie wykryto urządzenia spełniającego oczekiwania na żadnym z dostępnych portów.")
    return None, None

def after_build(source, target, env):
    """
    Po kompilacji:
      1. Przesyłamy firmware na laptopa (Windows) do unikalnego pliku zależnego od nazwy płytki.
      2. Na podstawie nazwy folderu wyznaczamy oczekiwany typ chipu oraz token, który powinien być zawarty w flash_id.
      3. Próbuje wykryć port COM dla podłączonego urządzenia.
         Jeśli urządzenie zostanie znalezione i jego flash_id zawiera oczekiwany token – wykonuje flashowanie.
         W przeciwnym przypadku flashowanie i czyszczenie zostaje pominięte.
    """
    if os.environ.get("GITHUB_ACTIONS"):
        print("Wykryto środowisko CI, pomijam post-build actions.")
        return

    print("Proces po kompilacji rozpoczęty...")

    # Wyznaczamy nazwę płytki na podstawie środowiska
    board_name = env['PIOENV']
    print(f"Wykryto nazwę płytki: {board_name}")

    # Określamy oczekiwany typ chipu oraz token na podstawie nazwy płytki.
    # Przykładowo, jeśli board_name zawiera "esp01" to oczekujemy tokenu "esp01",
    # a jeśli "nodemcuv2" to token "nodemcuv2".
    expected_token = None
    if "esp01" in board_name.lower():
        expected_chip = "esp8266"
        expected_token = "esp01"
    elif "nodemcuv2" in board_name.lower():
        expected_chip = "esp8266"
        expected_token = "esp8266"
    elif "esp32" in board_name.lower():
        expected_chip = "esp32"
    else:
        expected_chip = None

    # Określamy ścieżkę do firmware na podstawie środowiska
    firmware_source_path = env.subst("$BUILD_DIR/firmware.bin")
    firmware_source_path = os.path.abspath(firmware_source_path)
    
    # Nadajemy unikalną nazwę pliku firmware, aby nie nadpisywał się przy kolejnych uploadach.
    firmware_filename = f"firmware_{board_name}.bin"
    firmware_path_local = os.path.join("C:/Compiled/Firmware", firmware_filename)

    print("Firmware (Linux):", firmware_source_path)
    print("Firmware (Windows):", firmware_path_local)
    
    laptop_ip = get_client_ip()
    # Nawet jeśli nie wykryjemy IP, wykonamy kopię pliku.
    if laptop_ip:
        print("Adres IP laptopa:", laptop_ip)
        mkdir_cmd = f'ssh dot@{laptop_ip} "mkdir \\"C:/Compiled/Firmware\\" 2>nul"'
        print("Tworzę katalog na laptopie:")
        print(mkdir_cmd)
        subprocess.run(mkdir_cmd, shell=True)
        
        scp_command = f'scp "{firmware_source_path}" dot@{laptop_ip}:"{firmware_path_local}"'
        print("Wysyłam plik firmware:")
        print(scp_command)
        scp_result = subprocess.run(scp_command, shell=True, capture_output=True, text=True)
        print("Wynik przesyłania pliku:")
        print("STDOUT:", scp_result.stdout)
        print("STDERR:", scp_result.stderr)
        if scp_result.returncode != 0:
            print("⚠️  Ostrzeżenie: Nie udało się przesłać pliku przez SCP.")
    else:
        print("Brak adresu IP – kopiowanie pliku na klienta zostało pominięte.")
        # Jeśli nie mamy IP, nie próbujemy flashowania.
        return

    # Próbujemy wykryć urządzenie o oczekiwanym typie i tokenie (jeśli dotyczy).
    com_port, chip_type = get_com_port_and_chip(laptop_ip, expected_chip, expected_token)
    if not com_port or not chip_type:
        print(f"Nie wykryto urządzenia typu '{expected_chip}' z tokenem '{expected_token}'. Firmware został przesłany, flashowanie pominięte.")
        return
    
    print("Wykryty port COM:", com_port)
    print("Wykryty typ chipu:", chip_type)
    
    baud_rate = 921600

    erase_cmd = f'ssh dot@{laptop_ip} "python -m esptool --chip {chip_type} --port {com_port} --baud {baud_rate} erase_flash"'
    print("Czyszczenie ESP (erase_flash):")
    print(erase_cmd)
    erase_result = subprocess.run(erase_cmd, shell=True)
    if erase_result.returncode != 0:
        print("⚠️  Ostrzeżenie: Nie udało się wyczyścić ESP. Flashowanie pominięte.")
        return
    
    flash_cmd = f'ssh dot@{laptop_ip} "python -m esptool --chip {chip_type} --port {com_port} --baud {baud_rate} write_flash 0x00000 \\"{firmware_path_local}\\""'
    print("Rozpoczynam flashowanie przy użyciu polecenia:")
    print(flash_cmd)
    flash_result = subprocess.run(flash_cmd, shell=True)
    if flash_result.returncode != 0:
        print("⚠️  Ostrzeżenie: Nie udało się wgrać firmware na ESP. Flashowanie pominięte.")
        return
    
    print("✅ Flashowanie zakończone sukcesem!")

# Dodajemy post-akcję dla celu "buildprog", który triggeruje się po budowie firmware
env.AddPostAction("buildprog", after_build)
