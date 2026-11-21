#!/usr/bin/env python3
"""
ASYLYM MONOLITH // Ultra-Low CPU Telemetry Daemon (Windows & macOS)
CPU overhead: ~0.0-0.1% (no ping.exe subprocess, lazy process scanning, single-instance lock).

Usage:
  python telemetry_daemon.py           -> interactive console mode
  python telemetry_daemon.py --demo    -> simulated telemetry & alerts
  python telemetry_daemon.py --silent  -> silent background service (logs to ~/.asylym_monolith)
"""

import argparse
import json
import logging
from logging.handlers import RotatingFileHandler
import math
import os
from pathlib import Path
import platform
import socket
import sys
import threading
import time

try:
    import psutil
except ImportError:
    print("[ERROR] Package 'psutil' is not installed. Run install_win.bat or install_mac.command first.")
    sys.exit(1)

APP_DIR = Path.home() / ".asylym_monolith"
CONFIG_FILE = APP_DIR / "config.json"
LOG_FILE = APP_DIR / "daemon.log"

DEFAULT_PORT = 4210
DEFAULT_HOST = "aether-monolith.local"
LOCK_PORT = 42109

IGNORE_PROCS = {
    "system idle process",
    "idle",
    "system",
    "registry",
    "kernel_task",
    "windowserver",
    "python.exe",
    "pythonw.exe",
    "python3",
    "python",
    "",
}


def setup_logging(silent: bool) -> logging.Logger:
    APP_DIR.mkdir(parents=True, exist_ok=True)
    logger = logging.getLogger("asylym")
    logger.setLevel(logging.INFO)
    logger.handlers.clear()

    file_handler = RotatingFileHandler(LOG_FILE, maxBytes=512_000, backupCount=2, encoding="utf-8")
    file_handler.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
    logger.addHandler(file_handler)

    if not silent:
        console_handler = logging.StreamHandler(sys.stdout)
        console_handler.setFormatter(logging.Formatter("%(message)s"))
        logger.addHandler(console_handler)

    return logger


