#pragma once
#include "types.h"

enum Key {
    KEY_NONE,
    KEY_CHARACTER,
    KEY_ENTER, KEY_BACKSPACE, KEY_ESCAPE, KEY_TAB, KEY_DELETE,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F10
};

enum EventType { EVENT_KEY_PRESS, EVENT_MOUSE_PRESS, EVENT_MOUSE_RELEASE };
const int MOUSE_LEFT_BUTTON = 1;
const int MOUSE_RIGHT_BUTTON = 2;

struct InputEvent {
    EventType type;
    Key key;
    char character;
    int button;
};

void setup_keyboard_and_mouse();
void check_keyboard_and_mouse(); 
bool next_input_event(InputEvent* event);//false if nothing happened
int mouse_x();
int mouse_y();
bool mouse_has_moved(); 
