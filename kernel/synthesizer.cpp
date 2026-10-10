#include "synthesizer.h"
#include "math_utils.h"
#include "text_utils.h"

//Written by claude
const int WAVE_TABLE_SIZE = 1024;
const int PHASE_TO_INDEX_SHIFT = 22;
const double PHASE_STEPS_PER_CYCLE = 4294967296.0;   // 2 to the power of 32

const int INSTRUMENT_COUNT = 9;
enum Instrument { PIANO, BELLS, ORGAN, GUITAR, BASS, STRINGS, BRASS, REED, SYNTH };


static const float HARMONICS[INSTRUMENT_COUNT][8] = {
    { 1, 0.50f, 0.28f, 0.16f, 0.09f, 0.05f, 0.03f, 0.02f },   // piano
    { 1, 0,     0.25f, 0,     0.12f, 0,     0,     0.05f },   // bells
    { 1, 0.60f, 0.45f, 0.30f, 0.25f, 0.15f, 0.10f, 0.08f },   // organ
    { 1, 0.60f, 0.35f, 0.25f, 0.15f, 0.10f, 0.05f, 0.03f },   // guitar
    { 1, 0.45f, 0.20f, 0.10f, 0.05f, 0,     0,     0     },   // bass
    { 1, 0.50f, 0.33f, 0.25f, 0.20f, 0.16f, 0.14f, 0.12f },   // strings
    { 1, 0.80f, 0.60f, 0.45f, 0.30f, 0.20f, 0.12f, 0.08f },   // brass
    { 1, 0.08f, 0.35f, 0.05f, 0.15f, 0.02f, 0.06f, 0     },   // reed (clarinet, flute...)
    { 1, 0,     0.33f, 0,     0.20f, 0,     0.14f, 0     },   // synth (square-ish)
};

struct Envelope {
    float attack, decay, sustain, release;
};
static const Envelope ENVELOPES[INSTRUMENT_COUNT] = {
    { 0.004f, 1.4f, 0.00f, 0.15f },   // piano
    { 0.002f, 1.8f, 0.00f, 0.30f },   // bells
    { 0.010f, 9.0f, 0.85f, 0.08f },   // organ
    { 0.003f, 0.9f, 0.00f, 0.12f },   // guitar
    { 0.006f, 1.2f, 0.45f, 0.10f },   // bass
    { 0.080f, 9.0f, 0.90f, 0.30f },   // strings
    { 0.030f, 9.0f, 0.80f, 0.12f },   // brass
    { 0.030f, 9.0f, 0.85f, 0.10f },   // reed
    { 0.010f, 9.0f, 0.70f, 0.10f },   // synth
};

static float sine_table[WAVE_TABLE_SIZE];
static float instrument_tables[INSTRUMENT_COUNT][WAVE_TABLE_SIZE];

static void build_wave_tables() {
    for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
        sine_table[i] = sine(i * 2 * PI / WAVE_TABLE_SIZE);
    }
    for (int instrument = 0; instrument < INSTRUMENT_COUNT; instrument++) {
        float loudest = 0;
        for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
            float value = 0;
            for (int harmonic = 0; harmonic < 8; harmonic++) {
                int index = (i * (harmonic + 1)) % WAVE_TABLE_SIZE;
                value += HARMONICS[instrument][harmonic] * sine_table[index];
            }
            instrument_tables[instrument][i] = value;
            loudest = max_float(loudest, abs_float(value));
        }
        for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
            instrument_tables[instrument][i] /= loudest;   // make the peak exactly 1
        }
    }
}

static u32 phase_step_for(float frequency) {
    return (u32)(frequency * PHASE_STEPS_PER_CYCLE / SAMPLE_RATE);
}

