# pyright: reportUndefinedVariable=false
Import("env")
import os
import socket
import paramiko
import time
import sys

SSH_USER = "dot"
SSH_PORT = 22
SSH_KEY = os.path.expanduser("~/.ssh/id_ed25519")  # lub zmień według potrzeb

def get_client_ip():
    print("Sprawdzam zmienne SSH_CONNECTION, SSH_CLIENT oraz CLIENT_IP...")
    ssh_conn = os.environ.get("SSH_CONNECTION") or os.environ.get("SSH_CLIENT")
    if ssh_conn:
        ip = ssh_conn.split()[0]
        print(f"Wykryty adres IP klienta: {ip}")
        return ip
    client_ip = os.environ.get("CLIENT_IP")
    if client_ip:
        print(f"Używam ręcznie ustawionego CLIENT_IP: {client_ip}")
        return client_ip
    print("⚠️ Nie udało się wykryć IP klienta. Upload tylko do pliku.")
    return None

def ssh_connect(ip, timeout=5):
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    print(f"Łączę z {ip} przez SSH (timeout {timeout}s)...")
    client.connect(
        ip, port=SSH_PORT, username=SSH_USER,
        key_filename=SSH_KEY, timeout=timeout,
        allow_agent=True, look_for_keys=True
    )
    return client

def run_cmd(client, cmd, timeout=10, get_pty=False):
    stdin, stdout, stderr = client.exec_command(cmd, timeout=timeout, get_pty=get_pty)
    out = stdout.read().decode(errors="ignore")
    err = stderr.read().decode(errors="ignore")
    rc = stdout.channel.recv_exit_status()
    return rc, out, err

def run_cmd_live(client, cmd, timeout=None):
    """
    Wykonuje komendę SSH i wypisuje output na żywo, bez resetowania terminala.
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

def mkdir_remote(client, path):
    cmd = f'if not exist "{path}" mkdir "{path}"'
    rc, out, err = run_cmd(client, cmd, timeout=10)
    print(f" mkdir rc={rc}  out={out.strip()}  err={err.strip()}")

def sftp_upload(client, local, remote):
    print(f"Kopiuję plik {local} → {remote}")
    sftp = client.open_sftp()
    remote_dir = os.path.dirname(remote)
    try:
        sftp.stat(remote_dir)
    except IOError:
        print("Katalog nie istnieje, tworzę:", remote_dir)
        mkdir_remote(client, remote_dir)
    sftp.put(local, remote)
    sftp.close()
    print("Kopiowanie zakończone.")

def list_com_ports(client):
    code = (
        "import serial.tools.list_ports;"
        "print('\\n'.join(p.device for p in serial.tools.list_ports.comports()))"
    )
    rc, out, err = run_cmd(client, f'python -c "{code}"', timeout=5)
    print("LIST_COM rc=", rc, " out=", out.strip(), " err=", err.strip())
    if rc != 0:
        return []
    return [l.strip() for l in out.splitlines() if l.strip()]

def detect_chip(client, port, expected_chip):
    cmd = (
        f'python -m esptool --chip {expected_chip or ""} '
        f'--port {port} --baud 115200 chip_id'
    )
    rc, out, err = run_cmd(client, cmd, timeout=10, get_pty=True)
    full = out + err
    if "Chip is ESP8266" in full:
        return "esp8266"
    if "Chip is ESP32" in full:
        return "esp32"
    return None

def after_build(source, target, env):
    if os.environ.get("GITHUB_ACTIONS"):
        print("CI detected – skip post-build.")
        return

    print("Proces po kompilacji...")
    board = env['PIOENV']
    print("Płytka:", board)
    exp_token = None
    if "esp01" in board.lower():
        exp_chip, exp_token = "esp8266", "esp01"
    elif "nodemcuv2" in board.lower():
        exp_chip, exp_token = "esp8266", "esp8266"
    elif "esp32" in board.lower():
        exp_chip, exp_token = "esp32", None
    else:
        exp_chip, exp_token = None, None

    fw_src = os.path.abspath(env.subst("$BUILD_DIR/firmware.bin"))
    fw_dst = f"C:/Compiled/Firmware/firmware_{board}.bin"
    print("Firmware lokalnie:", fw_src)
    print("Firmware zdalnie:", fw_dst)

    ip = get_client_ip()
    if not ip:
        print("Brak IP – pomijam upload i flash.")
        return

    try:
        client = ssh_connect(ip)
    except (socket.timeout, paramiko.SSHException) as e:
        print("❌ Błąd SSH:", e)
        return

    mkdir_remote(client, "C:/Compiled/Firmware")
    sftp_upload(client, fw_src, fw_dst)
    ports = list_com_ports(client)
    if not ports:
        print("❌ Nie znaleziono portów COM.")
        client.close()
        return

    chosen = None
    for p in ports:
        chip = detect_chip(client, p, exp_chip)
        if not chip:
            continue
        if exp_chip and chip != exp_chip:
            continue
        print(f"🔍 Port {p} → {chip}")
        chosen = (p, chip)
        break

    if not chosen:
        print("❌ Nie znaleziono oczekiwanego urządzenia.")
        client.close()
        return
    port, chip = chosen

    print("Rozpoczynam kasowanie flash (erase_flash)...")
    erc = run_cmd_live(client, f'python -m esptool --chip {chip} --port {port} --baud 921600 erase_flash')
    print(f"ERASE zakończone z kodem: {erc}")
    if erc != 0:
        print("⚠️ Błąd erase_flash – przerwanie.")
        client.close()
        return

    print("Rozpoczynam flashowanie (write_flash)...")
    frc = run_cmd_live(client, f'python -m esptool --chip {chip} --port {port} --baud 921600 write_flash 0x00000 "{fw_dst}"')
    print(f"FLASH zakończone z kodem: {frc}")
    if frc == 0:
        print("✅ Flashowanie zakończone sukcesem!")
    else:
        print("⚠️ Flashowanie nie powiodło się.")

    client.close()

env.AddPostAction("buildprog", after_build)
