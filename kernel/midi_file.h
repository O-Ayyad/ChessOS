#pragma once
#include "types.h"

enum MidiEventType {
    MIDI_NOTE_OFF,
    MIDI_NOTE_ON,
    MIDI_CHANGE_INSTRUMENT,
    MIDI_CHANGE_TEMPO,
    MIDI_CONTROLLER
};

struct MidiEvent {
    u32 time;
    u8 type;
    u8 channel;
    u8 value1;
    u8 value2;
    u32 microseconds_per_beat;
};

struct Song {
    MidiEvent* events;
    int event_count;
    int ticks_per_beat;
    char name[48];
};

bool read_midi_file(const u8* file, u64 file_size, const char* name, Song* song);
