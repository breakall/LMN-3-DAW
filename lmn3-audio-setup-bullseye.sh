#!/usr/bin/env bash
set -euo pipefail

# =========================
# LMN-3 Audio Setup (Bullseye)
# Raspberry Pi 4 deterministic audio configuration:
#   ALSA -> JACK -> LMN-3
#
# Scope: audio only.
#
# Interactive behavior:
# - Enumerates ALSA playback devices
# - Presents a numbered list
# - Prompts you to choose one
# - Uses the selection for:
#     * /etc/asound.conf (predictable ALSA tests)
#     * systemd jackd.service (opens selected device)
#
# Non-interactive behavior:
# - Set AUDIO_CARD_DEVICE="CARD,DEVICE" (e.g. "1,0") to skip prompting.
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
# If set, skips interactive prompt. Format: "CARD,DEVICE" e.g. "1,0"
AUDIO_CARD_DEVICE="${AUDIO_CARD_DEVICE:-}"

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

validate_card_device_format() {
  local v="$1"
  [[ "$v" =~ ^[0-9]+,[0-9]+$ ]] || die "Value must look like 'CARD,DEVICE' e.g. '1,0' (got: '$v')"
}

# Build a numbered list of playback devices using /proc/asound (more parseable than aplay -l output)
# Produces lines like:
#   idx|card|device|card_name|pcm_name
enumerate_playback_devices() {
  local idx=0
  local card_dir card_num card_name pcm_dir dev_num pcm_info pcm_name

  # /proc/asound/cardX directories
  for card_dir in /proc/asound/card[0-9]*; do
    [[ -d "${card_dir}" ]] || continue
    card_num="$(basename "${card_dir}" | sed 's/^card//')"
    card_name="$(tr -d '\0' < "${card_dir}/id" 2>/dev/null || echo "card${card_num}")"

    # /proc/asound/cardX/pcmYp directories (playback)
    for pcm_dir in "${card_dir}"/pcm*p; do
      [[ -d "${pcm_dir}" ]] || continue
      dev_num="$(basename "${pcm_dir}" | sed -E 's/^pcm([0-9]+)p$/\1/')"

      pcm_info="${pcm_dir}/info"
      if [[ -f "${pcm_info}" ]]; then
        # Prefer "name:" line if present
        pcm_name="$(grep -E '^name:' "${pcm_info}" | head -n 1 | sed 's/^name:[[:space:]]*//' || true)"
        [[ -n "${pcm_name}" ]] || pcm_name="$(grep -E '^stream:' "${pcm_info}" | head -n 1 | sed 's/^stream:[[:space:]]*//' || true)"
      else
        pcm_name="playback"
      fi

      printf "%d|%s|%s|%s|%s\n" "${idx}" "${card_num}" "${dev_num}" "${card_name}" "${pcm_name}"
      idx=$((idx+1))
    done
  done
}

present_device_menu_and_choose() {
  if [[ -n "${AUDIO_CARD_DEVICE}" ]]; then
    validate_card_device_format "${AUDIO_CARD_DEVICE}"
    return 0
  fi

  log "Enumerating ALSA playback devices..."
  local devices
  devices="$(enumerate_playback_devices || true)"

  if [[ -z "${devices}" ]]; then
    log "aplay -l output for debugging:"
    aplay -l || true
    die "No ALSA playback devices found under /proc/asound. Check drivers/hardware."
  fi

  echo
  echo "Available playback devices:"
  echo "--------------------------"

  # Print menu
  while IFS='|' read -r idx card dev card_name pcm_name; do
    printf "  [%s] hw:%s,%s  (%s)  - %s\n" "${idx}" "${card}" "${dev}" "${card_name}" "${pcm_name}"
  done <<< "${devices}"

  echo
  echo "Choose a device by number. (Example: if you previously heard audio on hw:1,0, pick that.)"
  echo

  local choice max_idx
  max_idx="$(echo "${devices}" | awk -F'|' 'END{print $1}')"

  while true; do
    read -r -p "Enter selection [0-${max_idx}]: " choice
    [[ "${choice}" =~ ^[0-9]+$ ]] || { echo "Not a number."; continue; }
    (( choice >= 0 && choice <= max_idx )) || { echo "Out of range."; continue; }

    # Map selection -> card,device
    local selected
    selected="$(echo "${devices}" | awk -F'|' -v c="${choice}" '$1==c{print $2","$3; exit}')"
    [[ -n "${selected}" ]] || { echo "Selection not found."; continue; }

    AUDIO_CARD_DEVICE="${selected}"
    validate_card_device_format "${AUDIO_CARD_DEVICE}"

    echo
    echo "Selected: hw:${AUDIO_CARD_DEVICE}"
    echo
    break
  done
}

test_selected_device() {
  local test_cmd
  test_cmd="aplay -D hw:${AUDIO_CARD_DEVICE} /usr/share/sounds/alsa/Front_Center.wav"

  echo "Testing selected device with:"
  echo "  ${test_cmd}"
  echo "(You should hear 'Front Center'.)"
  echo

  if ! ${test_cmd}; then
    warn "Test playback failed. Mixer could be muted or device isn't connected."
    warn "Try: alsamixer (F6 to select the card) and unmute PCM/Master."
  fi
}

configure_alsa_for_testing_only() {
  local audio_hw="hw:${AUDIO_CARD_DEVICE}"
  log "Creating minimal /etc/asound.conf (default -> ${audio_hw})"

  if [[ -f /etc/asound.conf ]]; then
    cp -n /etc/asound.conf /etc/asound.conf.lmn3.bak || true
    log "Backed up /etc/asound.conf to /etc/asound.conf.lmn3.bak"
  fi

  local ctl_card="0"
  if [[ "${AUDIO_CARD_DEVICE}" =~ ^([0-9]+),[0-9]+$ ]]; then
    ctl_card="${BASH_REMATCH[1]}"
  fi

  cat >/etc/asound.conf <<EOF
pcm.!default {
  type plug
  slave.pcm "${audio_hw}"
  slave.rate ${JACK_RATE}
}

# Control interface is mostly irrelevant for JACK; kept generic to avoid tool failures.
ctl.!default {
  type hw
  card ${ctl_card}
}
EOF

  systemctl enable alsa-state 2>/dev/null || true
}

create_jack_service() {
  local audio_hw="hw:${AUDIO_CARD_DEVICE}"

  log "Creating systemd service: jackd (device ${audio_hw})"

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
ExecStart=/usr/bin/jackd -P${JACK_RT_PRIO} -dalsa -d${audio_hw} -r${JACK_RATE} -p${JACK_PERIOD} -n${JACK_NPERIODS}
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
  log "Selected device: hw:${AUDIO_CARD_DEVICE}"
  log "Verification commands:"
  echo "  aplay /usr/share/sounds/alsa/Front_Center.wav"
  echo "  systemctl start jackd && systemctl status jackd --no-pager"
  echo "  tail -n 80 /var/log/lmn3/jackd.log"
  echo "  systemctl start lmn3 && systemctl status lmn3 --no-pager"
  echo "  sudo reboot   (recommended once everything looks good)"
  echo
  echo "Non-interactive usage:"
  echo "  sudo AUDIO_CARD_DEVICE=${AUDIO_CARD_DEVICE} LMN3_CMD=${LMN3_CMD} $0"
}

main() {
  need_root
  assert_user_exists
  assert_bullseye
  health_checks

  install_packages
  disable_or_purge_pulseaudio
  configure_realtime_limits

  present_device_menu_and_choose
  test_selected_device

  configure_alsa_for_testing_only
  create_jack_service
  create_lmn3_service
  final_notes
}

main "$@"
