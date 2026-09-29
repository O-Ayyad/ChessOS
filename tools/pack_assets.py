import os
import struct
import sys
import tarfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_SONGS = 8
KEEP_EXTENSIONS = ('.bmp', '.txt')
PORTRAIT_SIZE = 64  
PIECE_SIZE = 192 
BMP_WIDTH_OFFSET = 18  

def asset_paths():
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
    height = abs(height)     # a negative height just means "stored top row first"
    size = PIECE_SIZE if '/pieces/' in path.replace(os.sep, '/') else PORTRAIT_SIZE
    if (width, height) != (size, size):
        return f'is {width} x {height}, must be {size} x {size}'
    return None


def check_pictures(paths):
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
    music = os.path.join(ROOT, 'music')
    if not os.path.isdir(music):
        return []
    songs = sorted(n for n in os.listdir(music) if n.lower().endswith(('.mid', '.midi')))
    if len(songs) > MAX_SONGS:
        print(f'note: only the first {MAX_SONGS} songs in music/ are used', file=sys.stderr)
    return ['music/' + name for name in songs[:MAX_SONGS]]


def main():
    paths = asset_paths() + song_paths()
    check_pictures(paths)
    os.makedirs(os.path.join(ROOT, 'build'), exist_ok=True)
    with tarfile.open(os.path.join(ROOT, 'build', 'assets.tar'), 'w', format=tarfile.USTAR_FORMAT) as archive:
        for path in paths:
            info = archive.gettarinfo(os.path.join(ROOT, path), arcname=path)
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            info.mtime = 0
            with open(os.path.join(ROOT, path), 'rb') as f:
                archive.addfile(info, f)
    print(len(paths), 'files packed into build/assets.tar')


if __name__ == '__main__':
    main()
