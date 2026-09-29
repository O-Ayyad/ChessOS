#include "bitmap.h"

// written by claude (mostly)
const int OFFSET_PIXEL_DATA = 10;
const int OFFSET_INFO_HEADER_SIZE = 14;
const int OFFSET_WIDTH = 18;
const int OFFSET_HEIGHT = 22;
const int OFFSET_BITS_PER_PIXEL = 28;
const int OFFSET_COMPRESSION = 30;
const int OFFSET_PALETTE_COLORS = 46;
const int FILE_HEADER_SIZE = 14;
const int SMALLEST_INFO_HEADER = 40;
const u32 NO_COMPRESSION = 0;
const u32 BITFIELDS = 3;
const int BYTES_PER_PALETTE_COLOR = 4;
const u32 ALPHA_SOLID = 0xFF000000;
// /written by claude

static u32 read_16(const u8* bytes) {
    return bytes[0] | (bytes[1] << 8);
}

static u32 read_32(const u8* bytes) {
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((u32)bytes[3] << 24);
}

bool open_bitmap(const u8* file, u64 file_size, int wanted_size, Bitmap* bitmap, const char** error) {
    *error = "not a BMP file";
    if (file_size < FILE_HEADER_SIZE + SMALLEST_INFO_HEADER || file[0] != 'B' || file[1] != 'M') return false;

    u32 pixel_data_offset = read_32(file + OFFSET_PIXEL_DATA);
    u32 info_header_size = read_32(file + OFFSET_INFO_HEADER_SIZE);
    int width = (int)read_32(file + OFFSET_WIDTH);
    int height = (int)read_32(file + OFFSET_HEIGHT);
    int bits_per_pixel = read_16(file + OFFSET_BITS_PER_PIXEL);
    u32 compression = read_32(file + OFFSET_COMPRESSION);

    bitmap->stored_top_down = height < 0;
    if (height < 0) height = -height;

    if (wanted_size == PORTRAIT_FILE_SIZE) *error = "portraits must be exactly 64 x 64 pixels";
    else                                   *error = "chess pieces must be exactly 192 x 192 pixels";
    if (width != wanted_size || height != wanted_size) return false;
    bitmap->size = wanted_size;

    *error = "must be a 4, 8, 24 or 32-bit BMP without compression";
    bool depth_ok = bits_per_pixel == 4 || bits_per_pixel == 8 || bits_per_pixel == 24 || bits_per_pixel == 32;
    bool compression_ok = compression == NO_COMPRESSION || (bits_per_pixel == 32 && compression == BITFIELDS);
    if (!depth_ok || !compression_ok) return false;

    bitmap->bits_per_pixel = bits_per_pixel;
    bitmap->bytes_per_row = (width * bits_per_pixel + 31) / 32 * 4;     // padded to 4 bytes
    bitmap->palette = file + FILE_HEADER_SIZE + info_header_size;
    bitmap->pixel_rows = file + pixel_data_offset;

    *error = "the file is shorter than its header says";

    if (pixel_data_offset + (u64)bitmap->bytes_per_row * height > file_size) return false;
    if (bits_per_pixel <= 8) {
        u32 palette_colors = read_32(file + OFFSET_PALETTE_COLORS);
        if (palette_colors == 0) palette_colors = 1 << bits_per_pixel;
        if (FILE_HEADER_SIZE + info_header_size + palette_colors * BYTES_PER_PALETTE_COLOR > pixel_data_offset) return false;
    }

    *error = "ok";
    return true;
}

u32 bitmap_pixel(const Bitmap& bitmap, int x, int y) {
    int stored_row = bitmap.stored_top_down ? y : bitmap.size - 1 - y;
    const u8* row = bitmap.pixel_rows + stored_row * bitmap.bytes_per_row;

    u32 color;
    if (bitmap.bits_per_pixel == 24 || bitmap.bits_per_pixel == 32) {
        const u8* pixel = row + x * (bitmap.bits_per_pixel / 8);
        color = (pixel[2] << 16) | (pixel[1] << 8) | pixel[0]; 
    } else {
        int palette_index;
        if (bitmap.bits_per_pixel == 8) {
            palette_index = row[x];
        } else {
            u8 two_pixels = row[x / 2];
            palette_index = (x % 2 == 0) ? (two_pixels >> 4) : (two_pixels & 0x0F);
        }
        const u8* entry = bitmap.palette + palette_index * BYTES_PER_PALETTE_COLOR;
        color = (entry[2] << 16) | (entry[1] << 8) | entry[0];
    }

    if (color == SEE_THROUGH_COLOR) return 0;
    return ALPHA_SOLID | color;
}

void draw_scaled_pixel(Image& target, int x, int y, u32 pixel, int scale) {
    if (pixel_alpha(pixel) == 0) return;          // see-through
    for (int block_y = y; block_y < y + scale; block_y++) {
        if (block_y < 0 || block_y >= target.height) continue;
        for (int block_x = x; block_x < x + scale; block_x++) {
            if (block_x < 0 || block_x >= target.width) continue;
            target.pixels[block_y * target.width + block_x] = pixel_color(pixel);
        }
    }
}

void draw_bitmap(Image& target, const Bitmap& bitmap, int x, int y, int scale) {
    for (int row = 0; row < bitmap.size; row++) {
        for (int column = 0; column < bitmap.size; column++) {
            u32 pixel = bitmap_pixel(bitmap, column, row);
            draw_scaled_pixel(target, x + column * scale, y + row * scale, pixel, scale);
        }
    }
}

VisibleArea find_visible_area(const Bitmap& bitmap) {
    VisibleArea area = { bitmap.size, bitmap.size, 0, 0 };
    for (int y = 0; y < bitmap.size; y++) {
        for (int x = 0; x < bitmap.size; x++) {
            if (pixel_alpha(bitmap_pixel(bitmap, x, y)) == 0) continue;
            if (x < area.left) area.left = x;
            if (y < area.top) area.top = y;
            if (x + 1 > area.right) area.right = x + 1;
            if (y + 1 > area.bottom) area.bottom = y + 1;
        }
    }
    return area;
}
