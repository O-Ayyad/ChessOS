#include "characters.h"
#include "asset_files.h"
#include "screen.h"
#include "memory.h"
#include "math_utils.h"
#include "text_utils.h"

const int MAX_CHARACTERS = 64;
const char* CHARACTERS_FOLDER = "/assets/characters/";
const int DEFAULT_ELO = 1000;

static Character* characters = nullptr;
static int number_of_characters = 0;

//In the same order as the Face and LineKind enums
static const char* FACE_FILE_NAMES[FACE_COUNT] = {
    "default.bmp", "sad.bmp", "happy.bmp", "victory.bmp", "defeat.bmp", "scared.bmp", "shocked.bmp", "laugh1.bmp", "laugh2.bmp"
};
static const char* LINE_KEYS[LINE_KIND_COUNT] = {
    "greet", "taunt", "humble", "smirk", "victory", "scared", "defeat", "shocked", "win", "lose", "draw", "rook_sac",
};

int character_count() { return number_of_characters; }
Character& get_character(int index) { return characters[index]; }

const char* pick_line(const Character& character, LineKind kind) {
    int count = character.line_count[kind];
    if (count == 0) return nullptr;
    return character.lines[kind][random_below(count)];
}

static bool split_path(const char* path, const char* folder, char* name, int name_capacity, const char** file) {
    const char* start = find_text(path, folder);
    if (start == nullptr) return false;

    const char* position = start + text_length(folder);
    int length = 0;
    while (position[length] != 0 && position[length] != '/') length++;
    if (position[length] != '/' || length >= name_capacity) return false;

    memcpy(name, position, length);
    name[length] = 0;
    *file = position + length + 1;
    return true;
}

//Copies into newly allocated mem.
static char* copy_part(const char* text, int length) {
    char* copy = (char*)allocate_memory(length + 1);
    memcpy(copy, text, length);
    copy[length] = 0;
    return copy;
}

static void copy_part_to(char* destination, int capacity, const char* text, int length) {
    length = min_int(length, capacity - 1);
    memcpy(destination, text, length);
    destination[length] = 0;
}

static int read_number(const char* text, int length) {
    int value = 0;
    for (int i = 0; i < length && is_digit(text[i]); i++) value = value * 10 + (text[i] - '0');
    return value;
}

static bool is_choice_separator(const char* text, const char* end) {
    return text + 2 < end && text[0] == ' ' && text[1] == '|' && text[2] == ' ';
}

//first line | second line
static void add_line_choices(Character& character, int kind, const char* value, int length) {
    const int SEPARATOR_LENGTH = 3;
    const char* end = value + length;
    const char* start = value;
    while (start < end && character.line_count[kind] < MAX_LINE_CHOICES) {
        const char* stop = start;
        while (stop < end && !is_choice_separator(stop, end)) stop++;

        character.lines[kind][character.line_count[kind]] = copy_part(start, stop - start);
        character.line_count[kind]++;
        start = stop < end ? stop + SEPARATOR_LENGTH : end;
    }
}

static void read_setting(Character& character, const char* key, const char* value, int value_length) {
    if (texts_equal(key, "name")) {
        copy_part_to(character.name, sizeof(character.name), value, value_length);
    } else if (texts_equal(key, "title")) {
        copy_part_to(character.title, sizeof(character.title), value, value_length);
    } else if (texts_equal(key, "elo")) {
        character.elo = read_number(value, value_length);
    } else if (texts_equal(key, "max")) {
        character.max_strength = value_length > 0 && value[0] == '1';
    } else {
        for (int kind = 0; kind < LINE_KIND_COUNT; kind++) {
            if (texts_equal(key, LINE_KEYS[kind])) add_line_choices(character, kind, value, value_length);
        }
    }
}

