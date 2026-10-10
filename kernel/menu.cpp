// On the left is the list of options. On the right is the chosen opponent
// with the "ladder" of all characters, or the players' own portraits.
#include "menu.h"
#include "ui.h"
#include "game.h"
#include "characters.h"
#include "sound.h"
#include "screen.h"
#include "timer.h"
#include "math_utils.h"
#include "text_utils.h"

Settings settings;

// The options from top to bottom. The two tables below are in the same order.
enum MenuOption {
    OPTION_MODE, OPTION_OPPONENT, OPTION_COLOR, OPTION_NAME_1, OPTION_PORTRAIT_1,
    OPTION_NAME_2, OPTION_PORTRAIT_2, OPTION_SHOW_EVAL, OPTION_MATE_FACES, OPTION_SHOW_EVAL_MATE,
    OPTION_MUSIC, OPTION_START, OPTION_COUNT
};

static const char* OPTION_LABELS[OPTION_COUNT] = {
    "MODE", "OPPONENT", "YOUR COLOR", "PLAYER 1 NAME", "PLAYER 1 PORTRAIT",
    "PLAYER 2 NAME", "PLAYER 2 PORTRAIT", "SHOW EVALUATION BAR", "MATE FACES", "SHOW MATE IN X", "MUSIC", ""
};

static const char* OPTION_DESCRIPTIONS[OPTION_COUNT] = {
    "Play against a computer character or against a friend on the same computer.",
    "Who you play against. The MAX deities play as well as ChessOS can.",
    "RANDOM lets the computer pick your color.",
    "Your name.",
    "Your picture: pick any character's face with the left and right arrow keys.",
    "The second player's name.",
    "The second player's picture: any character's face.",
    "Toggle the evaluation bar showing in game. It is still used in the background.",
    "When ON, opponents will reveal when a forced checkmate appears on the board with their portrait. Turn it OFF if the faces give away too much.",
    "When ON, evaluation bar shows mate in turns if forced checkmate exists.",
    "Plays the songs in the music folder. F2 turns music on or off at any time and F3 skips to the next song.",
    "Start the game. Press F10 to return to this menu.",
};

// Columns and rows of the text grid
const int MENU_LEFT = 6;            //column of the option names
const int MENU_VALUE_COLUMN = 30;   //column of the values
const int MENU_FIRST_ROW = 10;
const int MENU_ROW_SPACING = 2;
const int NAME_MAX_LENGTH = 14;
const int RIGHT_SIDE = 76;          //the portrait and ladder on the right
const int LADDER_FIRST_ROW = 25;
const int LADDER_ROWS = 22;
const int DESCRIPTION_ROW = 38;
const int DESCRIPTION_WIDTH = 64;
const int COLOR_CHOICE_COUNT = 3;   //white, black, random

static int focused_option = OPTION_OPPONENT;

void setup_menu() {
    settings.mode = MODE_VS_COMPUTER;
    settings.color = PLAY_RANDOM;
    settings.show_eval = true;
    settings.mate_faces = true;
    settings.show_mates = true;

    copy_text(settings.player_name[0], "PLAYER 1", sizeof(settings.player_name[0]));
    copy_text(settings.player_name[1], "PLAYER 2", sizeof(settings.player_name[1]));

    settings.player_portrait[0] = max_int(0, character_count() - (character_count())/2);
    settings.player_portrait[1] = max_int(0, character_count() - (character_count())/2)-1;

    //the default opponent is the strongest one that is still friendly to a beginner
    settings.opponent = 0;
    const int FRIENDLY_ELO = 1400;
    for (int i = 0; i < character_count(); i++) {
        if (get_character(i).elo <= FRIENDLY_ELO && !get_character(i).max_strength) {
            settings.opponent = i;
            break;
        }
    }
}

//Some options only make sense against the computer, others only with two humans.
static bool option_is_shown(int option) {
    if (settings.mode == MODE_VS_COMPUTER) {
        return option != OPTION_NAME_2 && option != OPTION_PORTRAIT_2;
    }
    return option != OPTION_OPPONENT && option != OPTION_COLOR && option != OPTION_MATE_FACES;
}

static int row_of_option(int option) {
    int row = MENU_FIRST_ROW;
    for (int i = 0; i < option; i++) {
        if (option_is_shown(i)) row += MENU_ROW_SPACING;
    }
    return row;
}

static bool is_name_option(int option) {
    return option == OPTION_NAME_1 || option == OPTION_NAME_2;
}

// The options about a player's own name and portrait
static bool is_player_option(int option) {
    return option == OPTION_NAME_1 || option == OPTION_PORTRAIT_1 ||
           option == OPTION_NAME_2 || option == OPTION_PORTRAIT_2;
}

