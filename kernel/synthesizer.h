#pragma once
#include "types.h"
#include "midi_file.h"

const int SAMPLE_RATE = 48000; 
const int MAX_SONGS = 8;

enum SoundEffect {
    SOUND_SELECT, SOUND_MOVE, SOUND_CAPTURE, SOUND_CASTLE, SOUND_CHECK, SOUND_ILLEGAL,
    SOUND_GAME_OVER, SOUND_CHANGE_VIEW, SOUND_GAME_START, SOUND_MENU
};

void setup_synthesizer();
bool add_song(const u8* midi_file, u64 size, const char* name);
int  song_count();
const char* current_song_name();
void start_next_song();
void set_music_playing(bool playing);
bool is_music_playing();
void start_sound_effect(SoundEffect effect);

void render_samples(i16* stereo_samples, int frames);

int speaker_frequency_after(int milliseconds);
