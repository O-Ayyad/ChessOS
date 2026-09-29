@echo off
rem Start ChessOS in QEMU on Windows. Uses the Windows Hypervisor Platform when it is
rem turned on (Windows features), otherwise QEMU's slower software CPU.
qemu-system-x86_64 -m 256M -accel whpx,kernel-irqchip=off -accel tcg -drive file="%~dp0chessos.img",format=raw -audiodev dsound,id=sound -device AC97,audiodev=sound %*
