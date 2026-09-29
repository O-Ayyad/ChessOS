#include "timer.h"
#include "cpu.h"

// The PIT chip counts down 1,193,182 times per second.
const u32 PIT_TICKS_PER_SECOND = 1193182;
const u16 PIT_CHANNEL_2_PORT = 0x42;
const u16 PIT_COMMAND_PORT = 0x43;
const u8  PIT_CHANNEL_2_ONE_SHOT = 0xB0; // channel 2, low byte then high byte, "count down once"
const u16 SPEAKER_CONTROL_PORT = 0x61; // also controls timer channel 2
const u8  TIMER_2_GATE = 0x01;     // lets channel 2 count
const u8  SPEAKER_ON = 0x02;
const u8  TIMER_2_FINISHED = 0x20;  // becomes 1 when channel 2 reaches zero
const u32 MEASURE_MILLISECONDS = 50;
const int MEASURE_ATTEMPTS = 3;

static u64 cycles_per_millisecond = 1000000;
static u64 start_cycles = 0;

void setup_timer() {
    u64 best = 0;
    for (int attempt = 0; attempt < MEASURE_ATTEMPTS; attempt++) {
        u8 control = read_port_8(SPEAKER_CONTROL_PORT);
        write_port_8(SPEAKER_CONTROL_PORT, control & ~(TIMER_2_GATE | SPEAKER_ON));  //stop

        // 50 ms
        u16 count = PIT_TICKS_PER_SECOND * MEASURE_MILLISECONDS / 1000;
        write_port_8(PIT_COMMAND_PORT, PIT_CHANNEL_2_ONE_SHOT);
        write_port_8(PIT_CHANNEL_2_PORT, count & 0xFF);
        write_port_8(PIT_CHANNEL_2_PORT, count >> 8);

        //count CPU cycles until it finishes
        write_port_8(SPEAKER_CONTROL_PORT, (control & ~SPEAKER_ON) | TIMER_2_GATE);
        u64 cycles_before = read_cycle_counter();
        u64 safety = 0;
        while ((read_port_8(SPEAKER_CONTROL_PORT) & TIMER_2_FINISHED) == 0) {
            safety++;
            if (safety > 200000000ULL) {
                break;  // the timer never finished (should not happen)
            }
        }
        u64 cycles_after = read_cycle_counter();
        write_port_8(SPEAKER_CONTROL_PORT, control & ~(TIMER_2_GATE | SPEAKER_ON));

        u64 measured = (cycles_after - cycles_before) / MEASURE_MILLISECONDS;
        if (measured > 0 && (best == 0 || measured < best)) {
            best = measured;
        }
    }
    if (best >= 1000) {
        cycles_per_millisecond = best;
    }
    start_cycles = read_cycle_counter();
}

u64 milliseconds_since_start() {
    return (read_cycle_counter() - start_cycles) / cycles_per_millisecond;
}

void wait_milliseconds(u64 milliseconds) {
    u64 end = milliseconds_since_start() + milliseconds;
    while (milliseconds_since_start() < end) {
        cpu_relax();
    }
}
