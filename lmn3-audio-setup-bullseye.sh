#!/usr/bin/env bash
set -euo pipefail

# =========================
# LMN-3 Audio Setup (Bullseye)
# Raspberry Pi 4 "firmware-ish" audio configuration:
#   ALSA -> JACK -> LMN-3
#
# Scope: audio only (no display, Wi-Fi, Bluetooth, CPU governor, etc.)
# Safe to re-run.
# =========================

log() { printf "\n[LMN3-AUDIO] %s\n" "$*"; }
warn() { printf "\n[LMN3-AUDIO] WARNING: %s\n" "$*" >&2; }
die() { printf "\n[LMN3-AUDIO] ERROR: %s\n" "$*" >&2; exit 1; }

need_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    die "Run as root: sudo $0"
  fi
}

# --------------------------
# Config (override via env)
# --------------------------
# ALSA device JACK will open directly (examples: hw:0, hw:1, hw:USB)
AUDIO_DEV="${AUDIO_DEV:-hw:0}"

# JACK params (stable defaults for Pi 4)
JACK_RATE="${JACK_RATE:-48000}"
JACK_PERIOD="${JACK_PERIOD:-256}"
JACK_NPERIODS="${JACK_NPERIODS:-3}"
JACK_RT_PRIO="${JACK_RT_PRIO:-95}"

# LMN-3 launch command (set this to your real command/path)
LMN3_CMD="${LMN3_CMD:-/usr/local/bin/lmn3}"

# User that runs jackd + LMN-3 (defaults to the sudo-invoking user)
AUDIO_USER="${AUDIO_USER:-${SUDO_USER:-pi}}"

# 1 = purge pulseaudio packages (recommended for dedicated audio boxes)
PURGE_PULSEAUDIO="${PURGE_PULSEAUDIO:-1}"
# --------------------------

assert_user_exists() {
  getent passwd "${AUDIO_USER}" >/dev/null || die "User '${AUDIO_USER}' does not exist. Set AUDIO_USER=... and rerun."
}

