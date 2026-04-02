Project 2: Linux Server Health Monitoring and Alert Automation Script

Main script:
  scripts/health_monitor.sh

Features:
- Monitors CPU, memory, disk usage, and active process count
- Configurable thresholds and monitoring interval
- Alerts when thresholds are exceeded
- Timestamped logging in logs/monitor.log
- Interactive menu for monitor/config/log actions
- True start and stop monitoring controls from the menu (background worker PID)
- Input validation and command availability checks

Run on Linux:
  chmod +x scripts/health_monitor.sh
  ./scripts/health_monitor.sh

Optional test on Windows (if Git Bash/WSL is available):
  bash scripts/health_monitor.sh

Default config file generated at runtime:
  config.env

Log file:
  logs/monitor.log

Quick demo flow:
1) Choose option 1 to display a snapshot
2) Choose option 2 to set thresholds (for demo, set low values like CPU 1)
3) Choose option 5 to start continuous monitoring
4) Wait one interval, then choose option 6 to stop monitoring
5) Choose option 3 to show recent alerts and activity logs
