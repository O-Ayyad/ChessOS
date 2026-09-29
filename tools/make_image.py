#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys

SECTOR_SIZE = 512
STAGE2_FIRST_SECTOR = 4           # must match boot/layout.inc
CD_SECTOR_SIZE = 2048
STAGE2_SECTOR_COUNT = 32          # must match boot/layout.inc
PARTITION_TABLE_OFFSET = 446
PARTITION_TYPE = 0xDA             # non-file-system data
BOOTABLE_FLAG = 0x80
HEADS = 16
SECTORS_PER_TRACK = 63

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, 'build')


def pad_to_cd_sectors(data):
    """Pad with zeros to a multiple of 2048 bytes."""
    return data + bytes((-len(data)) % CD_SECTOR_SIZE)


def assemble(source, output, defines):
    command = ['nasm', '-f', 'bin', '-I', os.path.join(ROOT, 'boot') + os.sep, source, '-o', output]
    for name, value in defines.items():
        command.append(f'-D{name}={value}')
    subprocess.run(command, check=True)
    with open(output, 'rb') as f:
        return f.read()


def chs(sector_number):
    """Convert a sector number to the old cylinder/head/sector form (3 bytes)."""
    cylinder = sector_number // (HEADS * SECTORS_PER_TRACK)
    head = (sector_number // SECTORS_PER_TRACK) % HEADS
    sector = sector_number % SECTORS_PER_TRACK + 1
    if cylinder > 1023:
        cylinder, head, sector = 1023, HEADS - 1, SECTORS_PER_TRACK
    return bytes([head, sector | ((cylinder >> 2) & 0xC0), cylinder & 0xFF])


def write_partition_table(image, total_sectors):
    # one bootable partition that covers the whole image (the CD's hard disk emulation needs it)
    first, last = 0, total_sectors - 1
    entry = bytes([BOOTABLE_FLAG]) + chs(first) + bytes([PARTITION_TYPE]) + chs(last)
    entry += first.to_bytes(4, 'little') + total_sectors.to_bytes(4, 'little')
    image[PARTITION_TABLE_OFFSET:PARTITION_TABLE_OFFSET + 16] = entry


BOOT_INFO_TABLE_START = 8 
BOOT_INFO_TABLE_END = 64
IMAGE_LOCATION_OFFSET = 12 


def make_iso_hybrid(iso_path, image_path):
    """Copies chessos.img's boot sector into the ISO's first sector, so the ISO can also
    boot from a USB stick. The CD tool has already written the file's position on the CD
    into that boot sector (the "boot info table"), which is how stage 1 finds it."""
    iso = bytearray(open(iso_path, 'rb').read())
    image_start = open(image_path, 'rb').read(CD_SECTOR_SIZE)
    # find chessos.img inside the CD: its first 2048 bytes match, except the boot info table
    for offset in range(0, len(iso), CD_SECTOR_SIZE):
        block = iso[offset:offset + CD_SECTOR_SIZE]
        if block[:BOOT_INFO_TABLE_START] == image_start[:BOOT_INFO_TABLE_START] and \
           block[BOOT_INFO_TABLE_END:] == image_start[BOOT_INFO_TABLE_END:]:
            break
    else:
        sys.exit('make_image: could not find chessos.img inside chessos.iso')
    cd_sector = int.from_bytes(block[IMAGE_LOCATION_OFFSET:IMAGE_LOCATION_OFFSET + 4], 'little')
    if cd_sector * CD_SECTOR_SIZE != offset:
        sys.exit('make_image: the CD tool did not fill in the boot info table')
    if any(iso[:SECTOR_SIZE]):
        sys.exit('make_image: the first sector of the ISO is not empty')
    boot_sector = bytearray(block[:SECTOR_SIZE])
    write_partition_table(boot_sector, len(iso) // SECTOR_SIZE)
    iso[:SECTOR_SIZE] = boot_sector
    with open(iso_path, 'wb') as f:
        f.write(iso)


def main():
    kernel_file = open(os.path.join(BUILD, 'kernel.bin'), 'rb').read()
    assets_file = open(os.path.join(BUILD, 'assets.tar'), 'rb').read()

    kernel = pad_to_cd_sectors(kernel_file)
    assets = pad_to_cd_sectors(assets_file)
    kernel_first = STAGE2_FIRST_SECTOR + STAGE2_SECTOR_COUNT
    assets_first = kernel_first + len(kernel) // SECTOR_SIZE
    defines = {
        'KERNEL_FIRST_SECTOR': kernel_first,
        'KERNEL_SECTOR_COUNT': len(kernel) // SECTOR_SIZE,
        'KERNEL_SIZE': len(kernel_file),
        'ASSETS_FIRST_SECTOR': assets_first,
        'ASSETS_SECTOR_COUNT': len(assets) // SECTOR_SIZE,
        'ASSETS_SIZE': len(assets_file),
    }
    stage1 = assemble(os.path.join(ROOT, 'boot', 'stage1.asm'), os.path.join(BUILD, 'stage1.bin'), {})
    stage2 = assemble(os.path.join(ROOT, 'boot', 'stage2.asm'), os.path.join(BUILD, 'stage2.bin'), defines)
    if len(stage1) != SECTOR_SIZE:
        sys.exit('stage 1 must be exactly 512 bytes')
    sectors_per_cd_sector = CD_SECTOR_SIZE // SECTOR_SIZE
    if (STAGE2_FIRST_SECTOR + STAGE2_SECTOR_COUNT) % sectors_per_cd_sector != 0:
        sys.exit('stage 2 must end on a 2048-byte boundary (for CDs): check STAGE2_SECTOR_COUNT')
    if len(stage2) > STAGE2_SECTOR_COUNT * SECTOR_SIZE:
        sys.exit('stage 2 is too big: raise STAGE2_SECTOR_COUNT here and in boot/layout.inc')

    image = bytearray(stage1)
    image += bytes((STAGE2_FIRST_SECTOR - 1) * SECTOR_SIZE)          # gap up to sector 4
    image += stage2 + bytes(STAGE2_SECTOR_COUNT * SECTOR_SIZE - len(stage2))
    image += kernel
    image += assets
    # round the whole image up to a full "cylinder", which some BIOSes like for disk emulation
    cylinder = HEADS * SECTORS_PER_TRACK * SECTOR_SIZE
    image += bytes((-len(image)) % cylinder)
    write_partition_table(image, len(image) // SECTOR_SIZE)

    image_path = os.path.join(ROOT, 'chessos.img')
    with open(image_path, 'wb') as f:
        f.write(image)
    print(f'chessos.img: {len(image) // 1024} KB (kernel {len(kernel_file) // 1024} KB, assets {len(assets_file) // 1024} KB)')

    iso_path = os.path.join(ROOT, 'chessos.iso')
    if shutil.which('xorriso') is not None:
        cd_folder = os.path.join(BUILD, 'cd')
        shutil.rmtree(cd_folder, ignore_errors=True)
        os.makedirs(cd_folder)
        shutil.copy(image_path, os.path.join(cd_folder, 'chessos.img'))
        subprocess.run(['xorriso', '-as', 'mkisofs', '-quiet', '-V', 'CHESSOS', '-b', 'chessos.img',
                        '-no-emul-boot', '-boot-load-size', '4', '-boot-info-table', '-o', iso_path, cd_folder], check=True)
    else:
        try:
            import pycdlib
        except ImportError:
            print('xorriso and pycdlib not found: skipping chessos.iso (the .img works on its own).')
            print('   To get the ISO too:  python -m pip install pycdlib')
            if os.path.exists(iso_path):
                os.remove(iso_path)       # don't leave an old, out-of-date ISO lying around
            return
        BOOT_LOAD_SECTORS = 4             # the BIOS loads 4 x 512 bytes = the first 2048 bytes
        cd = pycdlib.PyCdlib()
        cd.new(vol_ident='CHESSOS')
        cd.add_file(image_path, '/CHESSOS.IMG;1')
        cd.add_eltorito('/CHESSOS.IMG;1', bootcatfile='/BOOT.CAT;1', media_name='noemul',
                        boot_load_size=BOOT_LOAD_SECTORS, boot_info_table=True)
        cd.write(iso_path)
        cd.close()
    make_iso_hybrid(iso_path, image_path)
    print('chessos.iso: done')


if __name__ == '__main__':
    main()