assert_bullseye() {
  if [[ -f /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    if [[ "${VERSION_CODENAME:-}" != "bullseye" ]]; then
      warn "Script intended for Bullseye; detected: ${VERSION_CODENAME:-unknown}"
    fi
  fi
}

health_checks() {
  log "Quick health checks"
  if command -v vcgencmd >/dev/null 2>&1; then
    local t
    t="$(vcgencmd get_throttled || true)"
    log "vcgencmd get_throttled: ${t} (ideal: throttled=0x0)"
  fi
  log "dpkg audit (should be empty):"
  dpkg --audit || true
}

install_packages() {
  log "Installing required packages"
  export DEBIAN_FRONTEND=noninteractive

  apt-get update -y

  # Preseed jackd2 realtime prompt to avoid interactive hang.
  apt-get install -y --no-install-recommends debconf-utils
  echo "jackd2 jackd/tweak_rt_limits boolean true" | debconf-set-selections || true

  apt-get install -y --no-install-recommends \
    alsa-utils alsa-tools \
    jackd2 \
    rtkit
}

disable_or_purge_pulseaudio() {
  log "Disabling PulseAudio autospawn"
  local home_dir
  home_dir="$(getent passwd "${AUDIO_USER}" | cut -d: -f6)"
  [[ -n "${home_dir}" ]] || die "Could not determine home directory for ${AUDIO_USER}"

  install -d -m 0755 "${home_dir}/.config/pulse"
  printf "autospawn = no\n" > "${home_dir}/.config/pulse/client.conf"
  chown -R "${AUDIO_USER}:${AUDIO_USER}" "${home_dir}/.config/pulse"

  # Best-effort: stop/mask user units if a user systemd instance exists
  sudo -u "${AUDIO_USER}" systemctl --user stop pulseaudio.service pulseaudio.socket 2>/dev/null || true
  sudo -u "${AUDIO_USER}" systemctl --user mask pulseaudio.service pulseaudio.socket 2>/dev/null || true

  if [[ "${PURGE_PULSEAUDIO}" == "1" ]]; then
    log "Purging PulseAudio packages"
    apt-get purge -y pulseaudio pulseaudio-utils 2>/dev/null || true
    apt-get autoremove -y || true
  else
    log "Leaving PulseAudio packages installed (PURGE_PULSEAUDIO=0)"
  fi
}

configure_realtime_limits() {
  log "Configuring realtime privileges for ${AUDIO_USER}"
  getent group audio >/dev/null || groupadd audio
  usermod -aG audio "${AUDIO_USER}"

  cat >/etc/security/limits.d/99-lmn3-audio.conf <<'EOF'
@audio   -  rtprio     95
@audio   -  memlock    unlimited
@audio   -  nice      -10
EOF
}

configure_alsa_for_testing_only() {
  log "Creating minimal /etc/asound.conf for consistent ALSA tests"

  # NOTE:
  # - JACK will open AUDIO_DEV directly. This file mainly makes `aplay` predictable.
  # - We avoid dmix here because it can fight with JACK for device access.
  if [[ -f /etc/asound.conf ]]; then
    cp -n /etc/asound.conf /etc/asound.conf.lmn3.bak || true
    log "Backed up /etc/asound.conf to /etc/asound.conf.lmn3.bak"
  fi

  local ctl_card="0"
  if [[ "${AUDIO_DEV}" =~ ^hw:([^,]+) ]]; then
    ctl_card="${BASH_REMATCH[1]}"
  fi

  cat >/etc/asound.conf <<EOF
pcm.!default {
  type plug
  slave.pcm "${AUDIO_DEV}"
  slave.rate ${JACK_RATE}
}

ctl.!default {
  type hw
  card ${ctl_card}
}
EOF

  systemctl enable alsa-state 2>/dev/null || true
}

create_jack_service() {
  log "Creating systemd service: jackd"

  install -d -m 0755 /var/log/lmn3
  chown "${AUDIO_USER}:audio" /var/log/lmn3 || true

  cat >/etc/systemd/system/jackd.service <<EOF
[Unit]
Description=JACK Audio Daemon (LMN-3)
After=sound.target
Wants=sound.target

[Service]
Type=simple
User=${AUDIO_USER}
Group=audio
LimitRTPRIO=infinity
LimitMEMLOCK=infinity
Nice=-10
Environment=JACK_NO_AUDIO_RESERVATION=1
ExecStart=/usr/bin/jackd -P${JACK_RT_PRIO} -dalsa -d${AUDIO_DEV} -r${JACK_RATE} -p${JACK_PERIOD} -n${JACK_NPERIODS}
Restart=always
RestartSec=1
StandardOutput=append:/var/log/lmn3/jackd.log
StandardError=append:/var/log/lmn3/jackd.log

[Install]
WantedBy=multi-user.target
EOF

  systemctl daemon-reload
  systemctl enable jackd.service
}

create_lmn3_service() {
  log "Creating systemd service: lmn3"

  if [[ ! -e "${LMN3_CMD}" ]]; then
    warn "LMN3_CMD does not exist yet: ${LMN3_CMD} (service will be created anyway)"
  elif [[ ! -x "${LMN3_CMD}" ]]; then
    warn "LMN3_CMD is not executable: ${LMN3_CMD}"
  fi

  cat >/etc/systemd/system/lmn3.service <<EOF
[Unit]
Description=LMN-3
After=jackd.service
Requires=jackd.service

[Service]
Type=simple
User=${AUDIO_USER}
Group=audio
ExecStart=${LMN3_CMD}
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

  systemctl daemon-reload
  systemctl enable lmn3.service
}

final_notes() {
  log "Done. Suggested verification commands:"
  echo "  aplay -l"
  echo "  aplay /usr/share/sounds/alsa/Front_Center.wav"
  echo "  systemctl start jackd && systemctl status jackd --no-pager"
  echo "  tail -n 80 /var/log/lmn3/jackd.log"
  echo "  systemctl start lmn3 && systemctl status lmn3 --no-pager"
  echo "  sudo reboot   (recommended once everything looks good)"
}

main() {
  need_root
  assert_user_exists
  assert_bullseye
  health_checks

  install_packages
  disable_or_purge_pulseaudio
  configure_realtime_limits
  configure_alsa_for_testing_only
  create_jack_service
  create_lmn3_service
  final_notes
}

main "$@"