// MIDI note 69 is the A above middle C, 440 Hz. Each note up is 2^(1/12) higher.
static float note_frequency(int note) {
    static const float SEMITONE_RATIOS[12] = { 1.000000f, 1.059463f, 1.122462f, 1.189207f, 1.259921f, 1.334840f,
                                               1.414214f, 1.498307f, 1.587401f, 1.681793f, 1.781797f, 1.887749f };
    const int A440_NOTE = 69;
    int steps = note - A440_NOTE;
    int octaves = 0;
    while (steps < 0)   { steps += 12; octaves--; }
    while (steps >= 12) { steps -= 12; octaves++; }
    float frequency = 440.0f * SEMITONE_RATIOS[steps];
    while (octaves > 0) { frequency *= 2; octaves--; }
    while (octaves < 0) { frequency /= 2; octaves++; }
    return frequency;
}

// General MIDI puts instruments in groups of 8 (0-7 pianos, 8-15 bells, ...).
static Instrument instrument_for_program(int program) {
    if (program < 8)  return PIANO;
    if (program < 16) return BELLS;
    if (program < 24) return ORGAN;
    if (program < 32) return GUITAR;
    if (program < 40) return BASS;
    if (program < 56) return STRINGS;
    if (program < 64) return BRASS;
    if (program < 80) return REED;
    return SYNTH;
}

static u32 noise_state = 0x12345678;
static float random_noise() {        // a random number from -1 to 1
    noise_state ^= noise_state << 13;
    noise_state ^= noise_state >> 17;
    noise_state ^= noise_state << 5;
    return ((int)(noise_state & 0xFFFF) - 32768) / 32768.0f;
}

// ================================================================ voices
// A "voice" plays one note. 24 voices means up to 24 notes at the same time.
const int VOICE_COUNT = 24;
const int DRUM_CHANNEL = 9;          // in MIDI, channel 10 (counted from 1) is always drums
const float SILENT = 0.002f;         // below this loudness a sound counts as finished

enum DrumKind { KICK, SNARE, CLAP, CLOSED_HI_HAT, OPEN_HI_HAT, CRASH, RIDE, TOM };

struct Voice {
    bool playing;
    bool released;       // the key was let go; the note is fading out
    bool is_drum;
    u8 channel;
    u8 note;
    Instrument instrument;
    DrumKind drum;
    u32 phase;
    u32 phase_step;
    float volume;
    float envelope;      // current loudness from the envelope (0..1)
    float time;          // seconds since the note started (or since release)
    float drum_pitch;
    float previous_noise;
    u32 start_order;     // for choosing which voice to steal when all are busy
};

static Voice voices[VOICE_COUNT];
static u8 channel_program[16];
static float channel_volume[16];
static u32 voices_started = 0;

static Voice* find_free_voice() {
    for (int i = 0; i < VOICE_COUNT; i++) {
        if (!voices[i].playing) return &voices[i];
    }
    // all busy: take the oldest released note, or else the oldest note
    Voice* oldest = nullptr;
    for (int i = 0; i < VOICE_COUNT; i++) {
        if (voices[i].released && (oldest == nullptr || voices[i].start_order < oldest->start_order)) oldest = &voices[i];
    }
    if (oldest != nullptr) return oldest;
    oldest = &voices[0];
    for (int i = 1; i < VOICE_COUNT; i++) {
        if (voices[i].start_order < oldest->start_order) oldest = &voices[i];
    }
    return oldest;
}

static DrumKind drum_for_note(int note) {
    // General MIDI drum note numbers
    if (note == 35 || note == 36) return KICK;
    if (note == 38 || note == 40) return SNARE;
    if (note == 42 || note == 44) return CLOSED_HI_HAT;
    if (note == 46) return OPEN_HI_HAT;
    if (note == 49 || note == 52 || note == 55 || note == 57) return CRASH;
    if (note == 51 || note == 53 || note == 59) return RIDE;
    if (note == 41 || note == 43 || note == 45 || note == 47 || note == 48 || note == 50) return TOM;
    return CLAP;
}

static void start_note(int channel, int note, int velocity) {
    Voice* voice = find_free_voice();
    memset(voice, 0, sizeof(Voice));
    voice->playing = true;
    voice->channel = channel;
    voice->note = note;
    voice->start_order = voices_started++;
    voice->volume = (velocity / 127.0f) * channel_volume[channel];
    if (channel == DRUM_CHANNEL) {
        voice->is_drum = true;
        voice->drum = drum_for_note(note);
        const int LOWEST_TOM_NOTE = 41;
        voice->drum_pitch = voice->drum == TOM ? 70.0f + (note - LOWEST_TOM_NOTE) * 18.0f : 190.0f;
    } else {
        voice->instrument = instrument_for_program(channel_program[channel]);
        voice->phase_step = phase_step_for(note_frequency(note));
    }
}

