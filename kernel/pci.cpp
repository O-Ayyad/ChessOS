#include "pci.h"
#include "cpu.h"

const u16 PCI_ADDRESS_PORT = 0xCF8;
const u16 PCI_DATA_PORT = 0xCFC;
const u32 PCI_ENABLE = 0x80000000;
const int BUSES_TO_SEARCH = 8;
const int SLOTS_PER_BUS = 32;
const int FUNCTIONS_PER_SLOT = 8;
const u32 NO_DEVICE = 0xFFFF;

static u32 config_address(const PciDevice& device, int offset) {
    return PCI_ENABLE | (device.bus << 16) | (device.slot << 11) | (device.function << 8) | (offset & 0xFC);
}

u32 pci_read(const PciDevice& device, int offset) {
    write_port_32(PCI_ADDRESS_PORT, config_address(device, offset));
    return read_port_32(PCI_DATA_PORT);
}

void pci_write(const PciDevice& device, int offset, u32 value) {
    write_port_32(PCI_ADDRESS_PORT, config_address(device, offset));
    write_port_32(PCI_DATA_PORT, value);
}

PciDevice pci_find_device_by_class(u16 class_and_subclass) {
    PciDevice device;
    for (int bus = 0; bus < BUSES_TO_SEARCH; bus++) {
        for (int slot = 0; slot < SLOTS_PER_BUS; slot++) {
            for (int function = 0; function < FUNCTIONS_PER_SLOT; function++) {
                device.found = false;
                device.bus = bus;
                device.slot = slot;
                device.function = function;
                u32 ids = pci_read(device, PCI_VENDOR_AND_DEVICE);
                if ((ids & 0xFFFF) == NO_DEVICE) {
                    if (function == 0) break;   // empty slot
                    continue;
                }
                u32 class_register = pci_read(device, PCI_CLASS);
                if ((class_register >> 16) == class_and_subclass) {
                    device.found = true;
                    return device;
                }
            }
        }
    }
    device.found = false;
    return device;
}
