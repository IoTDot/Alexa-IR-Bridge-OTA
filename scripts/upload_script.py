# pyright: reportUndefinedVariable=false
Import("env")

import os             # operacje na systemie plików i zmienne środowiskowe
import sys            # dostęp do stderr
import time           # opóźnienia w pętli odczytu
import logging        # moduł logowania
from pathlib import Path            # wygodna praca ze ścieżkami
from typing import Optional, Tuple, List  # typowanie zmiennych


# -------------------------------------------------------------
# Globalne stałe (można nadpisać przez zmienne środowiskowe)
# -------------------------------------------------------------
FIRMWARE_OUTPUT_DIR = Path(
    os.environ.get("FIRMWARE_OUTPUT", "C:/Compiled/Firmware")
)
SSH_TIMEOUT       = int(os.environ.get("SSH_TIMEOUT", "5"))       # domyślny timeout SSH w sekundach
BAUD_DETECT       = int(os.environ.get("BAUD_DETECT", "115200"))  # prędkość do detect_chip
BAUD_FLASH        = int(os.environ.get("BAUD_FLASH", "921600"))   # prędkość do erase/write_flash

# -------------------------------------------------------------
# Konfiguracja loggera: wyświetlanie czasu, poziomu i wiadomości
# -------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
logger = logging.getLogger(__name__)

# -------------------------------------------------------------
# Mapa obsługiwanych płytek -> (chip, token)
# klucz to fragment nazwy środowiska PIOENV
# -------------------------------------------------------------
BOARD_MAP = {
    "esp01":     ("esp8266", "esp01"),
    "nodemcuv2": ("esp8266", "esp8266"),
    "esp32":     ("esp32",   None),
}

# -------------------------------------------------------------
# Funkcja wykrywająca IP klienta
# -------------------------------------------------------------
def get_client_ip() -> Optional[str]:
    """
    Próbuje odczytać adres IP z zmiennych SSH_CONNECTION, SSH_CLIENT lub CLIENT_IP.
    Zwraca None, jeśli nie uda się wykryć.
    """
    logger.debug("Sprawdzam zmienne SSH_CONNECTION, SSH_CLIENT oraz CLIENT_IP...")
    ssh_conn = os.environ.get("SSH_CONNECTION") or os.environ.get("SSH_CLIENT")
    if ssh_conn:
        ip = ssh_conn.split()[0]
        logger.info(f"Wykryty adres IP klienta: {ip}")
        return ip

    client_ip = os.environ.get("CLIENT_IP")
    if client_ip:
        logger.info(f"Używam ręcznie ustawionego CLIENT_IP: {client_ip}")
        return client_ip

    logger.warning("❌ Nie udało się wykryć IP klienta. ❎ Upload tylko do pliku.")
    return None

# -------------------------------------------------------------
# Funkcja nawiązująca połączenie SSH
# -------------------------------------------------------------
def ssh_connect(ip: str, timeout: int = SSH_TIMEOUT) -> paramiko.SSHClient:
    user = os.environ.get("USER") or getpass.getuser()
    client = paramiko.SSHClient()
    client.load_system_host_keys()
    # automatyczne dodawanie nieznanych kluczy do known_hosts
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    logger.info(f"Łączę z {user}@{ip} przez SSH (timeout {timeout}s)…")
    client.connect(
        hostname=ip,
        username=user,
        timeout=timeout,
        allow_agent=True,
        look_for_keys=True,
    )
    return client

