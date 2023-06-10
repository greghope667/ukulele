#include "serial.h"
#include "io_port.h"
#include "x86.h"

#include <stdint.h>

const uint16_t serial_address[] = {
	[SERIAL_PORT_1] = 0x3f8,
	[SERIAL_PORT_2] = 0x2f8,
	[SERIAL_PORT_3] = 0x3e8,
	[SERIAL_PORT_4] = 0x2e8,
};

/* Serial port - in IO space
 * struct serial {
 * 	union {
 * 		struct {
 * 			uint8_t data;
 * 			uint8_t interrupt_enable;
 * 		};
 * 		uint16le_t baud_divisor;
 * 	};
 * 	uint8_t int_id_fifo;
 * 	uint8_t line_control;
 * 	uint8_t modem_control;
 * 	uint8_t line_status;
 * 	uint8_t modem_status;
 * 	uint8_t scratch;
 * }
 */

/* Constants for chip NS16550 */
#define REG_BAUD_LOW 				0	// With divisor latch
#define REG_BAUD_HIGH				1	// With divisor latch
#define REG_DATA 					0
#define REG_INTERRUPT_ENABLE 		1
#define REG_INTERRUPT_ID 			2	// r
#define REG_FIFO_CONTROL 			2	// w
#define REG_LINE_CONTROL 			3
#define REG_MODEM_CONTROL 			4
#define REG_LINE_STATUS 			5
#define REG_MODEM_STATUS 			6
#define REG_SCRATCH 				7

#define INT_ENABLE_RX_AVAIL			0x01
#define INT_ENABLE_TX_EMPTY			0x02
#define INT_ENABLE_LINE_STATUS		0x04
#define INT_ENABLE_MODEM_STATUS		0x08

#define FIFO_CTRL_ENABLE			0x01
#define FIFO_CTRL_CLEAR_RX			0x02
#define FIFO_CTRL_CLEAR_TX			0x04
#define FIFO_CTRL_RDY_EN			0x08
#define FIFO_CTRL_TRIGGER_01		0x00
#define FIFO_CTRL_TRIGGER_04		0x40
#define FIFO_CTRL_TRIGGER_08		0x80
#define FIFO_CTRL_TRIGGER_14		0xC0

#define LINE_CTRL_DIV_LATCH			0x80
#define LINE_CTRL_8N1				0x03

#define MODEM_CTRL_DTR				0x01
#define MODEM_CTRL_RTS				0x02
#define MODEM_CTRL_ENABLE_IRQ		0x08 // a.k.a. OUT2
#define MODEM_CTRL_LOOPBACK			0x10

#define LINE_STATUS_DATA_READY		0x01

// TODO: add remaining register bits when needed

#define MAX_BAUD_RATE 				115200U

/* Configurable options */
#define BAUD_RATE 					9600U
#define SCRATCH_TEST_BYTE 			((char)0xAB) // arbitrary value
#define LOOP_TEST_BYTE 				((char)0x69) // arbitrary value

static inline void
set_div_latch (uint16_t port, bool enabled)
{
	uint16_t line_control = port + REG_LINE_CONTROL;
	uint8_t val = io_port_read (line_control);
	uint8_t new;

	if (enabled)
		new = val | LINE_CTRL_DIV_LATCH;
	else
		new = val & ~LINE_CTRL_DIV_LATCH;

	if (new != val)
		io_port_write (line_control, new);
}

bool
serial_detect (serial_port_id n)
{
	const uint16_t port = serial_address[n];

	io_port_write (port + REG_SCRATCH, SCRATCH_TEST_BYTE);
	if (SCRATCH_TEST_BYTE != (char)io_port_read (port + REG_SCRATCH))
		return false;

	// Disable interrupts
	set_div_latch (port, false);
	io_port_write (port + REG_INTERRUPT_ENABLE, 0);

	// Set BAUD
	set_div_latch (port, true);
	io_port_write (port + REG_BAUD_LOW, (MAX_BAUD_RATE / BAUD_RATE) & 0xff);
	io_port_write (port + REG_BAUD_HIGH, (MAX_BAUD_RATE / BAUD_RATE) >> 8);

	// Configure reasonable defaults
	io_port_write (port + REG_LINE_CONTROL, LINE_CTRL_8N1);
	io_port_write (port + REG_FIFO_CONTROL,
				   FIFO_CTRL_ENABLE
				   | FIFO_CTRL_CLEAR_RX
				   | FIFO_CTRL_CLEAR_TX
				   | FIFO_CTRL_TRIGGER_08);
	io_port_write (port + REG_MODEM_CONTROL,
				   MODEM_CTRL_DTR | MODEM_CTRL_RTS | MODEM_CTRL_LOOPBACK);

	// Test loopback
	io_port_write (port, LOOP_TEST_BYTE);
	if (io_port_read (port) != LOOP_TEST_BYTE)
		return false;

	io_port_write (port + REG_MODEM_CONTROL,
				   MODEM_CTRL_DTR | MODEM_CTRL_RTS);

	return true;
}

void
serial_write (serial_port_id n, char c)
{
	io_port_write (serial_address[n], c);
}

int
serial_read (serial_port_id n)
{
	uint16_t port = serial_address[n];
	uint8_t line = io_port_read (port + REG_LINE_STATUS);

	if (line & LINE_STATUS_DATA_READY)
		return (unsigned char) io_port_read (port);
	else
		return -1;
}
