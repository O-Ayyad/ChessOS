#pragma once
#include "types.h"

// The compiler expects these four to exist
extern "C" {
void* memcpy(void* destination, const void* source, size_t byte_count);
void* memset(void* destination, int value, size_t byte_count);
void* memmove(void* destination, const void* source, size_t byte_count);
int memcmp(const void* a, const void* b, size_t byte_count);
}

int  text_length(const char* text);
bool texts_equal(const char* a, const char* b);
bool text_starts_with(const char* text, const char* start);
bool text_ends_with(const char* text, const char* ending);

const char* find_text(const char* text, const char* wanted);

void copy_text(char* destination, const char* source, int capacity);
char to_upper_case(char c);
char to_lower_case(char c);
bool is_digit(char c);


struct Text {
    static const int CAPACITY = 200;
    char text[CAPACITY];
    int length;

    Text();
    Text& add(const char* more);
    Text& add_char(char c);
    Text& add_number(i64 number);
    Text& add_hex(u64 number); 
    Text& add_decimal(float number); 
};
