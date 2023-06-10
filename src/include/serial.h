#pragma once

#include <stdbool.h>

typedef enum {
	SERIAL_PORT_1,
	SERIAL_PORT_2,
	SERIAL_PORT_3,
	SERIAL_PORT_4,
} serial_port_id;

/* Detect, and attempt to initialise, given serial port
 * Returns True if serial port is detected + initialised
 */
bool serial_detect (serial_port_id);

void serial_write (serial_port_id, char);
int serial_read (serial_port_id); /* -1(EOF) on no data */