static void stop_note(int channel, int note) {
    for (int i = 0; i < VOICE_COUNT; i++) {
        Voice& voice = voices[i];
        if (voice.playing && !voice.released && !voice.is_drum && voice.channel == channel && voice.note == note) {
            voice.released = true;
            voice.time = 0;
        }
    }
}

static void stop_all_notes() {
    for (int i = 0; i < VOICE_COUNT; i++) voices[i].playing = false;
}

// ---- one sample of a drum sound. "time" is seconds since the drum was hit.
static float next_drum_sample(Voice& voice) {
    const float SECONDS_PER_SAMPLE = 1.0f / SAMPLE_RATE;
    voice.time += SECONDS_PER_SAMPLE;
    float t = voice.time;
    float sample = 0;
    float loudness = 1;

    if (voice.drum == KICK) {
        // a low sine wave whose pitch drops quickly from 150 Hz to 50 Hz
        float pitch = 50.0f + 100.0f * exponential(-t * 35.0f);
        voice.phase += phase_step_for(pitch);
        loudness = exponential(-t * 9.0f);
        sample = sine_table[voice.phase >> PHASE_TO_INDEX_SHIFT] * loudness * 1.4f;
    } else if (voice.drum == SNARE) {
        // noise (the snare wires) plus a short tone (the drum skin)
        voice.phase += phase_step_for(voice.drum_pitch);
        loudness = exponential(-t * 16.0f);
        sample = (random_noise() * 0.75f + sine_table[voice.phase >> PHASE_TO_INDEX_SHIFT] * 0.35f) * loudness;
    } else if (voice.drum == CLAP) {
        loudness = exponential(-t * 40.0f);
        sample = random_noise() * loudness * 0.8f;
    } else if (voice.drum == TOM) {
        float pitch = voice.drum_pitch * (0.7f + 0.3f * exponential(-t * 12.0f));
        voice.phase += phase_step_for(pitch);
        loudness = exponential(-t * 7.0f);
        sample = sine_table[voice.phase >> PHASE_TO_INDEX_SHIFT] * loudness;
    } else {
        // cymbals and hi-hats: the *change* in noise from one sample to the next,
        // which keeps only the high, hissy part of the noise
        float noise = random_noise();
        float hiss = noise - voice.previous_noise;
        voice.previous_noise = noise;
        float fade_speed = 4.0f;
        float level = 0.35f;
        if (voice.drum == CLOSED_HI_HAT) fade_speed = 55.0f;
        if (voice.drum == OPEN_HI_HAT)   fade_speed = 7.0f;
        if (voice.drum == CRASH)       { fade_speed = 2.2f; level = 0.55f; }
        loudness = exponential(-t * fade_speed);
        sample = hiss * loudness * level;
        if (voice.drum == RIDE) {       // a ride cymbal also rings with a high tone
            voice.phase += phase_step_for(3100.0f);
            sample += sine_table[voice.phase >> PHASE_TO_INDEX_SHIFT] * loudness * 0.1f;
        }
    }
    if (loudness < SILENT) voice.playing = false;
    return sample * voice.volume;
}

