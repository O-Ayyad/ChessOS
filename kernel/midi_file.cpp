#include "midi_file.h"
#include "memory.h"
#include "text_utils.h"

//This file is written mostly by claude

const int DEFAULT_TICKS_PER_BEAT = 96;
const u32 SMPTE_TIMING_FLAG = 0x8000;

const u8 STATUS_NOTE_OFF = 0x80;
const u8 STATUS_NOTE_ON = 0x90;
const u8 STATUS_CONTROLLER = 0xB0;
const u8 STATUS_PROGRAM_CHANGE = 0xC0;
const u8 STATUS_CHANNEL_PRESSURE = 0xD0;
const u8 STATUS_SYSEX_START = 0xF0;
const u8 STATUS_SYSEX_CONTINUE = 0xF7;
const u8 STATUS_META_EVENT = 0xFF;
const u8 META_SET_TEMPO = 0x51;

static u32 read_big_endian_32(const u8* bytes) {
    return ((u32)bytes[0] << 24) | ((u32)bytes[1] << 16) | ((u32)bytes[2] << 8) | bytes[3];
}

static u16 read_big_endian_16(const u8* bytes) {
    return (bytes[0] << 8) | bytes[1];
}

// MIDI stores many numbers as "variable length": 7 bits per byte, and the
// top bit of each byte says whether another byte follows.
static u32 read_variable_length(const u8*& position, const u8* end) {
    u32 value = 0;
    for (int i = 0; i < 4 && position < end; i++) {
        u8 byte = *position;
        position++;
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) break;
    }
    return value;
}

// Sorts events by time. "Merge sort": sort each half, then merge the two
// sorted halves. It keeps events with the same time in their original order,
// which matters (a note-off and note-on at the same time must stay in order).
static void sort_events_by_time(MidiEvent* events, MidiEvent* scratch, int count) {
    if (count < 2) return;
    int half = count / 2;
    sort_events_by_time(events, scratch, half);
    sort_events_by_time(events + half, scratch, count - half);
    int left = 0, right = half, out = 0;
    while (left < half && right < count) {
        if (events[right].time < events[left].time) {
            scratch[out++] = events[right++];
        } else {
            scratch[out++] = events[left++];
        }
    }
    while (left < half)   scratch[out++] = events[left++];
    while (right < count) scratch[out++] = events[right++];
    memcpy(events, scratch, count * sizeof(MidiEvent));
}

static void add_event(Song* song, int capacity, u32 time, u8 type, u8 channel, u8 value1, u8 value2, u32 tempo) {
    if (song->event_count >= capacity) return;
    MidiEvent& event = song->events[song->event_count];
    event.time = time;
    event.type = type;
    event.channel = channel;
    event.value1 = value1;
    event.value2 = value2;
    event.microseconds_per_beat = tempo;
    song->event_count++;
}

bool read_midi_file(const u8* file, u64 file_size, const char* name, Song* song) {
    const u64 HEADER_SIZE = 14;
    if (file_size < HEADER_SIZE || memcmp(file, "MThd", 4) != 0) return false;
    u32 header_length = read_big_endian_32(file + 4);
    int track_count = read_big_endian_16(file + 10);
    int ticks_per_beat = read_big_endian_16(file + 12);
    if (ticks_per_beat & SMPTE_TIMING_FLAG) return false;       // a rare timing format we don't support

    copy_text(song->name, name, sizeof(song->name));
    int capacity = (int)(file_size / 2) + 16;         // every event takes at least 2 bytes
    song->events = allocate_array<MidiEvent>(capacity);
    song->event_count = 0;
    song->ticks_per_beat = ticks_per_beat != 0 ? ticks_per_beat : DEFAULT_TICKS_PER_BEAT;

    const u8* track = file + 8 + header_length;
    const u8* file_end = file + file_size;
    for (int track_number = 0; track_number < track_count && track + 8 <= file_end; track_number++) {
        if (memcmp(track, "MTrk", 4) != 0) break;
        u32 track_length = read_big_endian_32(track + 4);
        const u8* position = track + 8;
        const u8* track_end = position + track_length;
        if (track_end > file_end) track_end = file_end;

        u32 time = 0;
        u8 status = 0;     // "running status": an event may reuse the previous status byte
        while (position < track_end) {
            time += read_variable_length(position, track_end);
            if (position >= track_end) break;
            if (*position & 0x80) {          // a new status byte
                status = *position;
                position++;
            }

            if (status == STATUS_META_EVENT) {
                if (position >= track_end) break;
                u8 meta_type = *position;
                position++;
                u32 length = read_variable_length(position, track_end);
                if (length > (u32)(track_end - position)) length = track_end - position;   // a damaged file
                if (meta_type == META_SET_TEMPO && length == 3) {
                    u32 tempo = (position[0] << 16) | (position[1] << 8) | position[2];
                    add_event(song, capacity, time, MIDI_CHANGE_TEMPO, 0, 0, 0, tempo);
                }
                position += length;
                status = 0;
                continue;
            }
            if (status == STATUS_SYSEX_START || status == STATUS_SYSEX_CONTINUE) {
                u32 length = read_variable_length(position, track_end);
                if (length > (u32)(track_end - position)) length = track_end - position;
                position += length;
                status = 0;
                continue;
            }

            u8 kind = status & 0xF0;
            u8 channel = status & 0x0F;
            u8 value1 = position < track_end ? *position++ : 0;
            u8 value2 = 0;
            if (kind != STATUS_PROGRAM_CHANGE && kind != STATUS_CHANNEL_PRESSURE) {   // these two have only one value
                value2 = position < track_end ? *position++ : 0;
            }

            if (kind == STATUS_NOTE_ON && value2 > 0) {
                add_event(song, capacity, time, MIDI_NOTE_ON, channel, value1, value2, 0);
            } else if (kind == STATUS_NOTE_OFF || kind == STATUS_NOTE_ON) {     // note on with volume 0 = note off
                add_event(song, capacity, time, MIDI_NOTE_OFF, channel, value1, 0, 0);
            } else if (kind == STATUS_PROGRAM_CHANGE) {
                add_event(song, capacity, time, MIDI_CHANGE_INSTRUMENT, channel, value1, 0, 0);
            } else if (kind == STATUS_CONTROLLER) {
                add_event(song, capacity, time, MIDI_CONTROLLER, channel, value1, value2, 0);
            }
        }
        track = track + 8 + track_length;
    }

    if (song->event_count == 0) return false;
    MidiEvent* scratch = allocate_array<MidiEvent>(song->event_count);
    sort_events_by_time(song->events, scratch, song->event_count);
    return true;
}
