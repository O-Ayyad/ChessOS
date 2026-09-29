#pragma once
#include "types.h"
#include "keyboard_mouse.h"

enum GameMode { MODE_VS_COMPUTER, MODE_VS_HUMAN };
enum ColorChoice { PLAY_WHITE, PLAY_BLACK, PLAY_RANDOM };

struct Settings {
    GameMode mode;
    int opponent;
    ColorChoice color;
    char player_name[2][16];
    int player_portrait[2];
    bool mate_faces;
    bool show_mates; 
    bool show_eval;
};
extern Settings settings;

void setup_menu(); //default settings
void draw_menu();
void menu_handle_key(const InputEvent& event);
void menu_handle_click(int column, int row);
