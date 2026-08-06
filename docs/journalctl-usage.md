# Filtering and Viewing holonightd Logs with journalctl

This document provides a reference guide for querying, filtering, and streaming `holonightd` log output using Linux `journalctl`.

---

## 1. Systemd Service Logging

When `holonightd` is running as a systemd service:

### View All Service Logs
```bash
journalctl -u holonightd.service
```

### Follow Log Output (Live Tail)
Stream log entries in real-time as they are written:
```bash
journalctl -u holonightd.service -f
```

### Limit Number of Entries
Display only the most recent $N$ entries (e.g., last 100 lines):
```bash
journalctl -u holonightd.service -n 100
```

### User-Level Service Instance
If `holonightd` is managed as a user service (`systemctl --user`):
```bash
journalctl --user -u holonightd.service
```

---

## 2. Process & Binary Name Filtering

If `holonightd` is executed outside of systemd service management (e.g., directly from the CLI or via custom runners):

### Filter by Process Name (`_COMM`)
```bash
journalctl _COMM=holonightd
```

### Filter by Executable Path
```bash
journalctl /usr/local/bin/holonightd
```

### Filter by Syslog Identifier
```bash
journalctl -t holonightd
```

---

## 3. Time Window Filtering

### Current Boot Logs
Show logs recorded since the latest system boot:
```bash
journalctl -u holonightd.service -b
```

### Relative Time Ranges
Query logs within relative timeframes:
```bash
# Logs from the last hour
journalctl -u holonightd.service --since "1 hour ago"

# Logs from today
journalctl -u holonightd.service --since "today"
```

### Absolute Time Ranges
Filter between specific timestamps:
```bash
journalctl -u holonightd.service --since "2026-07-31 00:00:00" --until "2026-07-31 12:00:00"
```

---

## 4. Severity & Priority Filtering (`-p`)

Filter logs using systemd syslog priority levels:
- `0`: Emergency (`emerg`)
- `1`: Alert (`alert`)
- `2`: Critical (`crit`)
- `3`: Error (`err`)
- `4`: Warning (`warning`)
- `5`: Notice (`notice`)
- `6`: Informational (`info`)
- `7`: Debug (`debug`)

### Error Level and Higher
```bash
journalctl -u holonightd.service -p err
```

### Priority Ranges (e.g., Warning through Error)
```bash
journalctl -u holonightd.service -p warning..err
```

### Debug Level Output
```bash
journalctl -u holonightd.service -p debug
```

---

## 5. Pattern Searching & Formatting

### Regex Keyword Search (`-g` / `--grep`)
Search for specific pattern matches in log messages:
```bash
journalctl -u holonightd.service -g "HealthCheckJob|FilesystemScanner"
```

### Structured JSON Output
Format output as formatted JSON for external log ingestion or automated parsing:
```bash
journalctl -u holonightd.service -o json-pretty
```

### Disable Pager
Output directly to standard stdout (useful for piping into `grep`, `awk`, or saved log files):
```bash
journalctl -u holonightd.service --no-pager
```