static int player_of_option(int option) {
    return (option == OPTION_NAME_2 || option == OPTION_PORTRAIT_2) ? 1 : 0;
}

static int first_character_in_ladder() {
    return clamp_int(settings.opponent - LADDER_ROWS / 2, 0, max_int(0, character_count() - LADDER_ROWS));
}

static void draw_opponent_side() {
    Character& opponent = get_character(settings.opponent);
    bool scared = focused_option == OPTION_MATE_FACES;
    bool on_start = focused_option == OPTION_START;

    int face = FACE_DEFAULT;

    if(scared){
        face = FACE_SCARED;
    }else if (on_start){
        face = FACE_VICTORY;
    }

    draw_portrait(opponent.faces[face], RIGHT_SIDE, 8);

    write_text(RIGHT_SIDE + 26, 9, opponent.name, COLOR_WHITE);
    Text elo;
    elo.add("ELO ").add_number(opponent.elo);
    if (opponent.max_strength) elo.add("  MAX");
    write_text(RIGHT_SIDE + 26, 10, elo.text, opponent.max_strength ? COLOR_YELLOW : COLOR_LIGHT_GRAY);
    write_wrapped_text(RIGHT_SIDE + 26, 11, 24, 3, opponent.title, COLOR_DARK_GRAY);

    const char* greeting = opponent.line_count[LINE_GREET] > 0 ? opponent.lines[LINE_GREET][0] : nullptr;
    if (greeting != nullptr) {
        write_text(RIGHT_SIDE, 21, "\"", COLOR_DARK_GRAY);
        write_wrapped_text(RIGHT_SIDE + 1, 21, 49, 2, greeting, COLOR_LIGHT_CYAN);
    }

    write_text(RIGHT_SIDE, 24, "LADDER", COLOR_DARK_GRAY);
    draw_dashes(RIGHT_SIDE + 7, 24, 43, COLOR_DIM_GRAY);
    int first = first_character_in_ladder();
    for (int i = first; i < character_count() && i < first + LADDER_ROWS; i++) {
        Character& character = get_character(i);
        bool chosen = i == settings.opponent;
        int row = LADDER_FIRST_ROW + i - first;
        Text name;
        name.add(chosen ? "> " : "  ").add(character.name);
        write_text(RIGHT_SIDE, row, name.text, chosen ? COLOR_WHITE : COLOR_LIGHT_GRAY);
        Text elo_text;
        if (character.max_strength) elo_text.add("MAX ");
        elo_text.add_number(character.elo);
        write_text(RIGHT_SIDE + 50 - elo_text.length, row, elo_text.text, chosen ? COLOR_YELLOW : COLOR_DARK_GRAY);
    }
}

static void write_option_value(int option, Text& value, bool focused) {
    const int CURSOR_BLINK_MILLISECONDS = 500;
    bool blink_on = (milliseconds_since_start() / CURSOR_BLINK_MILLISECONDS) % 2 == 0;

    if (option == OPTION_MODE) value.add(settings.mode == MODE_VS_COMPUTER ? "VS COMPUTER" : "VS HUMAN");
    if (option == OPTION_OPPONENT) {
        Character& opponent = get_character(settings.opponent);
        value.add(opponent.name).add("  ").add_number(opponent.elo);
        if (opponent.max_strength) value.add(" MAX");
    }
    if (option == OPTION_COLOR) {
        if (settings.color == PLAY_WHITE) value.add("WHITE");
        if (settings.color == PLAY_BLACK) value.add("BLACK");
        if (settings.color == PLAY_RANDOM) value.add("RANDOM");
    }
    if (is_name_option(option)) {
        value.add(settings.player_name[player_of_option(option)]);
        if (focused && blink_on) value.add_char('_');     // a blinking text cursor
    }
    if (option == OPTION_PORTRAIT_1 || option == OPTION_PORTRAIT_2) {
        value.add(get_character(settings.player_portrait[player_of_option(option)]).name);
    }
    if (option == OPTION_SHOW_EVAL) value.add(settings.show_eval ? "ON" : "OFF");
    if (option == OPTION_MATE_FACES) value.add(settings.mate_faces ? "ON" : "OFF");
    if (option == OPTION_SHOW_EVAL_MATE) {
        if (!settings.show_eval) value.add("-");
        else value.add(settings.show_mates ? "ON" : "OFF");
    }
    if (option == OPTION_MUSIC) value.add(is_music_playing() ? "ON" : "OFF");
}

