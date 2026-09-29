#pragma once
#include "types.h"

struct PciDevice {
    bool found;
    int bus, slot, function;
};

u32 pci_read(const PciDevice& device, int offset);
void pci_write(const PciDevice& device, int offset, u32 value);

PciDevice pci_find_device_by_class(u16 class_and_subclass);


const int PCI_VENDOR_AND_DEVICE = 0x00;
const int PCI_COMMAND = 0x04;
const int PCI_CLASS = 0x08;
const int PCI_BAR0 = 0x10;      //where the device's registers are
const int PCI_BAR1 = 0x14;
const u32 PCI_COMMAND_IO_SPACE = 0x1;
const u32 PCI_COMMAND_BUS_MASTER = 0x4;
const u32 PCI_BAR_IS_IO_PORT = 0x1;
