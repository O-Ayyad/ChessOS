#pragma once
#include "types.h"
#include "synthesizer.h"

void setup_sound();
void update_sound();
void play_sound_effect(SoundEffect effect);
const char* sound_device_name();