// The right side while a player name
static void draw_player_side() {
    int player = player_of_option(focused_option);
    draw_portrait(get_character(settings.player_portrait[player]).faces[FACE_DEFAULT], RIGHT_SIDE, 8);
    write_text(RIGHT_SIDE + 26, 9, settings.player_name[player], COLOR_WHITE);
    write_text(RIGHT_SIDE + 26, 10, player == 0 ? "PLAYER 1" : "PLAYER 2", COLOR_DARK_GRAY);
    if (settings.mode == MODE_VS_HUMAN) {
        int other = 1 - player;
        draw_portrait(get_character(settings.player_portrait[other]).faces[FACE_DEFAULT], RIGHT_SIDE, 23);
        write_text(RIGHT_SIDE + 26, 24, settings.player_name[other], COLOR_LIGHT_GRAY);
        write_text(RIGHT_SIDE + 26, 25, other == 0 ? "PLAYER 1" : "PLAYER 2", COLOR_DARK_GRAY);
    }
}

const char* ART_CREDIT = "art: linktr.ee / Paledoptera";
const int CREDITS_COLUMN = 76;
const char* START_LABEL = " START GAME ";

void draw_menu() {
    fill_rectangle(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, COLOR_BLACK);
    draw_text(canvas, column_x(MENU_LEFT), row_y(2), "CHESSOS", COLOR_WHITE, 4);
    write_text(MENU_LEFT, 6, "Chess without OS bloat", COLOR_DARK_GRAY);
    write_text(CREDITS_COLUMN, 6, ART_CREDIT, COLOR_WHITE);
    write_text(MENU_LEFT, 8, "NEW GAME", COLOR_LIGHT_GRAY);
    draw_dashes(MENU_LEFT + 9, 8, 50, COLOR_DIM_GRAY);

    //the options
    for (int option = 0; option < OPTION_COUNT; option++) {
        if (!option_is_shown(option)) continue;
        int row = row_of_option(option);
        bool focused = option == focused_option;

        // the start button is drawn differently from the other options
        if (option == OPTION_START) {
            if (focused) {
                draw_text_with_background(canvas, column_x(MENU_LEFT + 2), row_y(row), START_LABEL, COLOR_BLACK, COLOR_WHITE);
            } else {
                write_text(MENU_LEFT + 2, row, START_LABEL, COLOR_LIGHT_GRAY);
            }
            continue;
        }

        write_text(MENU_LEFT, row, focused ? ">" : " ", COLOR_LIGHT_GREEN);
        write_text(MENU_LEFT + 2, row, OPTION_LABELS[option], focused ? COLOR_WHITE : COLOR_DARK_GRAY);
        Text value;
        write_option_value(option, value, focused);
        write_text(MENU_VALUE_COLUMN, row, value.text, focused ? COLOR_WHITE : COLOR_LIGHT_GRAY);

        // < and > show that left/right changes this value (names are typed instead)
        if (focused && !is_name_option(option)) {
            write_text(MENU_VALUE_COLUMN - 2, row, "<", COLOR_LIGHT_GREEN);
            write_text(MENU_VALUE_COLUMN + value.length + 1, row, ">", COLOR_LIGHT_GREEN);
        }
    }

    // the description of the focused option bottom left
    draw_dashes(MENU_LEFT, DESCRIPTION_ROW - 1, DESCRIPTION_WIDTH, COLOR_DIM_GRAY);
    const char* label = focused_option == OPTION_START ? "START GAME" : OPTION_LABELS[focused_option];
    write_text(MENU_LEFT, DESCRIPTION_ROW, label, COLOR_YELLOW);
    write_wrapped_text(MENU_LEFT, DESCRIPTION_ROW + 1, DESCRIPTION_WIDTH, 4, OPTION_DESCRIPTIONS[focused_option], COLOR_LIGHT_GRAY);

    // the right side
    if (settings.mode == MODE_VS_COMPUTER && !is_player_option(focused_option)) {
        draw_opponent_side();
    } else {
        draw_player_side();
    }

    // the help lines at the bottom
    Text help;
    help.add_char(CHAR_ARROW_UP).add_char(CHAR_ARROW_DOWN).add(" choose  ");
    help.add_char(CHAR_ARROW_LEFT).add_char(CHAR_ARROW_RIGHT).add(" change  type names  ENTER start");
    write_text(MENU_LEFT, 45, help.text, COLOR_DARK_GRAY);
    Text sound_line;
    sound_line.add("   music: ").add(current_song_name());
    write_text(MENU_LEFT, 46, sound_line.text, COLOR_DIM_GRAY);
}

