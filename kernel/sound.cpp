// sound.cpp - see sound.h
//
// How the AC'97 sound card plays sound: we give it a list of 32 buffers
// (the "buffer descriptor list"). It plays them one after another, in a
// circle, reading the samples straight from memory by itself ("DMA").
// update_sound() keeps the buffers just ahead of the one playing filled with
// fresh samples from the synthesizer.
#include "sound.h"
#include "pci.h"
#include "cpu.h"
#include "timer.h"
#include "memory.h"
#include "text_utils.h"
#include "asset_files.h"
#include "debug_log.h"

// ---- AC'97 registers. The card has two sets: the "mixer" (volumes) and the "bus master" (playing).
const u16 PCI_CLASS_AUDIO = 0x0401;
const u16 MIXER_RESET = 0x00;
const u16 MIXER_MASTER_VOLUME = 0x02;
const u16 MIXER_HEADPHONE_VOLUME = 0x04;
const u16 MIXER_PCM_OUT_VOLUME = 0x18;
const u16 MIXER_EXTENDED_AUDIO_ID = 0x28;
const u16 MIXER_EXTENDED_AUDIO_CONTROL = 0x2A;
const u16 MIXER_FRONT_SAMPLE_RATE = 0x2C;
const u16 VOLUME_FULL = 0x0000;                  // 0 = no attenuation (loudest)
const u16 VOLUME_PCM_GOOD_LEVEL = 0x0606;
const u16 VARIABLE_RATE_AUDIO = 0x1;

const u16 PCM_OUT_BUFFER_LIST_ADDRESS = 0x10;
const u16 PCM_OUT_CURRENT_BUFFER = 0x14;
const u16 PCM_OUT_LAST_VALID_BUFFER = 0x15;
const u16 PCM_OUT_STATUS = 0x16;
const u16 PCM_OUT_CONTROL = 0x1B;
const u16 GLOBAL_CONTROL = 0x2C;
const u16 GLOBAL_STATUS = 0x30;
const u8  CONTROL_RUN = 0x01;
const u8  CONTROL_RESET = 0x02;
const u32 GLOBAL_COLD_RESET_OFF = 0x02;
const u32 STATUS_CODEC_READY = 0x100;
const u16 STATUS_HALTED = 0x01;
const u16 STATUS_CLEAR_BITS = 0x1C;

const int BUFFER_COUNT = 32;
const int FRAMES_PER_BUFFER = 1024;              // about 21 ms of sound each
const int BUFFERS_TO_KEEP_AHEAD = 6;             // about 128 ms: enough to survive a slow screen redraw
const int BYTES_PER_FRAME = 4;                   // left + right, 2 bytes each

struct __attribute__((packed)) BufferDescriptor {
    u32 address;       // where the samples are in memory
    u16 sample_count;  // number of samples (left and right count separately)
    u16 flags;
};

static bool using_sound_card = false;
static u16 mixer_port = 0;
static u16 player_port = 0;
static BufferDescriptor* buffer_list = nullptr;
static i16* buffers[BUFFER_COUNT];
static int next_buffer_to_fill = 0;
static int last_buffer_seen = 0;

