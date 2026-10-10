#pragma once
#include "types.h"
#include "midi_file.h"


const int SAMPLE_RATE = 48000;
// must match tools/pack_assets.py
const int MAX_SONGS = 64;

enum SoundEffect {
    SOUND_SELECT, SOUND_MOVE, SOUND_CAPTURE, SOUND_CASTLE, SOUND_CHECK, SOUND_ILLEGAL,
    SOUND_GAME_OVER, SOUND_CHANGE_VIEW, SOUND_GAME_START, SOUND_MENU, SOUND_FOUND_EASTER_EGG
};

void setup_synthesizer();
// Returns false if the file cannot be read as a song, or there are MAX_SONGS songs already.
bool add_song(const u8* midi_file, u64 size, const char* name);
int  song_count();
const char* current_song_name();
void start_next_song();
void set_music_playing(bool playing);
bool is_music_playing();
void start_sound_effect(SoundEffect effect);

// Fills stereo_samples with the next "frames" frames of sound. A frame is two samples: left and right.
void render_samples(i16* stereo_samples, int frames);

// For the PC speaker, which can only beep at one frequency at a time: moves the
// music on by that much time and returns the frequency to beep at now (0 = silence).
int speaker_frequency_after(int milliseconds);
