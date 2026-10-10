#include "debug_log.h"
#include "cpu.h"

const u16 COM1 = 0x3F8;
const u16 COM1_DATA = COM1 + 0;
const u16 COM1_INTERRUPTS = COM1 + 1;
const u16 COM1_FIFO = COM1 + 2;
const u16 COM1_LINE_CONTROL = COM1 + 3;
const u16 COM1_MODEM_CONTROL = COM1 + 4;
const u16 COM1_LINE_STATUS = COM1 + 5;
const u8 LINE_STATUS_READY_TO_SEND = 0x20;
const int MAX_WAIT_TRIES = 10000;

static bool serial_port_ready = false;

static void setup_serial_port() {
    write_port_8(COM1_INTERRUPTS, 0x00);     //no interrupts
    write_port_8(COM1_LINE_CONTROL, 0x80);   //next two writes set the speed
    write_port_8(COM1_DATA, 0x01);           //speed divisor 1 = 115200 bits per second
    write_port_8(COM1_INTERRUPTS, 0x00);
    write_port_8(COM1_LINE_CONTROL, 0x03);   //8 data bits, no parity, 1 stop bit
    write_port_8(COM1_FIFO, 0xC7);           //turn on the send receive buffers
    write_port_8(COM1_MODEM_CONTROL, 0x0B);
    serial_port_ready = true;
}

void debug_log(const char* message) {
    if (!serial_port_ready) {
        setup_serial_port();
    }
    for (int i = 0; message[i] != 0; i++) {
        // wait (a little) until the port can take another character
        for (int tries = 0; tries < MAX_WAIT_TRIES; tries++) {
            if (read_port_8(COM1_LINE_STATUS) & LINE_STATUS_READY_TO_SEND) {
                break;
            }
        }
        write_port_8(COM1_DATA, message[i]);
    }
}
