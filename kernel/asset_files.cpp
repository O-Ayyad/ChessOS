
#include "asset_files.h"
#include "text_utils.h"
#include "memory.h"

const int MAX_ASSET_FILES = 1024;
const int TAR_BLOCK_SIZE = 512;
const int TAR_NAME_OFFSET = 0;
const int TAR_NAME_LENGTH = 100;
const int TAR_SIZE_OFFSET = 124;
const int TAR_SIZE_LENGTH = 12;
const int TAR_TYPE_OFFSET = 156;

//long paths split into prefix and name
const int TAR_PREFIX_OFFSET = 345;
const int TAR_PREFIX_LENGTH = 155;
const char TAR_TYPE_NORMAL_FILE = '0';

// The archive is tar file 
// for every file 512-byte header followed by its contents + pad to a multiple of 512
static AssetFile* files = nullptr;
static int file_count = 0;

//nums in a tar header written as text in octal
static u64 read_octal_number(const u8* text, int length) {

    u64 value = 0;
    for (int i = 0; i < length && text[i] != 0; i++) {
        if (text[i] >= '0' && text[i] <= '7') {
            value = value * 8 + (text[i] - '0');
        }
    }
    return value;
}

static u64 round_up_to_block(u64 size) {
    return (size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE * TAR_BLOCK_SIZE;
}

void setup_asset_files(const u8* archive, u64 archive_size) {
    files = allocate_array<AssetFile>(MAX_ASSET_FILES);
    file_count = 0;

    u64 position = 0;
    while (position + TAR_BLOCK_SIZE <= archive_size && file_count < MAX_ASSET_FILES) {
        const u8* header = archive + position;
        if (header[TAR_NAME_OFFSET] == 0) {
            break;      //the end of the archive
        }

        u64 size = read_octal_number(header + TAR_SIZE_OFFSET, TAR_SIZE_LENGTH);
        char type = header[TAR_TYPE_OFFSET];

        if (type == TAR_TYPE_NORMAL_FILE || type == 0) {

            //build the path /prefix/name

            Text path;
            path.add_char('/');

            if (header[TAR_PREFIX_OFFSET] != 0) {
                for (int i = 0; i < TAR_PREFIX_LENGTH && header[TAR_PREFIX_OFFSET + i] != 0; i++) {
                    path.add_char(header[TAR_PREFIX_OFFSET + i]);
                }
                path.add_char('/');
            }
            
            for (int i = 0; i < TAR_NAME_LENGTH && header[TAR_NAME_OFFSET + i] != 0; i++) {
                path.add_char(header[TAR_NAME_OFFSET + i]);
            }

            AssetFile& file = files[file_count];
            copy_text(file.path, path.text, sizeof(file.path));
            file.data = header + TAR_BLOCK_SIZE;
            file.size = size;
            file_count++;
        }
        position += TAR_BLOCK_SIZE + round_up_to_block(size);
    }
}

int asset_file_count() {
    return file_count;
}

const AssetFile& asset_file(int index) {
    return files[index];
}

const AssetFile* find_asset_file(const char* ending) {
    for (int i = 0; i < file_count; i++) {
        if (text_ends_with(files[i].path, ending)) {
            return &files[i];
        }
    }
    return nullptr;
}
