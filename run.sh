#!/bin/sh
# Start ChessOS in QEMU on Linux or macOS. Uses the fast hardware CPU (KVM on
# Linux, HVF on macOS) when it is available, otherwise QEMU's slower software CPU.
AUDIO=pa
ACCEL="-accel kvm -accel tcg"
if [ "$(uname)" = "Darwin" ]; then
    AUDIO=coreaudio
    ACCEL="-accel hvf -accel tcg"
fi
exec qemu-system-x86_64 -m 256M $ACCEL -drive file="$(dirname "$0")/chessos.img",format=raw \
    -audiodev $AUDIO,id=sound -device AC97,audiodev=sound "$@"
