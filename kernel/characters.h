#pragma once
#include "types.h"
#include "bitmap.h"

//All portraits are strict 64x64 files
enum Face { FACE_DEFAULT, FACE_SAD, FACE_HAPPY, FACE_VICTORY, FACE_DEFEAT, FACE_SCARED, FACE_SHOCKED,
            FACE_LAUGH1, FACE_LAUGH2, FACE_COUNT };

enum LineKind { LINE_GREET, LINE_TAUNT, LINE_HUMBLE, LINE_SMIRK, LINE_VICTORY, LINE_SCARED, LINE_DEFEAT,
                LINE_SHOCKED, LINE_WIN, LINE_LOSE, LINE_DRAW, LINE_ROOK_SACRIFICE, LINE_ROOK_BLUNDER, LINE_KIND_COUNT };

const int MAX_LINE_CHOICES = 8;

struct Character {
    char folder[32];
    char name[32];
    char title[48];
    int elo;
    bool max_strength;
    bool has_rook_lines;
    Bitmap faces[FACE_COUNT];
    bool has_face[FACE_COUNT];
    const char* lines[LINE_KIND_COUNT][MAX_LINE_CHOICES];
    int line_count[LINE_KIND_COUNT];
};

void load_characters(void (*progress)(const char* file_name));
int count_character_pictures(); 

int character_count();
Character& get_character(int index);



const char* pick_line(const Character& character, LineKind kind);

