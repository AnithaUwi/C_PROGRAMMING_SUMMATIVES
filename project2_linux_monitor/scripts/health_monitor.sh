#!/usr/bin/env bash

set -u

CONFIG_FILE="config.env"
LOG_FILE="logs/monitor.log"
PID_FILE="logs/monitor.pid"
DEFAULT_CPU_THRESHOLD=85
DEFAULT_MEM_THRESHOLD=85
DEFAULT_DISK_THRESHOLD=90
DEFAULT_INTERVAL=60

cpu_threshold=$DEFAULT_CPU_THRESHOLD
mem_threshold=$DEFAULT_MEM_THRESHOLD
disk_threshold=$DEFAULT_DISK_THRESHOLD
monitor_interval=$DEFAULT_INTERVAL

require_commands() {
  local missing=0
  local cmd
  for cmd in top free df ps awk grep sed date tail wc kill; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
      echo "Error: Required command '$cmd' is missing."
      missing=1
    fi
  done

  if [ "$missing" -eq 1 ]; then
    echo "Install missing commands and run again."
    exit 1
  fi
}

ensure_paths() {
  mkdir -p "logs"
  if [ ! -f "$LOG_FILE" ]; then
    : > "$LOG_FILE"
  fi
}

log_msg() {
  local level="$1"
  local message="$2"
  local timestamp
  timestamp="$(date '+%Y-%m-%d %H:%M:%S')"
  printf '[%s] [%s] %s\n' "$timestamp" "$level" "$message" | tee -a "$LOG_FILE" >/dev/null
}

save_config() {
  cat > "$CONFIG_FILE" <<EOF
CPU_THRESHOLD=$cpu_threshold
MEM_THRESHOLD=$mem_threshold
DISK_THRESHOLD=$disk_threshold
INTERVAL=$monitor_interval
EOF
  log_msg "INFO" "Configuration saved to $CONFIG_FILE"
}

load_config() {
  if [ ! -f "$CONFIG_FILE" ]; then
    save_config
    return
  fi

  # shellcheck disable=SC1090
  . "$CONFIG_FILE"

  cpu_threshold="${CPU_THRESHOLD:-$DEFAULT_CPU_THRESHOLD}"
  mem_threshold="${MEM_THRESHOLD:-$DEFAULT_MEM_THRESHOLD}"
  disk_threshold="${DISK_THRESHOLD:-$DEFAULT_DISK_THRESHOLD}"
  monitor_interval="${INTERVAL:-$DEFAULT_INTERVAL}"

  if ! is_percent "$cpu_threshold"; then
    cpu_threshold=$DEFAULT_CPU_THRESHOLD
  fi
  if ! is_percent "$mem_threshold"; then
    mem_threshold=$DEFAULT_MEM_THRESHOLD
  fi
  if ! is_percent "$disk_threshold"; then
    disk_threshold=$DEFAULT_DISK_THRESHOLD
  fi
  if ! is_positive_int "$monitor_interval"; then
    monitor_interval=$DEFAULT_INTERVAL
  fi
}

is_positive_int() {
  case "$1" in
    ''|*[!0-9]*) return 1 ;;
    *)
      if [ "$1" -le 0 ]; then
        return 1
      fi
      return 0
      ;;
  esac
}

is_percent() {
  if ! is_positive_int "$1"; then
    return 1
  fi

  if [ "$1" -gt 100 ]; then
    return 1
  fi

  return 0
}

read_positive_int() {
  local prompt="$1"
  local value
  while true; do
    read -r -p "$prompt" value
    if is_positive_int "$value"; then
      printf '%s' "$value"
      return 0
    fi
    echo "Invalid input. Enter a positive integer."
  done
}

read_percent() {
  local prompt="$1"
  local value
  while true; do
    read -r -p "$prompt" value
    if is_percent "$value"; then
      printf '%s' "$value"
      return 0
    fi
    echo "Invalid input. Enter an integer from 1 to 100."
  done
}

get_cpu_usage() {
  local idle
  idle="$(top -bn1 | awk '/Cpu\(s\)|%Cpu\(s\)/ {for(i=1;i<=NF;i++){if($i ~ /id,|id/){gsub(",","",$(i-1)); print $(i-1); exit}}}')"
  if [ -z "$idle" ]; then
    idle="0"
  fi
  awk -v idle="$idle" 'BEGIN {printf "%.2f", 100 - idle}'
}

get_mem_usage() {
  free | awk '/Mem:/ {printf "%.2f", ($3/$2)*100}'
}

get_disk_usage() {
  df -P / | awk 'NR==2 {gsub("%","",$5); printf "%.2f", $5}'
}

get_active_process_count() {
  ps -e --no-headers | wc -l | awk '{print $1}'
}

