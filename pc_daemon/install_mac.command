#!/bin/bash

# Ensure Homebrew paths are available for both Apple Silicon and Intel Macs when launched from Finder
export PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

APP_DIR="$HOME/.asylym_monolith"
VENV_DIR="$APP_DIR/venv"
PY_BIN="$VENV_DIR/bin/python"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DAEMON_SRC="$SCRIPT_DIR/telemetry_daemon.py"
DAEMON_DST="$APP_DIR/telemetry_daemon.py"
PLIST_LABEL="com.asylym.monolith.telemetry"
PLIST_PATH="$HOME/Library/LaunchAgents/${PLIST_LABEL}.plist"
GUI_DOMAIN="gui/$(id -u)"

pause_exit() {
  echo
  read -r -p "Press Enter to exit..."
  exit "${1:-1}"
}

find_real_python() {
  # Verify a real Python 3.9+ installation with venv support
  # (skips the non-functional /usr/bin/python3 stub on fresh macOS installs)
  for cand in /opt/homebrew/bin/python3 /usr/local/bin/python3 "$(command -v python3 2>/dev/null)"; do
    if [ -n "$cand" ] && [ -x "$cand" ]; then
      if "$cand" -c "import sys, venv; assert sys.version_info >= (3, 9)" >/dev/null 2>&1; then
        echo "$cand"
        return 0
      fi
    fi
  done
  return 1
}

stop_service() {
  launchctl bootout "$GUI_DOMAIN" "$PLIST_PATH" >/dev/null 2>&1 || \
    launchctl unload -w "$PLIST_PATH" >/dev/null 2>&1 || true
  pkill -f "telemetry_daemon.py" >/dev/null 2>&1 || true
}

start_service() {
  mkdir -p "$HOME/Library/LaunchAgents"
  stop_service
  cat > "$PLIST_PATH" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>${PLIST_LABEL}</string>
  <key>ProgramArguments</key>
  <array>
    <string>${PY_BIN}</string>
    <string>${DAEMON_DST}</string>
    <string>--silent</string>
  </array>
  <key>WorkingDirectory</key>
  <string>${APP_DIR}</string>
  <key>ProcessType</key>
  <string>Background</string>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>StandardOutPath</key>
  <string>${APP_DIR}/launchd.out.log</string>
  <key>StandardErrorPath</key>
  <string>${APP_DIR}/launchd.err.log</string>
</dict>
</plist>
EOF

  if ! launchctl bootstrap "$GUI_DOMAIN" "$PLIST_PATH" >/dev/null 2>&1; then
    launchctl load -w "$PLIST_PATH" >/dev/null 2>&1 || {
      echo "[ERROR] Failed to register LaunchAgent."
      return 1
    }
  fi
  launchctl kickstart -k "${GUI_DOMAIN}/${PLIST_LABEL}" >/dev/null 2>&1 || true
  return 0
}

service_status() {
  if pgrep -f "$DAEMON_DST" >/dev/null 2>&1; then
    echo "RUNNING (background)"
  else
    echo "STOPPED"
  fi
}

echo "========================================================"
echo "  ASYLYM MONOLITH // TELEMETRY INSTALLER (macOS)"
echo "========================================================"
echo

mkdir -p "$APP_DIR"

if [ -f "$DAEMON_SRC" ]; then
  cp -f "$DAEMON_SRC" "$DAEMON_DST"
  chmod +x "$DAEMON_DST"
elif [ ! -f "$DAEMON_DST" ]; then
  echo "[ERROR] Please place telemetry_daemon.py in the same folder as install_mac.command."
  pause_exit 1
fi

# 1. Locate or auto-install Python 3.9+
SYS_PY="$(find_real_python || true)"
if [ -z "$SYS_PY" ]; then
  echo "[1/3] Working Python 3.9+ installation not found."
  if ! command -v brew >/dev/null 2>&1; then
    echo "      Installing Homebrew (macOS password may be required)..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" || pause_exit 1
    export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
  fi
  echo "      Installing python3 via Homebrew..."
  brew install python || pause_exit 1
  SYS_PY="$(find_real_python || true)"
  if [ -z "$SYS_PY" ]; then
    echo "[ERROR] Python was installed but could not be found in PATH. Please re-run this script."
    pause_exit 1
  fi
fi
echo "[1/3] Python ready: $("$SYS_PY" --version) ($SYS_PY)"

# 2. Create isolated virtual environment
if [ ! -x "$PY_BIN" ]; then
  echo "[2/3] Creating virtual environment in $VENV_DIR..."
  "$SYS_PY" -m venv "$VENV_DIR" || pause_exit 1
else
  echo "[2/3] Virtual environment already exists."
fi

# 3. Install dependencies
echo "[3/3] Checking and installing psutil..."
if ! "$PY_BIN" -c "import psutil" >/dev/null 2>&1; then
  "$PY_BIN" -m pip install --upgrade pip --quiet || true
  "$PY_BIN" -m pip install psutil --quiet || pause_exit 1
fi
echo "[OK] Environment is ready."
echo

while true; do
  CUR_IP="auto-discovery (aether-monolith.local)"
  if [ -f "$APP_DIR/config.json" ]; then
    CUR_IP="$("$PY_BIN" -c "import json; print(json.load(open('$APP_DIR/config.json')).get('ip','auto-discovery'))" 2>/dev/null || echo "auto-discovery")"
  fi

  echo "--------------------------------------------------------"
  echo "  Service: $(service_status) | Target: $CUR_IP"
  echo "--------------------------------------------------------"
  echo "  [1] Run manually (interactive console)"
  echo "  [2] Run in DEMO mode (simulated load and alerts)"
  echo "  [3] Install and start SILENT background service (LaunchAgent)"
  echo "  [4] Stop and remove background service"
  echo "  [5] Set custom ESP32 IP (leave empty for auto-discovery)"
  echo "  [0] Exit"
  echo "--------------------------------------------------------"
  read -r -p "Enter your choice: " CHOICE

  case "$CHOICE" in
    1)
      echo
      echo "Starting in interactive mode. Press Ctrl+C to stop."
      "$PY_BIN" "$DAEMON_DST" || true
      echo
      ;;
    2)
      echo
      echo "Starting in DEMO mode. Press Ctrl+C to stop."
      "$PY_BIN" "$DAEMON_DST" --demo || true
      echo
      ;;
    3)
      echo
      if start_service; then
        echo "[OK] Silent service installed and started (auto-starts at macOS login)."
        echo "     Log file: $APP_DIR/daemon.log"
      fi
      echo
      ;;
    4)
      stop_service
      rm -f "$PLIST_PATH"
      echo
      echo "[OK] Background service stopped and removed."
      echo
      ;;
    5)
      echo
      read -r -p "Enter ESP32 IP address (or press Enter for auto-discovery): " NEW_IP
      if [ -z "$NEW_IP" ]; then
        rm -f "$APP_DIR/config.json"
        echo "[OK] Auto-discovery enabled (aether-monolith.local / subnet broadcast)."
      else
        printf '{"ip": "%s"}\n' "$NEW_IP" > "$APP_DIR/config.json"
        echo "[OK] Custom IP saved: $NEW_IP"
      fi
      if [ -f "$PLIST_PATH" ]; then
        start_service >/dev/null 2>&1 && echo "[OK] Background service restarted with updated target."
      fi
      echo
      ;;
    0)
      exit 0
      ;;
  esac
done