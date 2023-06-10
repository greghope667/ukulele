# Boot description

Kernel boot + initialisation order, once out of the bootloader.

## Phase 1 - verification

- Read Limine date + verify stuff needed for later stages
- Setup debug serial/framebuffer output
- Print Limine data

## Phase 2 - memory/heap

- Free unused page map entries
- Allocate virtual + physical memory for heap

## Phase 3 - ACPI/PCI(e) enumeration

## Phase 4 - USB enumeration

## Phase 5 - Interrupts + exceptions

## Phase 6 - Keyboard