// ---- one sample of an instrument note
static float next_note_sample(Voice& voice) {
    const float SECONDS_PER_SAMPLE = 1.0f / SAMPLE_RATE;
    const Envelope& envelope = ENVELOPES[voice.instrument];

    if (!voice.released) {
        voice.time += SECONDS_PER_SAMPLE;
        if (voice.time < envelope.attack) {
            voice.envelope = voice.time / envelope.attack;          // rising
        } else {
            // move a little towards the sustain level every sample
            float step = SECONDS_PER_SAMPLE / (envelope.decay * 0.35f);
            voice.envelope = envelope.sustain + (voice.envelope - envelope.sustain) * (1.0f - step);
        }
        if (envelope.sustain == 0 && voice.time > envelope.attack && voice.envelope < SILENT / 4) {
            voice.playing = false;                                  // a piano note that has died away
            return 0;
        }
    } else {
        voice.envelope *= 1.0f - SECONDS_PER_SAMPLE / (envelope.release * 0.3f);   // fading out
        if (voice.envelope < SILENT / 2) {
            voice.playing = false;
            return 0;
        }
    }

    u32 step = voice.phase_step;
    if (voice.instrument == STRINGS) {
        // a gentle "vibrato" (the pitch wobbles 5.5 times per second), like a violinist's hand
        float wobble_position = voice.time * 5.5f * WAVE_TABLE_SIZE;
        float wobble = sine_table[(u32)wobble_position % WAVE_TABLE_SIZE] * 0.004f * min_float(1.0f, voice.time * 2);
        step = (u32)(step * (1.0f + wobble));
    }
    voice.phase += step;
    return instrument_tables[voice.instrument][voice.phase >> PHASE_TO_INDEX_SHIFT] * voice.envelope * voice.volume;
}

const u32 DEFAULT_MICROSECONDS_PER_BEAT = 500000;  //120 beats per minute
const int SECONDS_BETWEEN_SONGS = 2;
const int CONTROLLER_VOLUME = 7;
const int CONTROLLER_ALL_SOUND_OFF = 120;
const int CONTROLLER_ALL_NOTES_OFF = 123;
const float DEFAULT_CHANNEL_VOLUME = 100 / 127.0f;

static Song songs[MAX_SONGS];
static int number_of_songs = 0;
static bool music_playing = true;
static int current_song = 0;
static int next_event = 0;             // index of the next event to play
static double current_tick = 0;        // how far into the song we are
static double ticks_per_sample = 0;
static int silence_samples_left = 0;   // the pause between two songs

static void set_tempo(u32 microseconds_per_beat) {
    // ticks per second = ticks_per_beat * beats per second
    double beats_per_second = 1000000.0 / microseconds_per_beat;
    ticks_per_sample = songs[current_song].ticks_per_beat * beats_per_second / SAMPLE_RATE;
}

static void start_song(int index) {
    stop_all_notes();
    for (int channel = 0; channel < 16; channel++) {
        channel_program[channel] = 0;
        channel_volume[channel] = DEFAULT_CHANNEL_VOLUME;
    }
    current_song = index;
    next_event = 0;
    current_tick = 0;
    if (number_of_songs > 0) set_tempo(DEFAULT_MICROSECONDS_PER_BEAT);
}

static bool any_voice_playing() {
    for (int i = 0; i < VOICE_COUNT; i++) {
        if (voices[i].playing) return true;
    }
    return false;
}

// Moves the song forward by "samples" and plays every event that is now due.
static void advance_song(int samples) {
    if (number_of_songs == 0 || !music_playing) return;

    if (silence_samples_left > 0) {
        silence_samples_left -= samples;
        if (silence_samples_left <= 0) start_song((current_song + 1) % number_of_songs);
        return;
    }

    Song& song = songs[current_song];
    current_tick += ticks_per_sample * samples;
    while (next_event < song.event_count && song.events[next_event].time <= current_tick) {
        const MidiEvent& event = song.events[next_event];
        next_event++;
        if (event.type == MIDI_NOTE_ON)           start_note(event.channel, event.value1, event.value2);
        if (event.type == MIDI_NOTE_OFF)          stop_note(event.channel, event.value1);
        if (event.type == MIDI_CHANGE_INSTRUMENT) channel_program[event.channel] = event.value1;
        if (event.type == MIDI_CHANGE_TEMPO)      set_tempo(event.microseconds_per_beat != 0 ? event.microseconds_per_beat : DEFAULT_MICROSECONDS_PER_BEAT);
        if (event.type == MIDI_CONTROLLER) {
            if (event.value1 == CONTROLLER_VOLUME) channel_volume[event.channel] = event.value2 / 127.0f;
            if (event.value1 == CONTROLLER_ALL_NOTES_OFF || event.value1 == CONTROLLER_ALL_SOUND_OFF) stop_all_notes();
        }
    }
    // the song is over when all events are played and the last notes have faded
    if (next_event >= song.event_count && !any_voice_playing()) {
        silence_samples_left = SAMPLE_RATE * SECONDS_BETWEEN_SONGS;
    }
}

