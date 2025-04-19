# pyright: reportUndefinedVariable=false
Import("env")
import os
import socket
import sys
import time
import logging
from pathlib import Path
from typing import Optional, Tuple, List

import paramiko

# Konfiguracja loggera
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
logger = logging.getLogger(__name__)

# Stałe SSH
SSH_USER: str = "dot"
SSH_PORT: int = 22
SSH_KEY: Path = Path.home() / ".ssh" / "id_ed25519"

# Mapa obsługiwanych płytek -> (chip, token)
BOARD_MAP = {
    "esp01": ("esp8266", "esp01"),
    "nodemcuv2": ("esp8266", "esp8266"),
    "esp32": ("esp32", None),
}


def get_client_ip() -> Optional[str]:
    """
    Wykrywa adres IP klienta przez zmienne środowiskowe SSH_CONNECTION, SSH_CLIENT lub CLIENT_IP.
    """
    logger.debug("Sprawdzam zmienne SSH_CONNECTION, SSH_CLIENT oraz CLIENT_IP...")
    ssh_conn = os.environ.get("SSH_CONNECTION") or os.environ.get("SSH_CLIENT")
    if ssh_conn:
        ip = ssh_conn.split()[0]
        logger.info("Wykryty adres IP klienta: %s", ip)
        return ip

    client_ip = os.environ.get("CLIENT_IP")
    if client_ip:
        logger.info("Używam ręcznie ustawionego CLIENT_IP: %s", client_ip)
        return client_ip

    logger.warning("Nie udało się wykryć IP klienta. Upload tylko do pliku.")
    return None


def ssh_connect(ip: str, timeout: int = 5) -> paramiko.SSHClient:
    """
    Nawiązuje połączenie SSH z podanym adresem IP.
    """
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    logger.info("Łączę z %s przez SSH (timeout %ss)...", ip, timeout)
    client.connect(
        hostname=ip,
        port=SSH_PORT,
        username=SSH_USER,
        key_filename=str(SSH_KEY),
        timeout=timeout,
        allow_agent=True,
        look_for_keys=True,
    )
    return client


def run_cmd(
    client: paramiko.SSHClient,
    cmd: str,
    timeout: int = 10,
    get_pty: bool = False,
) -> Tuple[int, str, str]:
    """
    Wykonuje zdalne polecenie i zwraca (kod wyjścia, stdout, stderr).
    """
    stdin, stdout, stderr = client.exec_command(cmd, timeout=timeout, get_pty=get_pty)
    out = stdout.read().decode(errors="replace")
    err = stderr.read().decode(errors="replace")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err


def run_cmd_live(client: paramiko.SSHClient, cmd: str) -> int:
    """
    Wykonuje komendę SSH i wypisuje output na żywo.
    """
    transport = client.get_transport()
    channel = transport.open_session()
    channel.exec_command(cmd)

    stdout = channel.makefile("r")
    stderr = channel.makefile_stderr("r")

    while not channel.exit_status_ready():
        while channel.recv_ready():
            line = stdout.readline()
            if line:
                print(line.rstrip())
        while channel.recv_stderr_ready():
            line = stderr.readline()
            if line:
                print(line.rstrip(), file=sys.stderr)
        time.sleep(0.1)

    for line in stdout:
        print(line.rstrip())
    for line in stderr:
        print(line.rstrip(), file=sys.stderr)

    return channel.recv_exit_status()


def mkdir_remote(client: paramiko.SSHClient, path: str) -> None:
    """
    Tworzy zdalny katalog w Windows-ie (jeśli nie istnieje).
    """
    cmd = f'if not exist "{path}" mkdir "{path}"'
    rc, out, err = run_cmd(client, cmd)
    logger.debug("mkdir rc=%d out=%s err=%s", rc, out.strip(), err.strip())


def sftp_upload(client: paramiko.SSHClient, local: Path, remote: str) -> None:
    """
    Uploaduje plik lokalny na zdalny serwer.
    """
    logger.info("Kopiuję plik %s → %s", local, remote)
    with client.open_sftp() as sftp:
        remote_dir = os.path.dirname(remote)
        try:
            sftp.stat(remote_dir)
        except IOError:
            logger.info("Katalog nie istnieje, tworzę: %s", remote_dir)
            mkdir_remote(client, remote_dir)
        sftp.put(str(local), remote)
    logger.info("Kopiowanie zakończone.")