static void read_character_file(Character& character, const char* text, u64 size) {
    const char* end = text + size;
    const char* line = text;
    while (line < end) {

        // find the end of this line then leave off space
        const char* line_end = line;
        while (line_end < end && *line_end != '\n') line_end++;
        const char* next_line = line_end < end ? line_end + 1 : end;
        while (line_end > line && (line_end[-1] == '\r' || line_end[-1] == ' ')) line_end--;

        const char* equals = line;
        while (equals < line_end && *equals != '=') equals++;

        //skip empty lines
        bool is_setting = line < line_end && equals < line_end;
        if (is_setting) {
            char key[24];
            copy_part_to(key, sizeof(key), line, equals - line);

            const char* value = equals + 1;
            while (value < line_end && *value == ' ') value++;
            read_setting(character, key, value, line_end - value);
        }
        line = next_line;
    }
}

int count_character_pictures() {
    int count = 0;
    for (int i = 0; i < asset_file_count(); i++) {
        const char* path = asset_file(i).path;
        if (!text_ends_with(path, ".bmp")) continue;
        if (find_text(path, CHARACTERS_FOLDER) != nullptr) count++;
    }
    return count;
}

static int strength_order(const Character& character) {
    const int MAX_STRENGTH_BONUS = 999999;
    return character.elo + (character.max_strength ? MAX_STRENGTH_BONUS : 0);
}

//Equal strength characters are sorted with build OS file sort
static void sort_characters_by_strength() {

    for (int i = 1; i < number_of_characters; i++) {

        Character current = characters[i];
        int j = i - 1;

        while (j >= 0 && strength_order(characters[j]) < strength_order(current)) {
            characters[j + 1] = characters[j];
            j--;
        }

        characters[j + 1] = current;
    }
}

void load_characters(void (*progress)(const char* file_name)) {
    characters = allocate_array<Character>(MAX_CHARACTERS);
    char folder[32];
    const char* file;

    //read character.txt
    for (int i = 0; i < asset_file_count(); i++) {
        const AssetFile& asset = asset_file(i);
        if (!split_path(asset.path, CHARACTERS_FOLDER, folder, sizeof(folder), &file)) continue;
        if (!texts_equal(file, "character.txt") || number_of_characters >= MAX_CHARACTERS) continue;
        Character& character = characters[number_of_characters];
        number_of_characters++;
        copy_text(character.folder, folder, sizeof(character.folder));
        copy_text(character.name, folder, sizeof(character.name));
        character.elo = DEFAULT_ELO;
        read_character_file(character, (const char*)asset.data, asset.size);
    }

    //read .bmps
    for (int i = 0; i < asset_file_count(); i++) {

        const AssetFile& asset = asset_file(i);
        if (!text_ends_with(asset.path, ".bmp")) continue;

        if (find_text(asset.path, CHARACTERS_FOLDER) == nullptr) continue;

        Bitmap picture;
        const char* error;

        if (!open_bitmap(asset.data, asset.size, PORTRAIT_FILE_SIZE, &picture, &error)) {
            Text message;
            message.add(asset.path).add(": ").add(error);
            kernel_panic(message.text);
        }

        if (split_path(asset.path, CHARACTERS_FOLDER, folder, sizeof(folder), &file)) {

            if (progress != nullptr) progress(find_text(asset.path, "/assets/") + text_length("/assets/"));

            for (int c = 0; c < number_of_characters; c++) {
                if (!texts_equal(characters[c].folder, folder)) continue;

                for (int face = 0; face < FACE_COUNT; face++) {
                    if (texts_equal(file, FACE_FILE_NAMES[face])) {
                        characters[c].faces[face] = picture;
                        characters[c].has_face[face] = true;
                    }
                }
            }
        }
    }

    sort_characters_by_strength();

    //replace missing .bmp with default. 
    for (int c = 0; c < number_of_characters; c++) {
        Character& character = characters[c];

        if (!character.has_face[FACE_DEFAULT]) {
            Text message;
            message.add(CHARACTERS_FOLDER).add(character.folder).add("/default.bmp is missing");
            kernel_panic(message.text); //Kernel panic if default is not found
        }

        for (int face = 1; face < FACE_COUNT; face++) {
            if (character.has_face[face]) continue;
            if (face == FACE_LAUGH2 && character.has_face[FACE_LAUGH1]) {
                character.faces[face] = character.faces[FACE_LAUGH1];
            } else {
                character.faces[face] = character.faces[FACE_DEFAULT];
            }
        }
    }
}
