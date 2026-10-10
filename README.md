# ChessOS

ChessOS is a small operating system that does one thing: play chess. It starts
straight from a USB stick, a CD or a virtual machine without Windows, Linux,
a borrowed boot loader or other high-level systems. Every byte that runs after the PC's BIOS is in this
folder: a boot loader written in assembly (`boot/`) and a kernel written in C++ (`kernel/`).

- **8 viewpoints** of the board (← and →): four from above and four 3D corner views.
- **Play the computer or a friend.** Opponents range from a toddler to deities. They greet you, taunt you, get nervous, and laugh when they win.
- **Type moves** (`e2e4`, `Nf3`, `/select e2` then `/move e4`) or **click and drag**.
- **Replay** a finished game with B (back) and M (forward).

<img width="1021" height="767" alt="menu" src="https://github.com/user-attachments/assets/43d617cb-c8af-4d87-90d5-74bea1f9d686" />

<img width="1019" height="768" alt="" src="https://github.com/user-attachments/assets/badfb066-8689-4f56-aefd-ef9cc0576675" />

## Releases

Download ChessOS binaries from the [Releases page](https://github.com/O-Ayyad/ChessOS/releases/latest).

| Download | Use it for |
|---|---|
| `ChessOS-Setup.exe` | Windows. Run ChessOS from start menu   |
| `chessos.img` | QEMU, `dd`, and writing to USB sticks |
| `chessos.iso` | VirtualBox, VMware and Rufus. It can also boot from a USB stick. |


## Running ChessOS

### Quick start: QEMU

```bash
qemu-system-x86_64 -drive format=raw,file=chessos.img
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

> After writing, Windows shows the stick as 5 MB that it can't open. This is normal since the stick now holds ChessOS instead of a normal file system. To get the stick back for normal use, format it with Rufus or with Disk Management (*Delete volume*, then *New simple volume*).

### Real hardware

ChessOS is a **BIOS (legacy) operating system**:

- **Boot mode:** in the firmware settings, turn on **CSM** / **Legacy Boot** and turn off **Secure Boot**. Then choose the USB stick from the boot menu.
- **UEFI-only PCs:** many new PCs have no CSM option at all. They can't run ChessOS so please use a virtual machine instead.
- **Sound:** plays through AC'97 audio. Most PCs have HD Audio instead, so on real hardware ChessOS usually runs silently.

## Building from source

**Linux / WSL**
```bash
sudo apt update
sudo apt install nasm qemu-system
```

```bash
make run
./run.sh        # Linux / macOS / WSL
run.bat         # Windows (QEMU for Windows must be installed and in PATH)
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

- Midi files:
   - [Fur Elise](https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=931)
   - [Ode to Joy](https://bitmidi.com/beethoven-symphony9-4-ode-to-joy-piano-solo-mid)
   - [Radetsky Strauss](https://www.mididb.com/strauss/radetzky-midi/)
   - [The Entertainer](https://bitmidi.com/the-entertainer-mid)
   - [Rondo alla Turca](https://www.midiworld.com/search/?q=Turkish+March)
   - [Bach Prelude](https://midifind.com/files/b/bach_johann_sebastian/bach_johann_sebastian_prelude_in_c_major/592-1-0-14736)
   - [Pachelbel Canon](https://www.pachelbelcanon.com/pachelbel-canon-files/category/1-pachelbel-canon.html#)
   
- VGA console fonts from Debian's console setup package.
- Piece-square tables from Tomasz Michniewski's "Simplified Evaluation Function".