static void change_option(int direction) {
    if (focused_option == OPTION_MODE) {
        settings.mode = settings.mode == MODE_VS_COMPUTER ? MODE_VS_HUMAN : MODE_VS_COMPUTER;

    } else if (focused_option == OPTION_OPPONENT) {
        settings.opponent = wrap_around(settings.opponent + direction, character_count());

    } else if (focused_option == OPTION_COLOR) {
        settings.color = (ColorChoice)wrap_around(settings.color + direction, COLOR_CHOICE_COUNT);

    } else if (focused_option == OPTION_PORTRAIT_1) {
        settings.player_portrait[0] = wrap_around(settings.player_portrait[0] + direction, character_count());

    } else if (focused_option == OPTION_PORTRAIT_2) {
        settings.player_portrait[1] = wrap_around(settings.player_portrait[1] + direction, character_count());

    } else if (focused_option == OPTION_MATE_FACES) {
        settings.mate_faces = !settings.mate_faces;

    } else if (focused_option == OPTION_SHOW_EVAL) {
        settings.show_eval = !settings.show_eval;
        settings.show_mates = settings.show_eval;

    } else if (focused_option == OPTION_SHOW_EVAL_MATE) {
        if (!settings.show_eval) return;
        settings.show_mates = !settings.show_mates;

    } else if (focused_option == OPTION_MUSIC) {
        set_music_playing(!is_music_playing());

    } else {
        return;
    }
    play_sound_effect(SOUND_MENU);
    screen_needs_redraw = true;
}

static void move_focus(int direction) {
    do {
        focused_option = wrap_around(focused_option + direction, OPTION_COUNT);
    } while (!option_is_shown(focused_option));
    play_sound_effect(SOUND_MENU);
    screen_needs_redraw = true;
}

void menu_handle_key(const InputEvent& event) {
    bool typing_name = is_name_option(focused_option);
    char* name = settings.player_name[player_of_option(focused_option)];
    int name_length = text_length(name);

    if (event.key == KEY_UP) move_focus(-1);
    if (event.key == KEY_DOWN || event.key == KEY_TAB) move_focus(1);
    if (event.key == KEY_LEFT) change_option(-1);
    if (event.key == KEY_RIGHT) change_option(1);
    if (event.key == KEY_ENTER) {
        if (focused_option == OPTION_START) start_new_game();
        else if (typing_name) move_focus(1);
        else change_option(1);
    }
    if (event.key == KEY_BACKSPACE && typing_name && name_length > 0) {
        name[name_length - 1] = 0;
        screen_needs_redraw = true;
    }
    if (event.key == KEY_CHARACTER) {
        bool printable = event.character >= ' ' && event.character <= '~';
        if (typing_name && printable && name_length < NAME_MAX_LENGTH) {
            name[name_length] = to_upper_case(event.character);
            name[name_length + 1] = 0;
            screen_needs_redraw = true;
        } else if (!typing_name && event.character == ' ') {
            change_option(1);
        }
    }
    if (event.key == KEY_F2) {
        set_music_playing(!is_music_playing());
        screen_needs_redraw = true;
    }
    if (event.key == KEY_F3) {
        start_next_song();
        screen_needs_redraw = true;
    }
}
void menu_handle_click(int column, int row) {
    // a click on an option on the left: the first click gives it the focus,
    // another click changes its value
    bool on_left_side = column < RIGHT_SIDE;
    for (int option = 0; option < OPTION_COUNT && on_left_side; option++) {
        if (!option_is_shown(option) || row != row_of_option(option)) continue;

        if (option == OPTION_START) {
            if(focused_option == OPTION_START){
                start_new_game();
            }else{
                focused_option = OPTION_START;
                play_sound_effect(SOUND_MENU);
            }


        } else if (focused_option == option && !is_name_option(option)) {
            change_option(column < MENU_VALUE_COLUMN - 1 ? -1 : 1);     // the < side or the > side
        } else {
            focused_option = option;
            play_sound_effect(SOUND_MENU);
            screen_needs_redraw = true;
        }
        return;
    }

    // a click on a name in the ladder picks that opponent
    bool in_ladder = !on_left_side && row >= LADDER_FIRST_ROW && row < LADDER_FIRST_ROW + LADDER_ROWS;
    bool ladder_showing = settings.mode == MODE_VS_COMPUTER && !is_player_option(focused_option);
    if (in_ladder && ladder_showing) {
        int index = first_character_in_ladder() + row - LADDER_FIRST_ROW;
        if (index < character_count()) {
            settings.opponent = index;
            focused_option = OPTION_OPPONENT;
            play_sound_effect(SOUND_MENU);
            screen_needs_redraw = true;
        }
    }
}