# -------------------------------------------------------------
# Wykonanie pojedynczego polecenia SSH
# -------------------------------------------------------------
def run_cmd(
    client: paramiko.SSHClient,
    cmd: str,
    timeout: int = 10,
    get_pty: bool = False,
) -> Tuple[int, str, str]:
    stdin, stdout, stderr = client.exec_command(cmd, timeout=timeout, get_pty=get_pty)
    out = stdout.read().decode(errors="replace")
    err = stderr.read().decode(errors="replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

# -------------------------------------------------------------
# Wykonanie polecenia SSH i wyświetlanie outputu na żywo
# -------------------------------------------------------------
def run_cmd_live(client: paramiko.SSHClient, cmd: str) -> int:
    transport = client.get_transport()
    channel = transport.open_session()
    channel.exec_command(cmd)
    channel.settimeout(2)

    stdout_buffer = []
    stderr_buffer = []

    def process_buffer(buffer, is_stderr=False):
        data = "".join(buffer)
        lines = []
        while True:
            idx = max(data.find("\r\n"), data.find("\n"), data.find("\r"))
            if idx < 0:
                break
            line = data[:idx].rstrip()
            line_ending_len = 2 if data[idx:idx+2] == "\r\n" else 1
            data = data[idx+line_ending_len:]
            lines.append(line)
        if lines:
            for line in lines:
                if is_stderr:
                    logger.error(line)
                else:
                    logger.info(line)
        return list(data)

    try:
        while not channel.exit_status_ready():
            if channel.recv_ready():
                part = channel.recv(4096).decode(errors="replace")
                stdout_buffer = process_buffer(stdout_buffer + list(part))
            if channel.recv_stderr_ready():
                part = channel.recv_stderr(4096).decode(errors="replace")
                stderr_buffer = process_buffer(stderr_buffer + list(part), is_stderr=True)
            time.sleep(0.05)

        final_reads = 0
        while final_reads < 3:
            if channel.recv_ready():
                part = channel.recv(4096).decode(errors="replace")
                stdout_buffer = process_buffer(stdout_buffer + list(part))
                final_reads = 0
            elif channel.recv_stderr_ready():
                part = channel.recv_stderr(4096).decode(errors="replace")
                stderr_buffer = process_buffer(stderr_buffer + list(part), is_stderr=True)
                final_reads = 0
            else:
                final_reads += 1
                time.sleep(0.1)

        if stdout_buffer:
            logger.info("".join(stdout_buffer))
        if stderr_buffer:
            logger.error("".join(stderr_buffer))

    except socket.timeout:
        pass
    finally:
        if not channel.closed:
            channel.close()

    return channel.recv_exit_status()

# -------------------------------------------------------------
# Tworzenie katalogu na zdalnym Windows-ie
# -------------------------------------------------------------
def mkdir_remote(client: paramiko.SSHClient, path: str) -> None:
    cmd = f'if not exist "{path}" mkdir "{path}"'
    rc, out, err = run_cmd(client, cmd)
    logger.debug(f"mkdir rc={rc} out={out.strip()} err={err.strip()}")

# -------------------------------------------------------------
# Upload pliku przez SFTP
# -------------------------------------------------------------
def sftp_upload(client: paramiko.SSHClient, local: Path, remote: str) -> None:
    logger.info(f"⏳ Kopiuję plik {local} → {remote}")
    with client.open_sftp() as sftp:
        remote_dir = os.path.dirname(remote)
        try:
            sftp.stat(remote_dir)
        except IOError:
            logger.info(f"Katalog nie istnieje, tworzę: {remote_dir}")
            mkdir_remote(client, remote_dir)
        sftp.put(str(local), remote)
    logger.info("✅ Kopiowanie zakończone.")

# -------------------------------------------------------------
# Lista portów COM na zdalnej maszynie
# -------------------------------------------------------------
def list_com_ports(client: paramiko.SSHClient) -> List[str]:
    code = (
        "import serial.tools.list_ports;"
        "print('\\n'.join(p.device for p in serial.tools.list_ports.comports()))"
    )
    cmd = f'python -c "{code}"'
    rc, out, err = run_cmd(client, cmd)
    logger.debug(f"LIST_COM rc={rc} out={out.strip()} err={err.strip()}")
    return [l.strip() for l in out.splitlines() if rc == 0 and l.strip()]

# -------------------------------------------------------------
# Wykrywanie typu układu (ESP8266/ESP32)
# -------------------------------------------------------------
def detect_chip(
    client: paramiko.SSHClient,
    port: str,
    expected_chip: Optional[str],
) -> Optional[str]:
    chip_arg = f"--chip {expected_chip}" if expected_chip else ""
    cmd = f'python -m esptool {chip_arg} --port {port} --baud {BAUD_DETECT} chip_id'
    rc, out, err = run_cmd(client, cmd, get_pty=True)
    full = out + err
    if "Chip is ESP8266" in full:
        return "esp8266"
    if "Chip is ESP32" in full:
        return "esp32"
    return None

# -------------------------------------------------------------
# Dodatkowa weryfikacja tokena przez esptool flash_id
# -------------------------------------------------------------
def check_flash_id(
    client: paramiko.SSHClient,
    port: str,
    expected_token: Optional[str],
) -> bool:
    if not expected_token:
        return True

    cmd = f'python -m esptool --port {port} flash_id'
    rc, out, err = run_cmd(client, cmd, get_pty=True)
    full = (out + err).lower()
    if expected_token.lower() in full:
        logger.info(f"🔑 Token '{expected_token}' znaleziony w flash_id na {port}")
        return True
    else:
        logger.warning(
            f"❌ Token '{expected_token}' nie znaleziony w flash_id na {port}. Odrzucam port."
        )
        return False

# -------------------------------------------------------------
# Główna funkcja wywoływana po buildzie PlatformIO
# -------------------------------------------------------------
def after_build(source, target, env) -> None:
    if os.environ.get("GITHUB_ACTIONS"):
        logger.info("CI detected – skip post-build.")
        return
    import getpass        # pobieranie nazwy aktualnego użytkownika
    import paramiko       # SSH i SFTP
    import socket         # obsługa wyjątków związanych z siecią

    logger.info("Proces po kompilacji...")
    board: str = env['PIOENV']
    logger.info(f"Płytka: {board}")

    exp_chip, exp_token = next(
        (v for k, v in BOARD_MAP.items() if k in board.lower()),
        (None, None)
    )

    fw_src = Path(env.subst("$BUILD_DIR")) / "firmware.bin"
    fw_dst = FIRMWARE_OUTPUT_DIR / f"firmware_{board}.bin"
    logger.info(f"Firmware lokalnie: {fw_src}")
    logger.info(f"Firmware zdalnie: {fw_dst}")

    ip = get_client_ip()
    if not ip:
        logger.warning("⛔ Brak IP – pomijam upload i flash.")
        return

    try:
        with ssh_connect(ip) as client:
            mkdir_remote(client, str(fw_dst.parent))
            sftp_upload(client, fw_src, str(fw_dst))

            ports = list_com_ports(client)
            if not ports:
                logger.error("⛔ Nie znaleziono portów COM.")
                return

            # wybór pierwszego pasującego portu (z dodatkowym sprawdzeniem tokena)
            chosen: Optional[Tuple[str, str]] = None
            for p in ports:
                chip = detect_chip(client, p, exp_chip)
                if not chip or (exp_chip and chip != exp_chip):
                    continue

                if chip == "esp8266" and exp_token:
                    if not check_flash_id(client, p, exp_token):
                        continue

                logger.info(f"🔍 Port {p} → {chip}")
                chosen = (p, chip)
                break

            if not chosen:
                logger.error("⛔ Nie znaleziono oczekiwanego urządzenia.")
                return

            port, chip = chosen
            logger.info("🔥 Rozpoczynam kasowanie flash (erase_flash)…")
            erc = run_cmd_live(
                client,
                f'python -m esptool --chip {chip} --port {port} --baud {BAUD_FLASH} erase_flash'
            )
            logger.info(f"💡 ERASE zakończone z kodem: {erc}")
            if erc != 0:
                logger.warning("💥 Błąd erase_flash – przerwanie.")
                return

            logger.info("💾 Rozpoczynam flashowanie (write_flash)…")
            frc = run_cmd_live(
                client,
                f'python -m esptool --chip {chip} --port {port} --baud {BAUD_FLASH} write_flash 0x00000 "{fw_dst}"'
            )
            logger.info(f"💡 FLASH zakończone z kodem: {frc}")
            if frc == 0:
                logger.info("✅ Flashowanie zakończone sukcesem!")
            else:
                logger.error("⚠️ Flashowanie nie powiodło się.")

    except (socket.timeout, paramiko.SSHException) as e:
        logger.error(f"❌ Błąd SSH: {e}")

# -------------------------------------------------------------
# Rejestracja akcji po buildzie w PlatformIO
# -------------------------------------------------------------
env.AddPostAction("buildprog", after_build)