// ================================================================ sound effects
// A sound effect is a short list of beeps. Frequency NOISE means a burst of noise,
// and 0 means a short silence.
const int NOISE = 1;
const int MAX_EFFECT_NOTES = 32;
struct Beep { int frequency; int milliseconds; };

static Beep effect_beeps[MAX_EFFECT_NOTES];
static int effect_beep_count = 0;
static int effect_beep_index = 0;
static int effect_samples_left = 0;      // in the current beep
static int effect_samples_total = 0;
static bool effect_playing = false;
static u32 effect_phase = 0;

static void add_beep(int frequency, int milliseconds) {
    if (effect_beep_count < MAX_EFFECT_NOTES) {
        effect_beeps[effect_beep_count].frequency = frequency;
        effect_beeps[effect_beep_count].milliseconds = milliseconds;
        effect_beep_count++;
    }
}

void start_sound_effect(SoundEffect effect) {
    effect_beep_count = 0;
    if (effect == SOUND_SELECT)      { add_beep(1400, 25); }
    if (effect == SOUND_MOVE)        { add_beep(NOISE, 22); add_beep(330, 70); }
    if (effect == SOUND_CAPTURE)     { add_beep(NOISE, 30); add_beep(520, 50); add_beep(NOISE, 25); add_beep(260, 90); }
    if (effect == SOUND_CASTLE)      { add_beep(NOISE, 20); add_beep(440, 50); add_beep(0, 40); add_beep(NOISE, 20); add_beep(660, 70); }
    if (effect == SOUND_CHECK)       { add_beep(988, 110); add_beep(0, 40); add_beep(1318, 170); }
    if (effect == SOUND_ILLEGAL)     { add_beep(160, 90); add_beep(0, 20); add_beep(130, 140); }
    if (effect == SOUND_CHANGE_VIEW) { add_beep(NOISE, 15); add_beep(1100, 25); }
    if (effect == SOUND_MENU)        { add_beep(880, 20); }
    if (effect == SOUND_GAME_OVER)   { add_beep(523, 150); add_beep(659, 150); add_beep(784, 150); add_beep(1046, 450); }
    if (effect == SOUND_GAME_START)  { add_beep(392, 100); add_beep(523, 100); add_beep(659, 100); add_beep(784, 260); }
    if (effect == SOUND_FOUND_EASTER_EGG) {
        add_beep(1400, 35);
        add_beep(0, 20);
        add_beep(1400, 35);
        add_beep(0, 20);
        add_beep(1400, 35);
        add_beep(0, 20);
    }
    effect_beep_index = 0;
    effect_samples_left = 0;
    effect_playing = true;
}

// Moves to the next beep when the current one is finished. Returns false when the effect is over.
static bool effect_move_on() {
    if (effect_samples_left > 0) return true;
    if (effect_beep_index >= effect_beep_count) {
        effect_playing = false;
        return false;
    }
    effect_samples_total = effect_beeps[effect_beep_index].milliseconds * SAMPLE_RATE / 1000;
    effect_samples_left = effect_samples_total;
    effect_beep_index++;
    return true;
}

static float next_effect_sample() {
    if (!effect_playing || !effect_move_on()) return 0;
    const Beep& beep = effect_beeps[effect_beep_index - 1];
    int samples_done = effect_samples_total - effect_samples_left;
    effect_samples_left--;
    // fade in over 60 samples and out over 300, so the beeps don't click
    float fade = min_float(1.0f, samples_done / 60.0f) * min_float(1.0f, effect_samples_left / 300.0f);
    float sample = 0;
    if (beep.frequency == NOISE) {
        sample = random_noise() * 0.6f;
    } else if (beep.frequency > 0) {
        effect_phase += phase_step_for(beep.frequency);
        // the tone plus a little of its third harmonic, for a brighter "chip" sound
        sample = sine_table[effect_phase >> PHASE_TO_INDEX_SHIFT] + 0.3f * sine_table[(effect_phase * 3) >> PHASE_TO_INDEX_SHIFT];
    }
    const float EFFECT_VOLUME = 0.30f;
    return sample * fade * EFFECT_VOLUME;
}

