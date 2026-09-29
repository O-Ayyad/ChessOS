// sound.h - everything the game needs for sound and music.
//
// Uses the AC'97 sound card if there is one (VirtualBox, QEMU and VMware all
// have it), and otherwise the PC speaker (a simple beeper).
#pragma once
#include "types.h"
#include "synthesizer.h"      // for SoundEffect and the music functions

void setup_sound();            // loads the songs from music/ and finds the sound card
void update_sound();           // call this often: it keeps the sound card fed with samples
void play_sound_effect(SoundEffect effect);
const char* sound_device_name();    // "AC'97 sound card" or "PC speaker"
