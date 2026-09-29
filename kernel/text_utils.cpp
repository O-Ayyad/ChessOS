#include "text_utils.h"

extern "C" {

void* memcpy(void* destination, const void* source, size_t byte_count) {

    void* start = destination;
    size_t eight_byte_steps = byte_count / 8;
    size_t leftover = byte_count % 8;
    asm volatile("rep movsq" : "+D"(destination), "+S"(source), "+c"(eight_byte_steps) : : "memory");
    asm volatile("rep movsb" : "+D"(destination), "+S"(source), "+c"(leftover) : : "memory");
    return start;
}

void* memset(void* destination, int value, size_t byte_count) {
    void* start = destination;
    u64 eight_copies = 0x0101010101010101ULL * (u8)value;
    size_t eight_byte_steps = byte_count / 8;
    size_t leftover = byte_count % 8;
    asm volatile("rep stosq" : "+D"(destination), "+c"(eight_byte_steps) : "a"(eight_copies) : "memory");
    asm volatile("rep stosb" : "+D"(destination), "+c"(leftover) : "a"(eight_copies) : "memory");
    return start;
}

void* memmove(void* destination, const void* source, size_t byte_count) {
    u8* to = (u8*)destination;
    const u8* from = (const u8*)source;
    if (to < from) {
        for (size_t i = 0; i < byte_count; i++) {
            to[i] = from[i];
        }
    } else {
        for (size_t i = byte_count; i > 0; i--) {
            to[i - 1] = from[i - 1];
        }
    }
    return destination;
}

int memcmp(const void* a, const void* b, size_t byte_count) {
    const u8* x = (const u8*)a;
    const u8* y = (const u8*)b;
    for (size_t i = 0; i < byte_count; i++) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

void __cxa_pure_virtual() { while (true) {} }
int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void* __dso_handle = nullptr;

}

int text_length(const char* text) {
    int length = 0;
    while (text[length] != 0) {
        length++;
    }
    return length;
}

bool texts_equal(const char* a, const char* b) {
    int i = 0;
    while (a[i] != 0 && a[i] == b[i]) {
        i++;
    }
    return a[i] == b[i];
}

bool text_starts_with(const char* text, const char* start) {
    for (int i = 0; start[i] != 0; i++) {
        if (text[i] != start[i]) {
            return false;
        }
    }
    return true;
}

bool text_ends_with(const char* text, const char* ending) {
    int text_len = text_length(text);
    int ending_len = text_length(ending);
    if (ending_len > text_len) {
        return false;
    }
    return texts_equal(text + text_len - ending_len, ending);
}

const char* find_text(const char* text, const char* wanted) {
    for (int start = 0; text[start] != 0; start++) {
        if (text_starts_with(text + start, wanted)) {
            return text + start;
        }
    }
    return nullptr;
}

void copy_text(char* destination, const char* source, int capacity) {
    int i = 0;
    while (source[i] != 0 && i < capacity - 1) {
        destination[i] = source[i];
        i++;
    }
    destination[i] = 0;
}

char to_upper_case(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 'A';
    }
    return c;
}

char to_lower_case(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 'a';
    }
    return c;
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

Text::Text() {
    length = 0;
    text[0] = 0;
}

Text& Text::add_char(char c) {
    if (length < CAPACITY - 1) {
        text[length] = c;
        length++;
        text[length] = 0;
    }
    return *this;
}

Text& Text::add(const char* more) {
    for (int i = 0; more[i] != 0; i++) {
        add_char(more[i]);
    }
    return *this;
}

Text& Text::add_number(i64 number) {
    if (number < 0) {
        add_char('-');
        number = -number;
    }
    
    char digits[24];
    int count = 0;
    do {
        digits[count] = '0' + (number % 10);
        count++;
        number = number / 10;
    } while (number > 0);
    while (count > 0) {
        count--;
        add_char(digits[count]);
    }
    return *this;
}

Text& Text::add_hex(u64 number) {
    const char* hex_digits = "0123456789abcdef";
    add("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        add_char(hex_digits[(number >> shift) & 0xF]);
    }
    return *this;
}

Text& Text::add_decimal(float number) {
    if (number < 0) {
        add_char('-');
        number = -number;
    }
    i64 tenths = (i64)(number * 10 + 0.5f);
    add_number(tenths / 10);
    add_char('.');
    add_char('0' + tenths % 10);
    return *this;
}
