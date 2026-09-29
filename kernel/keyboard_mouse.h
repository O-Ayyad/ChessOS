// keyboard_mouse.h - reading the keyboard and mouse.
//
// Both are "PS/2" devices behind the same little controller chip. We don't use
// interrupts: the main loop calls check_keyboard_and_mouse() very often, which
// reads any waiting bytes and turns them into "events" (key presses, clicks).
#pragma once
#include "types.h"

enum Key {
    KEY_NONE,
    KEY_CHARACTER,        // a normal character; see InputEvent.character
    KEY_ENTER, KEY_BACKSPACE, KEY_ESCAPE, KEY_TAB, KEY_DELETE,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F10
};

enum EventType { EVENT_KEY_PRESS, EVENT_MOUSE_PRESS, EVENT_MOUSE_RELEASE };
const int MOUSE_LEFT_BUTTON = 1;
const int MOUSE_RIGHT_BUTTON = 2;

struct InputEvent {
    EventType type;
    Key key;              // for key presses
    char character;       // for KEY_CHARACTER
    int button;           // for mouse presses/releases
};

void setup_keyboard_and_mouse();
void check_keyboard_and_mouse();          // call this often
bool next_input_event(InputEvent* event); // gives the next waiting event, if any

// Where the mouse pointer is, in canvas pixels (0..1023, 0..767)
int mouse_x();
int mouse_y();
bool mouse_has_moved();                  // true once after every movement