show_health_snapshot() {
  local cpu mem disk processes
  cpu="$(get_cpu_usage)"
  mem="$(get_mem_usage)"
  disk="$(get_disk_usage)"
  processes="$(get_active_process_count)"

  echo "----------------------------------------"
  echo "Current System Health"
  echo "CPU Usage      : ${cpu}%"
  echo "Memory Usage   : ${mem}%"
  echo "Disk Usage     : ${disk}%"
  echo "Active Processes: ${processes}"
  echo "Thresholds     : CPU ${cpu_threshold}% | MEM ${mem_threshold}% | DISK ${disk_threshold}%"
  echo "Interval       : ${monitor_interval}s"
  echo "----------------------------------------"

  log_msg "INFO" "Snapshot CPU=${cpu}% MEM=${mem}% DISK=${disk}% PROC=${processes}"
}

check_thresholds_and_alert() {
  local cpu mem disk
  cpu="$(get_cpu_usage)"
  mem="$(get_mem_usage)"
  disk="$(get_disk_usage)"

  local cpu_int mem_int disk_int
  cpu_int="$(printf '%.0f' "$cpu")"
  mem_int="$(printf '%.0f' "$mem")"
  disk_int="$(printf '%.0f' "$disk")"

  if [ "$cpu_int" -ge "$cpu_threshold" ]; then
    echo "ALERT: CPU usage high (${cpu}%)"
    log_msg "WARN" "CPU threshold exceeded (${cpu}% >= ${cpu_threshold}%)"
  fi

  if [ "$mem_int" -ge "$mem_threshold" ]; then
    echo "ALERT: Memory usage high (${mem}%)"
    log_msg "WARN" "Memory threshold exceeded (${mem}% >= ${mem_threshold}%)"
  fi

  if [ "$disk_int" -ge "$disk_threshold" ]; then
    echo "ALERT: Disk usage high (${disk}%)"
    log_msg "WARN" "Disk threshold exceeded (${disk}% >= ${disk_threshold}%)"
  fi
}

monitor_worker() {
  log_msg "INFO" "Continuous monitoring worker started (PID $$)"

  while true; do
    show_health_snapshot
    check_thresholds_and_alert
    sleep "$monitor_interval"
  done
}

is_monitor_running() {
  if [ ! -f "$PID_FILE" ]; then
    return 1
  fi

  local pid
  pid="$(cat "$PID_FILE" 2>/dev/null || true)"
  if ! is_positive_int "$pid"; then
    rm -f "$PID_FILE"
    return 1
  fi

  if kill -0 "$pid" >/dev/null 2>&1; then
    return 0
  fi

  rm -f "$PID_FILE"
  return 1
}

start_monitoring_loop() {
  if is_monitor_running; then
    echo "Monitoring is already running."
    return
  fi

  "$0" --worker >/dev/null 2>&1 &
  echo "$!" > "$PID_FILE"
  log_msg "INFO" "Continuous monitoring started (PID $!)"
  echo "Monitoring started in background (PID $!)."
}

stop_monitoring() {
  if ! is_monitor_running; then
    echo "Monitoring is not currently running."
    return
  fi

  local pid
  pid="$(cat "$PID_FILE")"
  if kill "$pid" >/dev/null 2>&1; then
    rm -f "$PID_FILE"
    log_msg "INFO" "Continuous monitoring stopped (PID $pid)"
    echo "Monitoring stopped."
  else
    rm -f "$PID_FILE"
    echo "Monitoring process could not be stopped cleanly; stale PID removed."
  fi
}

configure_thresholds() {
  echo "Configure Monitoring Thresholds"
  cpu_threshold="$(read_percent "Enter CPU threshold (%) [1-100]: ")"
  mem_threshold="$(read_percent "Enter Memory threshold (%) [1-100]: ")"
  disk_threshold="$(read_percent "Enter Disk threshold (%) [1-100]: ")"
  monitor_interval="$(read_positive_int "Enter monitoring interval (seconds): ")"
  save_config
  echo "Configuration updated."
}

view_logs() {
  if [ ! -s "$LOG_FILE" ]; then
    echo "Log file is empty."
    return
  fi

  echo "----- Last 30 log lines -----"
  tail -n 30 "$LOG_FILE"
}

clear_logs() {
  : > "$LOG_FILE"
  log_msg "INFO" "Logs cleared"
  echo "Logs cleared."
}

show_menu() {
  echo
  echo "===== Linux Server Health Monitor ====="
  echo "1. Display current system health"
  echo "2. Configure thresholds and interval"
  echo "3. View activity logs"
  echo "4. Clear logs"
  echo "5. Start continuous monitoring"
  echo "6. Stop continuous monitoring"
  echo "7. Exit"
}

main() {
  if [ "${1:-}" = "--worker" ]; then
    require_commands
    ensure_paths
    load_config
    monitor_worker
    exit 0
  fi

  require_commands
  ensure_paths
  load_config

  while true; do
    show_menu
    read -r -p "Choose an option [1-7]: " choice

    case "$choice" in
      1)
        show_health_snapshot
        check_thresholds_and_alert
        ;;
      2)
        configure_thresholds
        ;;
      3)
        view_logs
        ;;
      4)
        clear_logs
        ;;
      5)
        start_monitoring_loop
        ;;
      6)
        stop_monitoring
        ;;
      7)
        stop_monitoring
        log_msg "INFO" "Program exited"
        echo "Goodbye."
        break
        ;;
      *)
        echo "Invalid menu choice. Please enter 1-7."
        ;;
    esac
  done
}

main