static bool setup_ac97() {
    PciDevice card = pci_find_device_by_class(PCI_CLASS_AUDIO);
    if (!card.found) return false;
    u32 bar0 = pci_read(card, PCI_BAR0);
    u32 bar1 = pci_read(card, PCI_BAR1);
    if ((bar0 & PCI_BAR_IS_IO_PORT) == 0 || (bar1 & PCI_BAR_IS_IO_PORT) == 0) return false;
    mixer_port = bar0 & ~3;
    player_port = bar1 & ~3;

    u32 command = pci_read(card, PCI_COMMAND);
    pci_write(card, PCI_COMMAND, (command & 0xFFFF) | PCI_COMMAND_IO_SPACE | PCI_COMMAND_BUS_MASTER);

    // wake the card up and wait until it says it is ready
    write_port_32(player_port + GLOBAL_CONTROL, GLOBAL_COLD_RESET_OFF);
    wait_milliseconds(20);
    write_port_16(mixer_port + MIXER_RESET, 1);
    for (int i = 0; i < 200; i++) {
        if (read_port_32(player_port + GLOBAL_STATUS) & STATUS_CODEC_READY) break;
        wait_milliseconds(1);
    }

    // volumes up, and ask for 48,000 samples per second if the card can change its rate
    write_port_16(mixer_port + MIXER_MASTER_VOLUME, VOLUME_FULL);
    write_port_16(mixer_port + MIXER_HEADPHONE_VOLUME, VOLUME_FULL);
    write_port_16(mixer_port + MIXER_PCM_OUT_VOLUME, VOLUME_PCM_GOOD_LEVEL);
    if (read_port_16(mixer_port + MIXER_EXTENDED_AUDIO_ID) & VARIABLE_RATE_AUDIO) {
        u16 control = read_port_16(mixer_port + MIXER_EXTENDED_AUDIO_CONTROL);
        write_port_16(mixer_port + MIXER_EXTENDED_AUDIO_CONTROL, control | VARIABLE_RATE_AUDIO);
        write_port_16(mixer_port + MIXER_FRONT_SAMPLE_RATE, SAMPLE_RATE);
    }

    // The card reads memory by physical address. Our memory is "identity mapped"
    // (address = physical address) and below 4 GB, so we can pass pointers directly.
    const u64 PAGE_SIZE = 4096;
    buffer_list = (BufferDescriptor*)allocate_aligned_memory(sizeof(BufferDescriptor) * BUFFER_COUNT, PAGE_SIZE);
    for (int i = 0; i < BUFFER_COUNT; i++) {
        buffers[i] = (i16*)allocate_aligned_memory(FRAMES_PER_BUFFER * BYTES_PER_FRAME, PAGE_SIZE);
        buffer_list[i].address = (u32)(u64)buffers[i];
        buffer_list[i].sample_count = FRAMES_PER_BUFFER * 2;
        buffer_list[i].flags = 0;
    }

    // reset the player, give it the list, and start it
    write_port_8(player_port + PCM_OUT_CONTROL, CONTROL_RESET);
    for (int i = 0; i < 100 && (read_port_8(player_port + PCM_OUT_CONTROL) & CONTROL_RESET); i++) {
        wait_milliseconds(1);
    }
    write_port_32(player_port + PCM_OUT_BUFFER_LIST_ADDRESS, (u32)(u64)buffer_list);
    write_port_8(player_port + PCM_OUT_LAST_VALID_BUFFER, BUFFER_COUNT - 1);
    write_port_16(player_port + PCM_OUT_STATUS, STATUS_CLEAR_BITS);
    next_buffer_to_fill = 1;
    last_buffer_seen = 0;
    write_port_8(player_port + PCM_OUT_CONTROL, CONTROL_RUN);
    return true;
}

static void feed_ac97() {
    int playing = read_port_8(player_port + PCM_OUT_CURRENT_BUFFER) % BUFFER_COUNT;

    // silence the buffers that have finished playing, in case we fall behind
    while (last_buffer_seen != playing) {
        memset(buffers[last_buffer_seen], 0, FRAMES_PER_BUFFER * BYTES_PER_FRAME);
        last_buffer_seen = (last_buffer_seen + 1) % BUFFER_COUNT;
    }

    int ahead = (next_buffer_to_fill - playing + BUFFER_COUNT) % BUFFER_COUNT;
    if (ahead == 0 || ahead > BUFFER_COUNT / 2) {
        next_buffer_to_fill = (playing + 1) % BUFFER_COUNT;     // we fell behind: catch up
    }
    while ((next_buffer_to_fill - playing + BUFFER_COUNT) % BUFFER_COUNT <= BUFFERS_TO_KEEP_AHEAD) {
        render_samples(buffers[next_buffer_to_fill], FRAMES_PER_BUFFER);
        next_buffer_to_fill = (next_buffer_to_fill + 1) % BUFFER_COUNT;
    }

    // "last valid buffer" = the one just before the playing one, so the card never stops
    write_port_8(player_port + PCM_OUT_LAST_VALID_BUFFER, (playing + BUFFER_COUNT - 1) % BUFFER_COUNT);
    u16 status = read_port_16(player_port + PCM_OUT_STATUS);
    if (status & STATUS_CLEAR_BITS) write_port_16(player_port + PCM_OUT_STATUS, status & STATUS_CLEAR_BITS);
    if (status & STATUS_HALTED) write_port_8(player_port + PCM_OUT_CONTROL, CONTROL_RUN);
}