// ================================================================ mixing everything together
const int SAMPLES_PER_SONG_STEP = 32;     // how often (in samples) we check the song for new events
const float MUSIC_VOLUME = 0.22f;
const float MUSIC_VOLUME_UNDER_EFFECT = 0.6f;
const float LOUDEST_SAMPLE = 30000;

void render_samples(i16* stereo_samples, int frames) {
    for (int start = 0; start < frames; start += SAMPLES_PER_SONG_STEP) {
        advance_song(SAMPLES_PER_SONG_STEP);
        int count = min_int(SAMPLES_PER_SONG_STEP, frames - start);
        for (int i = 0; i < count; i++) {
            float music = 0;
            for (int v = 0; v < VOICE_COUNT; v++) {
                if (!voices[v].playing) continue;
                music += voices[v].is_drum ? next_drum_sample(voices[v]) : next_note_sample(voices[v]);
            }
            music *= MUSIC_VOLUME;
            if (effect_playing) music *= MUSIC_VOLUME_UNDER_EFFECT;     // make room for the effect
            float mix = music + next_effect_sample();
            mix = mix / (1.0f + abs_float(mix) * 0.6f);                  // "soft clipping": loud parts get squashed, not cut off
            int sample = clamp_int((int)(mix * LOUDEST_SAMPLE), -32768, 32767);
            stereo_samples[(start + i) * 2] = (i16)sample;              // left
            stereo_samples[(start + i) * 2 + 1] = (i16)sample;          // right
        }
    }
}

int speaker_frequency_after(int milliseconds) {
    int samples = milliseconds * SAMPLE_RATE / 1000;
    advance_song(samples);
    // The speaker can't fade notes out, so a released note just stops.
    // It can't play drums either, so they stop right away.
    for (int v = 0; v < VOICE_COUNT; v++) {
        if (voices[v].playing && (voices[v].released || voices[v].is_drum)) voices[v].playing = false;
    }

    if (effect_playing) {
        while (samples > 0 && effect_move_on()) {
            int step = min_int(samples, effect_samples_left);
            effect_samples_left -= step;
            samples -= step;
        }
        if (effect_playing && effect_beep_index > 0) {
            const int NOISE_BEEP_FREQUENCY = 90;       // the speaker can't do noise: use a low buzz
            int frequency = effect_beeps[effect_beep_index - 1].frequency;
            return frequency == NOISE ? NOISE_BEEP_FREQUENCY : frequency;
        }
    }
    if (!music_playing) return 0;
    // play the highest note that is sounding, which is usually the melody
    int highest = -1;
    for (int v = 0; v < VOICE_COUNT; v++) {
        if (voices[v].playing && !voices[v].is_drum && voices[v].note > highest) highest = voices[v].note;
    }
    if (highest < 0) return 0;
    return (int)note_frequency(highest);
}

// ================================================================ public functions
void setup_synthesizer() {
    build_wave_tables();
    for (int channel = 0; channel < 16; channel++) channel_volume[channel] = DEFAULT_CHANNEL_VOLUME;
}

bool add_song(const u8* midi_file, u64 size, const char* name) {
    if (number_of_songs >= MAX_SONGS) return false;
    if (!read_midi_file(midi_file, size, name, &songs[number_of_songs])) return false;
    number_of_songs++;
    if (number_of_songs == 1) start_song(0);
    return true;
}

int song_count() { return number_of_songs; }

const char* current_song_name() {
    return number_of_songs > 0 ? songs[current_song].name : "no music loaded";
}

void start_next_song() {
    if (number_of_songs == 0) return;
    silence_samples_left = 0;
    start_song((current_song + 1) % number_of_songs);
}

void set_music_playing(bool playing) {
    music_playing = playing;
    if (!playing) stop_all_notes();
}

bool is_music_playing() { return music_playing; }
