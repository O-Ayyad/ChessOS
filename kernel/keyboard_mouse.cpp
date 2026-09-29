// keyboard_mouse.cpp - see keyboard_mouse.h
#include "keyboard_mouse.h"
#include "cpu.h"
#include "screen.h"
#include "math_utils.h"

// ---- the PS/2 controller ----
const u16 PS2_DATA_PORT = 0x60;
const u16 PS2_STATUS_PORT = 0x64;       // reading gives the status ...
const u16 PS2_COMMAND_PORT = 0x64;      // ... writing sends a command
const u8 STATUS_BYTE_WAITING = 0x01;
const u8 STATUS_BUSY = 0x02;
const u8 STATUS_BYTE_IS_FROM_MOUSE = 0x20;

const u8 COMMAND_READ_CONFIG = 0x20;
const u8 COMMAND_WRITE_CONFIG = 0x60;
const u8 COMMAND_DISABLE_MOUSE = 0xA7;
const u8 COMMAND_ENABLE_MOUSE = 0xA8;
const u8 COMMAND_DISABLE_KEYBOARD = 0xAD;
const u8 COMMAND_ENABLE_KEYBOARD = 0xAE;
const u8 COMMAND_SEND_TO_MOUSE = 0xD4;
const u8 CONFIG_KEYBOARD_INTERRUPT = 0x01;
const u8 CONFIG_MOUSE_INTERRUPT = 0x02;
const u8 CONFIG_KEYBOARD_CLOCK_OFF = 0x10;
const u8 CONFIG_MOUSE_CLOCK_OFF = 0x20;
const u8 CONFIG_TRANSLATE_SCANCODES = 0x40;
const u8 MOUSE_USE_DEFAULTS = 0xF6;
const u8 MOUSE_START_SENDING = 0xF4;
const int WAIT_TRIES = 100000;

static void wait_until_controller_ready() {
    for (int i = 0; i < WAIT_TRIES; i++) {
        if ((read_port_8(PS2_STATUS_PORT) & STATUS_BUSY) == 0) return;
    }
}

static u8 wait_and_read_byte() {
    for (int i = 0; i < WAIT_TRIES; i++) {
        if (read_port_8(PS2_STATUS_PORT) & STATUS_BYTE_WAITING) break;
    }
    return read_port_8(PS2_DATA_PORT);
}

static void send_command(u8 command) {
    wait_until_controller_ready();
    write_port_8(PS2_COMMAND_PORT, command);
}

static void send_data(u8 data) {
    wait_until_controller_ready();
    write_port_8(PS2_DATA_PORT, data);
}

static void send_to_mouse(u8 value) {
    send_command(COMMAND_SEND_TO_MOUSE);
    send_data(value);
    wait_and_read_byte();          // the mouse answers 0xFA ("acknowledged")
}

static void throw_away_waiting_bytes() {
    for (int i = 0; i < 64; i++) {
        if ((read_port_8(PS2_STATUS_PORT) & STATUS_BYTE_WAITING) == 0) return;
        read_port_8(PS2_DATA_PORT);
    }
}

// ---- the queue of events waiting to be handled ----
const int QUEUE_SIZE = 256;
static InputEvent queue[QUEUE_SIZE];
static int queue_first = 0;    // the oldest event
static int queue_count = 0;

static void add_event(InputEvent event) {
    if (queue_count == QUEUE_SIZE) return;     // full: drop it
    queue[(queue_first + queue_count) % QUEUE_SIZE] = event;
    queue_count++;
}

bool next_input_event(InputEvent* event) {
    if (queue_count == 0) return false;
    *event = queue[queue_first];
    queue_first = (queue_first + 1) % QUEUE_SIZE;
    queue_count--;
    return true;
}

static void add_key_event(Key key, char character) {
    InputEvent event;
    event.type = EVENT_KEY_PRESS;
    event.key = key;
    event.character = character;
    event.button = 0;
    add_event(event);
}

static void add_mouse_event(EventType type, int button) {
    InputEvent event;
    event.type = type;
    event.key = KEY_NONE;
    event.character = 0;
    event.button = button;
    add_event(event);
}