def list_com_ports(client: paramiko.SSHClient) -> List[str]:
    """
    Zwraca listę portów COM na zdalnej maszynie.
    """
    code = (
        "import serial.tools.list_ports;"
        "print('\\n'.join(p.device for p in serial.tools.list_ports.comports()))"
    )
    cmd = f'python -c "{code}"'
    rc, out, err = run_cmd(client, cmd)
    logger.debug("LIST_COM rc=%d out=%s err=%s", rc, out.strip(), err.strip())
    if rc != 0:
        return []
    return [l.strip() for l in out.splitlines() if l.strip()]


def detect_chip(
    client: paramiko.SSHClient,
    port: str,
    expected_chip: Optional[str],
) -> Optional[str]:
    """
    Wykrywa rodzaj układu (ESP8266/ESP32) na podanym porcie.
    """
    chip_arg = f"--chip {expected_chip}" if expected_chip else ""
    cmd = f'python -m esptool {chip_arg} --port {port} --baud 115200 chip_id'
    rc, out, err = run_cmd(client, cmd, get_pty=True)
    full = out + err
    if "Chip is ESP8266" in full:
        return "esp8266"
    if "Chip is ESP32" in full:
        return "esp32"
    return None


def after_build(source, target, env) -> None:
    """
    Akcje wykonywane po kompilacji PlatformIO.
    """
    if os.environ.get("GITHUB_ACTIONS"):
        logger.info("CI detected – skip post-build.")
        return

    logger.info("Proces po kompilacji...")
    board: str = env['PIOENV']
    logger.info("Płytka: %s", board)

    exp_chip, exp_token = next(
        (v for k, v in BOARD_MAP.items() if k in board.lower()),
        (None, None)
    )

    fw_src = Path(env.subst("$BUILD_DIR")) / "firmware.bin"
    fw_dst = Path("C:/Compiled/Firmware") / f"firmware_{board}.bin"
    logger.info("Firmware lokalnie: %s", fw_src)
    logger.info("Firmware zdalnie: %s", fw_dst)

    ip = get_client_ip()
    if not ip:
        logger.warning("Brak IP – pomijam upload i flash.")
        return

    try:
        with ssh_connect(ip) as client:
            mkdir_remote(client, str(fw_dst.parent))
            sftp_upload(client, fw_src, str(fw_dst))

            ports = list_com_ports(client)
            if not ports:
                logger.error("Nie znaleziono portów COM.")
                return

            chosen: Optional[Tuple[str, str]] = None
            for p in ports:
                chip = detect_chip(client, p, exp_chip)
                if not chip or (exp_chip and chip != exp_chip):
                    continue
                logger.info("🔍 Port %s → %s", p, chip)
                chosen = (p, chip)
                break

            if not chosen:
                logger.error("Nie znaleziono oczekiwanego urządzenia.")
                return

            port, chip = chosen
            logger.info("Rozpoczynam kasowanie flash (erase_flash)...")
            erc = run_cmd_live(client, f'python -m esptool --chip {chip} --port {port} --baud 921600 erase_flash')
            logger.info("ERASE zakończone z kodem: %d", erc)
            if erc != 0:
                logger.warning("Błąd erase_flash – przerwanie.")
                return

            logger.info("Rozpoczynam flashowanie (write_flash)...")
            frc = run_cmd_live(client, f'python -m esptool --chip {chip} --port {port} --baud 921600 write_flash 0x00000 "{fw_dst}"')
            logger.info("FLASH zakończone z kodem: %d", frc)
            if frc == 0:
                logger.info("✅ Flashowanie zakończone sukcesem!")
            else:
                logger.error("⚠️ Flashowanie nie powiodło się.")

    except (socket.timeout, paramiko.SSHException) as e:
        logger.error("❌ Błąd SSH: %s", e)

# Rejestracja akcji po buildzie w PlatformIO
env.AddPostAction("buildprog", after_build)
