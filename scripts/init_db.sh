#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "please run as root: sudo ./scripts/init_db.sh" >&2
  exit 1
fi

mysql <<'SQL'
CREATE DATABASE IF NOT EXISTS dist_platform CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER IF NOT EXISTS 'dist_platform'@'localhost' IDENTIFIED BY 'dist_platform';
GRANT ALL PRIVILEGES ON dist_platform.* TO 'dist_platform'@'localhost';
FLUSH PRIVILEGES;
SQL