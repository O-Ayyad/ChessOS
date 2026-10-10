#pragma once
#include "types.h"

struct AssetFile {
    // /assets/pieces/white_pawn_s_0.bmp
    char path[160];
    const u8* data;
    u64 size;
};

void setup_asset_files(const u8* archive, u64 archive_size);
int asset_file_count();
const AssetFile& asset_file(int index);
const AssetFile* find_asset_file(const char* ending);
