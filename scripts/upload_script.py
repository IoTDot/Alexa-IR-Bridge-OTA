# pyright: reportUndefinedVariable=false
Import("env")
import os
import subprocess

# Funkcja wywoływana po kompilacji
def after_build(source, target, env):
    # Ścieżka do pliku binarnego na serwerze
    firmware_path_remote = os.path.abspath(str(target[0]))  # Absolutna ścieżka pliku na serwerze
    firmware_path_local = "C:/Compiled/Firmware/firmware.bin".replace("\\", "/")  # Ścieżka na laptopie

    # Tworzenie folderu lokalnego na laptopie (opcjonalne, gdy plik ma być zapisany)
    os.makedirs("C:/Compiled/Firmware", exist_ok=True)

    # Adres IP laptopa (192.168.50.90)
    laptop_ip = "192.168.50.90"

    # SCP do przesyłania pliku na laptop
    scp_command = f"scp {firmware_path_remote} dot@{laptop_ip}:{firmware_path_local}"
    print(f"Przesyłanie pliku: {scp_command}")
    result = subprocess.run(scp_command, shell=True)
    if result.returncode != 0:
        print("Błąd: Nie udało się przesłać pliku przez SCP.")
        env.Exit(1)

    # Wgrywanie firmware na ESP przy użyciu Pythona do uruchomienia esptool
    remote_flash_command = (
        f"ssh dot@{laptop_ip} 'python -m esptool --port COM5 write_flash 0x00000 {firmware_path_local}'"
    )
    print(f"Wgrywanie firmware: {remote_flash_command}")
    result = subprocess.run(remote_flash_command, shell=True)
    if result.returncode != 0:
        print("Błąd: Nie udało się wgrać firmware na ESP.")
        env.Exit(1)

# Dodanie post-akcji po procesie kompilacji
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