// ---------------------------------------------------------------- the keyboard
// The keyboard sends a "scancode" number for every key press, and the same
// number + 0x80 when the key is let go. Some keys (like the arrows) first send
// an extra 0xE0 byte.
const u8 SCANCODE_EXTENDED = 0xE0;
const u8 SCANCODE_RELEASED = 0x80;
const u8 SCANCODE_ESCAPE = 0x01, SCANCODE_BACKSPACE = 0x0E, SCANCODE_TAB = 0x0F, SCANCODE_ENTER = 0x1C;
const u8 SCANCODE_LEFT_SHIFT = 0x2A, SCANCODE_RIGHT_SHIFT = 0x36;
const u8 SCANCODE_F1 = 0x3B, SCANCODE_F2 = 0x3C, SCANCODE_F3 = 0x3D, SCANCODE_F4 = 0x3E;
const u8 SCANCODE_F5 = 0x3F, SCANCODE_F10 = 0x44;
const u8 SCANCODE_UP = 0x48, SCANCODE_LEFT = 0x4B, SCANCODE_RIGHT = 0x4D, SCANCODE_DOWN = 0x50;
const u8 SCANCODE_DELETE = 0x53, SCANCODE_SLASH = 0x35;

// Which character each scancode types (0 = not a character key)
static const char KEY_CHARACTERS[64] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,   0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,   0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`', 0,   '\\','z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0 };
static const char KEY_CHARACTERS_WITH_SHIFT[64] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,   0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,   0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0 };

static bool shift_held = false;
static bool next_byte_is_extended = false;

static void handle_keyboard_byte(u8 byte) {
    if (byte == SCANCODE_EXTENDED) {
        next_byte_is_extended = true;
        return;
    }
    bool released = (byte & SCANCODE_RELEASED) != 0;
    u8 scancode = byte & ~SCANCODE_RELEASED;
    bool extended = next_byte_is_extended;
    next_byte_is_extended = false;

    if (scancode == SCANCODE_LEFT_SHIFT || scancode == SCANCODE_RIGHT_SHIFT) {
        shift_held = !released;
        return;
    }
    if (released) return;                       // we only care about key presses

    // keys that are the same with or without the 0xE0 byte
    if (scancode == SCANCODE_UP)    { add_key_event(KEY_UP, 0); return; }
    if (scancode == SCANCODE_DOWN)  { add_key_event(KEY_DOWN, 0); return; }
    if (scancode == SCANCODE_LEFT)  { add_key_event(KEY_LEFT, 0); return; }
    if (scancode == SCANCODE_RIGHT) { add_key_event(KEY_RIGHT, 0); return; }
    if (scancode == SCANCODE_ENTER) { add_key_event(KEY_ENTER, 0); return; }

    if (extended) {
        if (scancode == SCANCODE_DELETE) add_key_event(KEY_DELETE, 0);
        if (scancode == SCANCODE_SLASH)  add_key_event(KEY_CHARACTER, '/');   // the keypad '/'
        return;
    }

    if (scancode == SCANCODE_ESCAPE)    { add_key_event(KEY_ESCAPE, 0); return; }
    if (scancode == SCANCODE_BACKSPACE) { add_key_event(KEY_BACKSPACE, 0); return; }
    if (scancode == SCANCODE_TAB)       { add_key_event(KEY_TAB, 0); return; }
    if (scancode == SCANCODE_F1)        { add_key_event(KEY_F1, 0); return; }
    if (scancode == SCANCODE_F2)        { add_key_event(KEY_F2, 0); return; }
    if (scancode == SCANCODE_F3)        { add_key_event(KEY_F3, 0); return; }
    if (scancode == SCANCODE_F4)        { add_key_event(KEY_F4, 0); return; }
    if (scancode == SCANCODE_F5)        { add_key_event(KEY_F5, 0); return; }
    if (scancode == SCANCODE_F10)       { add_key_event(KEY_F10, 0); return; }

    if (scancode < 64) {
        char character = shift_held ? KEY_CHARACTERS_WITH_SHIFT[scancode] : KEY_CHARACTERS[scancode];
        if (character != 0) {
            add_key_event(KEY_CHARACTER, character);
        }
    }
}

// ---------------------------------------------------------------- the mouse
// The mouse sends 3 bytes every time it moves or a button changes:
//   byte 0: buttons and sign bits, byte 1: X movement, byte 2: Y movement
const u8 MOUSE_ALWAYS_ONE = 0x08;       // this bit of byte 0 is always 1 (helps us stay in step)
const u8 MOUSE_X_NEGATIVE = 0x10;
const u8 MOUSE_Y_NEGATIVE = 0x20;
const u8 MOUSE_OVERFLOW = 0xC0;
const u8 MOUSE_BUTTON_BITS = 0x07;

static u8 mouse_packet[3];
static int mouse_packet_length = 0;
static int buttons_down = 0;
static int pointer_x = CANVAS_WIDTH / 2;
static int pointer_y = CANVAS_HEIGHT / 2;
static bool pointer_moved = false;

static void handle_mouse_byte(u8 byte) {
    if (mouse_packet_length == 0 && (byte & MOUSE_ALWAYS_ONE) == 0) {
        return;                                 // out of step: wait for a real first byte
    }
    mouse_packet[mouse_packet_length] = byte;
    mouse_packet_length++;
    if (mouse_packet_length < 3) return;
    mouse_packet_length = 0;

    u8 flags = mouse_packet[0];
    if (flags & MOUSE_OVERFLOW) return;

    // the movement is a 9-bit number: 8 bits in bytes 1/2 plus a sign bit in byte 0
    int move_x = mouse_packet[1];
    int move_y = mouse_packet[2];
    if (flags & MOUSE_X_NEGATIVE) move_x -= 256;
    if (flags & MOUSE_Y_NEGATIVE) move_y -= 256;
    if (move_x != 0 || move_y != 0) {
        pointer_x = clamp_int(pointer_x + move_x, 0, CANVAS_WIDTH - 1);
        pointer_y = clamp_int(pointer_y - move_y, 0, CANVAS_HEIGHT - 1);   // the mouse counts "up" as positive
        pointer_moved = true;
    }

    int buttons_now = flags & MOUSE_BUTTON_BITS;
    for (int button = MOUSE_LEFT_BUTTON; button <= MOUSE_RIGHT_BUTTON; button++) {
        bool was_down = (buttons_down & button) != 0;
        bool is_down = (buttons_now & button) != 0;
        if (is_down && !was_down) add_mouse_event(EVENT_MOUSE_PRESS, button);
        if (!is_down && was_down) add_mouse_event(EVENT_MOUSE_RELEASE, button);
    }
    buttons_down = buttons_now;
}

int mouse_x() { return pointer_x; }
int mouse_y() { return pointer_y; }

bool mouse_has_moved() {
    bool moved = pointer_moved;
    pointer_moved = false;
    return moved;
}

// ---------------------------------------------------------------- setup and polling
void setup_keyboard_and_mouse() {
    send_command(COMMAND_DISABLE_KEYBOARD);
    send_command(COMMAND_DISABLE_MOUSE);
    throw_away_waiting_bytes();

    send_command(COMMAND_READ_CONFIG);
    u8 config = wait_and_read_byte();
    config &= ~(CONFIG_KEYBOARD_INTERRUPT | CONFIG_MOUSE_INTERRUPT);       // we poll instead
    config &= ~(CONFIG_KEYBOARD_CLOCK_OFF | CONFIG_MOUSE_CLOCK_OFF);
    config |= CONFIG_TRANSLATE_SCANCODES;                                // gives us the classic scancodes
    send_command(COMMAND_WRITE_CONFIG);
    send_data(config);

    send_command(COMMAND_ENABLE_KEYBOARD);
    send_command(COMMAND_ENABLE_MOUSE);
    send_to_mouse(MOUSE_USE_DEFAULTS);
    send_to_mouse(MOUSE_START_SENDING);
    throw_away_waiting_bytes();
}

void check_keyboard_and_mouse() {
    for (int i = 0; i < 64; i++) {
        u8 status = read_port_8(PS2_STATUS_PORT);
        if ((status & STATUS_BYTE_WAITING) == 0) return;
        u8 byte = read_port_8(PS2_DATA_PORT);
        if (status & STATUS_BYTE_IS_FROM_MOUSE) {
            handle_mouse_byte(byte);
        } else {
            handle_keyboard_byte(byte);
        }
    }
}
