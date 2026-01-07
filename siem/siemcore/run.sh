#!/usr/bin/env bash
set -euo pipefail

# --- Paths ---
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIEM_DIR="$ROOT_DIR/siemcore"
DB_DIR="$ROOT_DIR"                    # у тебя db_server лежит в SUBD
DB_BIN="${DB_BIN:-$DB_DIR/build/db_server}"

# --- Config ---
DB_PORT="${DB_PORT:-8080}"
SIEM_PORT="${SIEM_PORT:-8090}"

DB_SCHEMA="${DB_SCHEMA:-$DB_DIR/schema.json}"
DB_DATA_ROOT="${DB_DATA_ROOT:-$DB_DIR/data}"

export SIEM_USER="${SIEM_USER:-admin}"
export SIEM_PASS="${SIEM_PASS:-admin}"
export CPPDB_ADDR="${CPPDB_ADDR:-127.0.0.1:$DB_PORT}"
export SIEM_HTTP_ADDR="${SIEM_HTTP_ADDR:-127.0.0.1:$SIEM_PORT}"
export SIEM_TLS_CERT="${SIEM_TLS_CERT:-$SIEM_DIR/cert/cert.pem}"
export SIEM_TLS_KEY="${SIEM_TLS_KEY:-$SIEM_DIR/cert/key.pem}"
export SIEM_ENABLE_HSTS="${SIEM_ENABLE_HSTS:-1}"

pids=()

need() { command -v "$1" >/dev/null 2>&1 || { echo "Need '$1'"; exit 1; }; }
need ss
need go

kill_port() {
  local port="$1"
  if ss -ltn "( sport = :$port )" | tail -n +2 | grep -q .; then
    echo "[run.sh] port $port busy -> killing owner"
    if command -v fuser >/dev/null 2>&1; then
      sudo fuser -k "${port}/tcp" >/dev/null 2>&1 || true
    else
      # fallback: kill by pid extracted from ss
      local pid
      pid="$(ss -ltnp | awk -v p=":$port" '$4 ~ p {print $NF}' | head -n1 | sed -n 's/.*pid=\([0-9]\+\).*/\1/p')"
      [[ -n "${pid:-}" ]] && kill "$pid" 2>/dev/null || true
    fi
  fi
}

cleanup() {
  echo
  echo "[run.sh] stopping..."
  for pid in "${pids[@]:-}"; do
    kill "$pid" 2>/dev/null || true
  done
  # подчистим порты на всякий
  if command -v fuser >/dev/null 2>&1; then
    sudo fuser -k "$DB_PORT/tcp"  >/dev/null 2>&1 || true
    sudo fuser -k "$SIEM_PORT/tcp" >/dev/null 2>&1 || true
  fi
}
trap cleanup INT TERM EXIT

echo "[run.sh] freeing ports..."
kill_port "$DB_PORT"
kill_port "$SIEM_PORT"

echo "[run.sh] build siemcore..."
cd "$SIEM_DIR"
go build -o siemcore ./cmd/siemcore

echo "[run.sh] start db_server: $DB_BIN --port $DB_PORT --schema $DB_SCHEMA --data-root $DB_DATA_ROOT"
cd "$DB_DIR"
"$DB_BIN" --port "$DB_PORT" --schema "$DB_SCHEMA" --data-root "$DB_DATA_ROOT" &
pids+=("$!")
sleep 0.2

echo "[run.sh] start siemcore: $SIEM_DIR/siemcore (HTTP=$SIEM_HTTP_ADDR, CPPDB=$CPPDB_ADDR)"
cd "$SIEM_DIR"
./siemcore &
pids+=("$!")

echo
echo "[run.sh] OK"
echo "UI: https://127.0.0.1:${SIEM_PORT}/"
echo "Press Ctrl+C to stop."
wait
