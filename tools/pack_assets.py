import os
import struct
import sys
import tarfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_SONGS = 64                   # must match kernel/synthesizer.h
KEEP_EXTENSIONS = ('.bmp', '.txt')
PORTRAIT_SIZE = 64              #must match PORTRAIT_FILE_SIZE in kernel/bitmap.h
PIECE_SIZE = 192                #must match PIECE_FILE_SIZE in kernel/bitmap.h
# the width is at byte 18 of a .bmp file, the height right after
BMP_WIDTH_OFFSET = 18


def asset_paths():
    """Every picture and text file under assets/, in a fixed order."""
    paths = []
    for folder, subfolders, names in sorted(os.walk(os.path.join(ROOT, 'assets'))):
        subfolders.sort()
        for name in sorted(names):
            if name.endswith(KEEP_EXTENSIONS):
                paths.append(os.path.relpath(os.path.join(folder, name), ROOT).replace(os.sep, '/'))
    return paths


def bitmap_problem(path):
    """Returns why a .bmp breaks the picture rule, or None if it is fine."""
    with open(path, 'rb') as f:
        header = f.read(BMP_WIDTH_OFFSET + 8)
    if len(header) < BMP_WIDTH_OFFSET + 8 or header[:2] != b'BM':
        return 'not a BMP file'
    width, height = struct.unpack_from('<ii', header, BMP_WIDTH_OFFSET)
    # a negative height just means "stored top row first"
    height = abs(height)
    size = PIECE_SIZE if '/pieces/' in path.replace(os.sep, '/') else PORTRAIT_SIZE
    if (width, height) != (size, size):
        return f'is {width} x {height}, must be {size} x {size}'
    return None


def check_pictures(paths):
    """Stops with an error if a picture has the wrong size, and warns about .png files."""
    problems = []
    for path in paths:
        if path.endswith('.bmp'):
            problem = bitmap_problem(os.path.join(ROOT, path))
            if problem:
                problems.append(f'  {path}: {problem}')
    if problems:
        print('These pictures have the wrong size (portraits 64 x 64, pieces 192 x 192):', file=sys.stderr)
        print('\n'.join(problems), file=sys.stderr)
        sys.exit(1)

    for folder, subfolders, names in os.walk(os.path.join(ROOT, 'assets')):
        for name in names:
            if name.lower().endswith('.png'):
                print(f'note: {os.path.join(folder, name)} is ignored - ChessOS only reads .bmp '
                      '(tools/picture_to_bmp.py converts it)', file=sys.stderr)


def song_paths():
    """The first MAX_SONGS MIDI files in midi/, in alphabetical order."""
    midi = os.path.join(ROOT, 'midi')
    if not os.path.isdir(midi):
        return []
    songs = sorted(n for n in os.listdir(midi) if n.lower().endswith(('.mid', '.midi')))
    if len(songs) > MAX_SONGS:
        print(f'note: only the first {MAX_SONGS} songs in midi/ are used', file=sys.stderr)
    return ['midi/' + name for name in songs[:MAX_SONGS]]


def main():
    paths = asset_paths() + song_paths()
    check_pictures(paths)

    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    archive_path = os.path.join(ROOT, 'build', 'assets.tar')
    with tarfile.open(archive_path, 'w', format=tarfile.USTAR_FORMAT) as archive:
        for path in paths:
            info = archive.gettarinfo(os.path.join(ROOT, path), arcname=path)
            # leave out the owner and the date so the same files always give the same archive
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            info.mtime = 0
            with open(os.path.join(ROOT, path), 'rb') as f:
                archive.addfile(info, f)
    print(len(paths), 'files packed into build/assets.tar')


if __name__ == '__main__':
    main()