def acquire_single_instance_lock():
    """Ensures only one instance of the daemon runs at a time."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind(("127.0.0.1", LOCK_PORT))
        sock.listen(1)
        return sock
    except OSError:
        sock.close()
        return None


def lower_process_priority():
    try:
        proc = psutil.Process()
        if os.name == "nt":
            proc.nice(psutil.BELOW_NORMAL_PRIORITY_CLASS)
        else:
            proc.nice(10)
    except Exception:
        pass


def load_saved_ip() -> str:
    if CONFIG_FILE.exists():
        try:
            data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
            return str(data.get("ip", "")).strip()
        except Exception:
            pass
    return ""


def get_os_tag() -> str:
    sys_name = platform.system().lower()
    if "darwin" in sys_name:
        return "MAC"
    if "windows" in sys_name:
        return "WIN"
    return "LNX"


class NetworkHelper:
    """Background thread: lightweight TCP latency check + periodic UDP target discovery."""

    def __init__(self, explicit_ip: str = "", ping_interval: float = 5.0):
        self.explicit_ip = explicit_ip
        self.ping_interval = ping_interval
        self.ping_ms = 0
        self.net_alert = False
        self.targets = self._discover_targets(use_mdns=False)
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self._thread.start()

    def stop(self):
        self._stop.set()

    def _discover_targets(self, use_mdns: bool = True) -> list:
        if self.explicit_ip:
            return [self.explicit_ip]
        saved = load_saved_ip()
        if saved:
            return [saved]

        targets = set()
        if use_mdns:
            try:
                esp_ip = socket.gethostbyname(DEFAULT_HOST)
                if esp_ip:
                    targets.add(esp_ip)
            except OSError:
                pass

        try:
            for _, snics in psutil.net_if_addrs().items():
                for snic in snics:
                    if snic.family == socket.AF_INET:
                        ip = snic.address
                        if ip and not ip.startswith("127.") and not ip.startswith("169.254."):
                            parts = ip.split(".")
                            if len(parts) == 4:
                                targets.add(f"{parts[0]}.{parts[1]}.{parts[2]}.255")
        except Exception:
            pass

        if os.name == "nt" or not targets:
            targets.add("255.255.255.255")

        return list(targets)

    def _measure_ping(self):
        t0 = time.perf_counter()
        try:
            with socket.create_connection(("1.1.1.1", 53), timeout=1.0):
                self.ping_ms = int(round((time.perf_counter() - t0) * 1000))
                self.net_alert = self.ping_ms > 150
        except OSError:
            self.ping_ms = 999
            self.net_alert = True

    def _run(self):
        last_target_refresh = 0.0
        while not self._stop.is_set():
            now = time.monotonic()
            if now - last_target_refresh >= 60.0:
                self.targets = self._discover_targets(use_mdns=True)
                last_target_refresh = now
            self._measure_ping()
            self._stop.wait(self.ping_interval)


def get_top_process_lazy(cpu_load: int, num_cores: int) -> str:
    """Scans running processes only when overall CPU load >= 15%."""
    if cpu_load < 15:
        return "IDLE"

    best_name = "IDLE"
    best_cpu = 0.0
    for proc in psutil.process_iter(["name", "cpu_percent"]):
        try:
            name = (proc.info.get("name") or "").strip()
            if name.lower() in IGNORE_PROCS:
                continue
            cpu = (proc.info.get("cpu_percent") or 0.0) / num_cores
            if cpu > best_cpu:
                best_cpu = cpu
                best_name = name
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            continue

    return best_name.replace(".exe", "")[:16] if best_cpu > 1.5 else "IDLE"


def get_cpu_temp() -> int:
    try:
        if hasattr(psutil, "sensors_temperatures"):
            temps = psutil.sensors_temperatures() or {}
            for key in ("coretemp", "k10temp", "cpu_thermal", "acpitz"):
                if key in temps and temps[key]:
                    return int(round(temps[key][0].current))
    except Exception:
        pass
    return 0


def collect_demo_payload(t: float) -> dict:
    phase = t * 0.45
    cpu = max(5, min(98, int(35 + 32 * math.sin(phase) + 18 * math.sin(phase * 2.3))))
    ram = int(58 + 20 * math.sin(phase * 0.35))
    swap_gb = round(max(0.0, 2.2 + 2.4 * math.sin(phase * 0.22)), 1)
    disk_mbs = round(max(0.0, 45.0 + 70.0 * (math.sin(phase * 1.4) ** 2)), 1)
    net_kbs = round(max(0.0, 600.0 + 1800.0 * (math.cos(phase * 0.9) ** 2)), 1)
    temp = int(48 + cpu * 0.38)
    ping = int(18 + 12 * abs(math.sin(phase * 0.7)))

    hogs = ["Unity", "Rider", "Blender", "Chrome", "Docker"]
    hog = hogs[int(t / 4) % len(hogs)]

    now = time.localtime()
    return {
        "cpu": cpu,
        "ram": ram,
        "swap_gb": swap_gb,
        "swap_alert": swap_gb > 4.0,
        "disk_mbs": disk_mbs,
        "net_kbs": net_kbs,
        "temp": temp,
        "therm_alert": temp >= 82,
        "ping": ping,
        "net_alert": False,
        "hog": hog,
        "os": "DEMO",
        "time": time.strftime("%H:%M:%S", now),
        "date": time.strftime("%d %b", now).upper(),
    }


def main():
    parser = argparse.ArgumentParser(description="ASYLYM MONOLITH Telemetry Daemon")
    parser.add_argument("--ip", default="", help="ESP32 IP address (default: auto-discover via mDNS / broadcast)")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="UDP port (default: 4210)")
    parser.add_argument("--interval", type=float, default=1.0, help="Send interval in seconds (default: 1.0)")
    parser.add_argument("--demo", action="store_true", help="Run with simulated telemetry and alerts")
    parser.add_argument("--silent", action="store_true", help="Silent background mode (log to file only)")
    args = parser.parse_args()

    logger = setup_logging(args.silent)

    lock_sock = acquire_single_instance_lock()
    if lock_sock is None:
        if not args.silent:
            print("[INFO] Daemon is already running in the background. Stop the service via the installer (option 4) before starting manually.")
        else:
            logger.info("Duplicate instance prevented; exiting.")
        sys.exit(0)

    lower_process_priority()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    os_tag = get_os_tag()
    num_cores = psutil.cpu_count() or 1
    net_helper = NetworkHelper(explicit_ip=args.ip)
    net_helper.start()

    if not args.demo:
        psutil.cpu_percent(interval=None)
        try:
            for _ in psutil.process_iter(["cpu_percent"]):
                pass
        except Exception:
            pass

    prev_disk = psutil.disk_io_counters()
    prev_net = psutil.net_io_counters()
    prev_t = time.monotonic()
    top_hog = "IDLE"
    last_hog_t = 0.0

    mode_name = "DEMO" if args.demo else ("SILENT SERVICE" if args.silent else "INTERACTIVE")
    logger.info(f"ASYLYM MONOLITH Telemetry Daemon [{mode_name}] -> {net_helper.targets}:{args.port}")

    t_start = time.monotonic()
    try:
        while True:
            now_mono = time.monotonic()

            if args.demo:
                payload = collect_demo_payload(now_mono - t_start)
            else:
                dt = max(0.2, now_mono - prev_t)
                prev_t = now_mono

                cpu = int(round(psutil.cpu_percent(interval=None)))
                vm = psutil.virtual_memory()
                sw = psutil.swap_memory()
                swap_gb = round(sw.used / (1024 ** 3), 1)

                cur_disk = psutil.disk_io_counters()
                disk_mbs = 0.0
                if cur_disk and prev_disk:
                    d_bytes = (cur_disk.read_bytes - prev_disk.read_bytes) + (cur_disk.write_bytes - prev_disk.write_bytes)
                    disk_mbs = round(max(0.0, d_bytes / dt / (1024 ** 2)), 1)
                prev_disk = cur_disk

                cur_net = psutil.net_io_counters()
                net_kbs = 0.0
                if cur_net and prev_net:
                    n_bytes = (cur_net.bytes_sent - prev_net.bytes_sent) + (cur_net.bytes_recv - prev_net.bytes_recv)
                    net_kbs = round(max(0.0, n_bytes / dt / 1024.0), 1)
                prev_net = cur_net

                # Scan process table at most once every 8 seconds (and only when CPU >= 15%)
                if now_mono - last_hog_t >= 8.0:
                    top_hog = get_top_process_lazy(cpu, num_cores)
                    last_hog_t = now_mono

                temp = get_cpu_temp()
                now_loc = time.localtime()

                payload = {
                    "cpu": cpu,
                    "ram": int(round(vm.percent)),
                    "swap_gb": swap_gb,
                    "swap_alert": (swap_gb >= 4.0) or (sw.percent >= 80.0 and swap_gb >= 1.5),
                    "disk_mbs": disk_mbs,
                    "net_kbs": net_kbs,
                    "temp": temp,
                    "therm_alert": (temp >= 85) or (cpu >= 95),
                    "ping": net_helper.ping_ms,
                    "net_alert": net_helper.net_alert,
                    "hog": top_hog,
                    "os": os_tag,
                    "time": time.strftime("%H:%M:%S", now_loc),
                    "date": time.strftime("%d %b", now_loc).upper(),
                }

            raw = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            for target_ip in list(net_helper.targets):
                try:
                    sock.sendto(raw, (target_ip, args.port))
                except OSError:
                    pass

            if not args.silent:
                sys.stdout.write(
                    f"\r[{payload['time']}] -> {net_helper.targets[0]} | "
                    f"CPU:{payload['cpu']:3d}% RAM:{payload['ram']:3d}% "
                    f"SWP:{payload['swap_gb']:4.1f}G DSK:{payload['disk_mbs']:5.1f}M "
                    f"NET:{payload['net_kbs']:6.1f}K PNG:{payload['ping']:3d}ms HOG:{payload['hog']:<12s}"
                )
                sys.stdout.flush()

            time.sleep(max(0.5, args.interval))
    except KeyboardInterrupt:
        if not args.silent:
            print("\nStopped.")
    finally:
        net_helper.stop()
        sock.close()
        lock_sock.close()


if __name__ == "__main__":
    main()