// ---- the PC speaker: timer channel 2 makes a square wave of any frequency
const u16 PIT_CHANNEL_2_PORT = 0x42;
const u16 PIT_COMMAND_PORT = 0x43;
const u8  PIT_CHANNEL_2_SQUARE_WAVE = 0xB6;
const u16 SPEAKER_CONTROL_PORT = 0x61;
const u8  SPEAKER_BITS = 0x03;          // timer 2 on + speaker on
const u32 PIT_TICKS_PER_SECOND = 1193182;

static u64 speaker_last_update = 0;
static int speaker_frequency = 0;

static void set_speaker(int frequency) {
    if (frequency <= 0) {
        write_port_8(SPEAKER_CONTROL_PORT, read_port_8(SPEAKER_CONTROL_PORT) & ~SPEAKER_BITS);
        return;
    }
    u32 divisor = PIT_TICKS_PER_SECOND / frequency;
    write_port_8(PIT_COMMAND_PORT, PIT_CHANNEL_2_SQUARE_WAVE);
    write_port_8(PIT_CHANNEL_2_PORT, divisor & 0xFF);
    write_port_8(PIT_CHANNEL_2_PORT, (divisor >> 8) & 0xFF);
    write_port_8(SPEAKER_CONTROL_PORT, read_port_8(SPEAKER_CONTROL_PORT) | SPEAKER_BITS);
}

static void update_speaker() {
    u64 now = milliseconds_since_start();
    int elapsed = (int)(now - speaker_last_update);
    if (elapsed <= 0) return;
    speaker_last_update = now;
    int frequency = speaker_frequency_after(elapsed);
    if (frequency != speaker_frequency) {
        set_speaker(frequency);
        speaker_frequency = frequency;
    }
}

// ---- public functions
static void load_songs_from_assets() {
    for (int i = 0; i < asset_file_count(); i++) {
        const AssetFile& file = asset_file(i);
        const char* after_folder = find_text(file.path, "/music/");
        if (after_folder == nullptr) continue;
        if (!text_ends_with(file.path, ".mid") && !text_ends_with(file.path, ".MID") && !text_ends_with(file.path, ".midi")) continue;

        // the song name is the file name without ".mid", with _ shown as spaces
        after_folder += text_length("/music/");
        char name[48];
        int length = 0;
        while (after_folder[length] != 0 && after_folder[length] != '.' && length < 47) {
            name[length] = after_folder[length] == '_' ? ' ' : after_folder[length];
            length++;
        }
        name[length] = 0;
        add_song(file.data, file.size, name);
    }
}

void setup_sound() {
    setup_synthesizer();
    load_songs_from_assets();
    set_speaker(0);
    using_sound_card = setup_ac97();
    speaker_last_update = milliseconds_since_start();
    debug_log("sound: ");
    debug_log(sound_device_name());
    debug_log("\n");
}

void update_sound() {
    if (using_sound_card) {
        feed_ac97();
    } else {
        update_speaker();
    }
}

void play_sound_effect(SoundEffect effect) {
    start_sound_effect(effect);
}

const char* sound_device_name() {
    return using_sound_card ? "AC'97 sound card" : "PC speaker";
}
