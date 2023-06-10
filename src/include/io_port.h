#pragma once

#include <stdint.h>

inline static void 
io_port_write (uint16_t addr, uint8_t val)
{
	asm volatile ( "outb\t%1, %0" : : "d" (addr), "a" (val) );
}

inline static uint8_t 
io_port_read (uint16_t addr)
{
	uint8_t val;
	asm volatile ( "inb\t%1, %0" : "=a" (val) : "d" (addr) );
	return val;
}
