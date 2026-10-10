#pragma once
#include "types.h"

enum MidiEventType {
    MIDI_NOTE_OFF,
    MIDI_NOTE_ON,
    MIDI_CHANGE_INSTRUMENT,
    MIDI_CHANGE_TEMPO,
    MIDI_CONTROLLER
};

//One thing that happens in a song.
struct MidiEvent {
    u32 time;                       // when: in ticks from the start of the song
    u8 type;                        // a MidiEventType
    u8 channel;                     // 0 to 15: each channel has its own instrument
    u8 value1;                      // the note, the instrument or the controller number
    u8 value2;                      // how hard the note is hit, or the controller's new value
    u32 microseconds_per_beat;      // for MIDI_CHANGE_TEMPO
};

struct Song {
    //in the order they happen
    MidiEvent* events;
    int event_count;
    int ticks_per_beat;
    char name[48];
};

// Returns false if the file cannot be read as a song.
bool read_midi_file(const u8* file, u64 file_size, const char* name, Song* song);
