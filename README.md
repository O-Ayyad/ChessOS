# ChessOS

ChessOS is a small operating system that does one thing: play chess. It starts
straight from a USB stick, a CD or a virtual machine without Windows, Linux,
a borrowed boot loader or other high-level systems. Every byte that runs after the PC's BIOS is in this
folder: a boot loader written in assembly (`boot/`) and a kernel written in C++ (`kernel/`).

- **8 viewpoints** of the board (← and →): four from above and four 3D corner views.
- **Play the computer or a friend.** Opponents range from a toddler to deities. They greet you, taunt you, get nervous, and laugh when they win.
- **Type moves** (`e2e4`, `Nf3`, `/select e2` then `/move e4`) or **click and drag**.
- **Replay** a finished game with B (back) and M (forward).
- **MIDI music** (up to 8 songs) through the AC'97 sound card or the PC speaker.
- The mouse and music keep working while the computer is thinking.

## Running ChessOS

`make` builds two files. They hold the same system, packaged two ways:

| File | Use it for |
|---|---|
| `chessos.img` | QEMU, `dd`, and writing to USB sticks |
| `chessos.iso` | VirtualBox, VMware and Rufus. It can boot from a USB stick. |

### Quick start: QEMU

```bash
make run    
./run.sh        # Linux / macOS / WSL
run.bat         # Windows (QEMU for Windows must be installed and in path)
```

### Virtual machines

**VirtualBox**
1. **New** Select chessos.iso and run.

**VMware**: use the same settings. Choose *Other 64-bit* with BIOS firmware.

### USB stick

| Tool | How |
|---|---|
| **Rufus** (Windows) | Select `chessos.iso` or `chessos.img`. When Rufus asks, choose **"Write in DD Image mode"**. |
| **balenaEtcher** (any OS) | Select either file, pick the stick, then Flash. |
| **dd** (Linux / macOS) | `sudo dd if=chessos.img of=/dev/sdX bs=4M status=progress && sync` |

> After writing, Windows shows the stick as 5 MB that it can't open. This is normal since the stick now holds ChessOS instead a normal file system. To get the stick back for normal use, format it with Rufus or with Disk Management (*Delete volume*, then *New simple volume*).

### Real hardware

ChessOS is a **BIOS (legacy) operating system**:

- **Boot mode:** in the firmware settings, turn on **CSM** / **Legacy Boot** and turn off **Secure Boot**. Then choose the USB stick from the boot menu.
- **UEFI-only PCs:** many new PCs no CSM option at all. They can't run ChessOS so please use a virtual machine instead.
- **Sound:** plays through AC'97 audio. Most PCs have HD Audio instead, so on real hardware ChessOS usually runs silently.

```

## How it boots

1. **The BIOS** loads the first 512 bytes of the disk (`boot/stage1.asm`) and runs them.
2. **Stage 1** checks whether it is on a hard disk/USB stick or a CD, loads stage 2 and jumps to it.
3. **Stage 2** (`boot/stage2.asm`), still in 16-bit mode:
   - switches on the A20 line (so memory above 1 MB works),
   - asks the BIOS for the memory map,
   - copies the kernel to 1 MB and the asset archive to 16 MB,
   - picks a 1024 × 768 32-bit graphics mode,
   - builds page tables and switches the CPU to 64-bit mode,
   - jumps to `kernel_start()` in `kernel/main.cpp`.

4. **The kernel** sets up memory, the screen, the timer, the keyboard and mouse and
   the sound, loads the pictures and characters, and shows the menu.

`tools/make_image.py` puts the disk image together.

## Credits

- Art by [Paledoptera](https://linktr.ee/paledoptera). Find more of their work here: https://linktr.ee/paledoptera 
- VGA console fonts from Debian's console setup package.
- Piece-square tables from Tomasz Michniewski's "Simplified Evaluation Function".
