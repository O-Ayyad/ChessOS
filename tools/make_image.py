#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys

SECTOR_SIZE = 512
CD_SECTOR_SIZE = 2048
STAGE2_FIRST_SECTOR = 4  # must match boot/layout.inc
STAGE2_SECTOR_COUNT = 32

PARTITION_TABLE_OFFSET = 446
PARTITION_TYPE = 0xDA
BOOTABLE_FLAG = 0x80
HEADS = 16
SECTORS_PER_TRACK = 63

BOOT_INFO_TABLE_START = 8
BOOT_INFO_TABLE_END = 64
IMAGE_LOCATION_OFFSET = 12
BOOT_LOAD_SECTORS = 4

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, 'build')


def read_file(path):
    with open(path, 'rb') as f:
        return f.read()


def pad_to_cd_sectors(data):
    """Pad with zeros to a multiple of 2048 bytes."""
    return data + bytes((-len(data)) % CD_SECTOR_SIZE)


def assemble(source, output, defines):
    """Runs nasm on a boot loader file and returns the bytes it made."""
    command = ['nasm', '-f', 'bin', '-I', os.path.join(ROOT, 'boot') + os.sep, source, '-o', output]
    for name, value in defines.items():
        command.append(f'-D{name}={value}')
    subprocess.run(command, check=True)
    return read_file(output)


def chs(sector_number):
    """Convert a sector number to the old cylinder/head/sector form (3 bytes)."""
    cylinder = sector_number // (HEADS * SECTORS_PER_TRACK)
    head = (sector_number // SECTORS_PER_TRACK) % HEADS
    sector = sector_number % SECTORS_PER_TRACK + 1
    if cylinder > 1023:
        #the convention is to give the biggest values
        cylinder, head, sector = 1023, HEADS - 1, SECTORS_PER_TRACK
    return bytes([head, sector | ((cylinder >> 2) & 0xC0), cylinder & 0xFF])


def write_partition_table(image, total_sectors):
    # one bootable partition that covers the whole image (the CD's hard disk emulation needs it)
    first, last = 0, total_sectors - 1
    entry = bytes([BOOTABLE_FLAG]) + chs(first) + bytes([PARTITION_TYPE]) + chs(last)
    entry += first.to_bytes(4, 'little') + total_sectors.to_bytes(4, 'little')
    image[PARTITION_TABLE_OFFSET:PARTITION_TABLE_OFFSET + 16] = entry


def make_image(image_path):
    kernel_file = read_file(os.path.join(BUILD, 'kernel.bin'))
    assets_file = read_file(os.path.join(BUILD, 'assets.tar'))
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
    # gap up to sector 4
    image += bytes((STAGE2_FIRST_SECTOR - 1) * SECTOR_SIZE)
    image += stage2 + bytes(STAGE2_SECTOR_COUNT * SECTOR_SIZE - len(stage2))
    image += kernel
    image += assets
    # round the whole image up to a full "cylinder", which some BIOSes like for disk emulation
    cylinder = HEADS * SECTORS_PER_TRACK * SECTOR_SIZE
    image += bytes((-len(image)) % cylinder)
    write_partition_table(image, len(image) // SECTOR_SIZE)

    with open(image_path, 'wb') as f:
        f.write(image)
    print(f'chessos.img: {len(image) // 1024} KB (kernel {len(kernel_file) // 1024} KB, assets {len(assets_file) // 1024} KB)')


def make_iso(iso_path, image_path):
    if shutil.which('xorriso') is not None:
        cd_folder = os.path.join(BUILD, 'cd')
        shutil.rmtree(cd_folder, ignore_errors=True)
        os.makedirs(cd_folder)
        shutil.copy(image_path, os.path.join(cd_folder, 'chessos.img'))
        subprocess.run(['xorriso', '-as', 'mkisofs', '-quiet', '-V', 'CHESSOS', '-b', 'chessos.img',
                        '-no-emul-boot', '-boot-load-size', str(BOOT_LOAD_SECTORS), '-boot-info-table',
                        '-o', iso_path, cd_folder], check=True)
        return True

    try:
        import pycdlib
    except ImportError:
        return False
    cd = pycdlib.PyCdlib()
    cd.new(vol_ident='CHESSOS')
    cd.add_file(image_path, '/CHESSOS.IMG;1')
    cd.add_eltorito('/CHESSOS.IMG;1', bootcatfile='/BOOT.CAT;1', media_name='noemul',
                    boot_load_size=BOOT_LOAD_SECTORS, boot_info_table=True)
    cd.write(iso_path)
    cd.close()
    return True


def make_iso_hybrid(iso_path, image_path):
    #Copies chessos.img's boot sector into the ISO first sector so the ISO can boot from a USB stick
    iso = bytearray(read_file(iso_path))
    image_start = read_file(image_path)[:CD_SECTOR_SIZE]

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
    image_path = os.path.join(ROOT, 'chessos.img')
    iso_path = os.path.join(ROOT, 'chessos.iso')
    make_image(image_path)

    if not make_iso(iso_path, image_path):
        print('xorriso and pycdlib not found: skipping chessos.iso (the .img works on its own).')
        print('   To get the ISO too:  python -m pip install pycdlib')
        if os.path.exists(iso_path):
            os.remove(iso_path) 
        return
    make_iso_hybrid(iso_path, image_path)
    print('chessos.iso: done')


if __name__ == '__main__':
    main()
