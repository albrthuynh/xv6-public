#include "types.h"
#include "defs.h"
#include "x86.h"
#include "window.h"
#include "traps.h"

#define PS2_CTRL 0x64
#define PS2_DATA 0x60

//Mouse States
static uchar mouse_cycle = 0;
static char mouse_byte[3];

extern int windowgetfocus(void);
extern int windowpostevent(int, struct win_event*);

// Wait until the PS/2 controller is ready to accept a command
static void mouse_wait_write() {
	  while ((inb(PS2_CTRL) & 2) != 0);
}

// Wait until the PS/2 controller has data to read
static void mouse_wait_read() {
	  while ((inb(PS2_CTRL) & 1) == 0);
}

// Send a command to the mouse device itself
 static void mouse_write(uchar write) {
	mouse_wait_write();
	outb(PS2_CTRL, 0xD4); // Tell controller we are writing to the auxillary device
	mouse_wait_write();
	outb(PS2_DATA, write);
	mouse_wait_read();
	inb(PS2_DATA);
 }

void mouseinit(void) {
	mouse_wait_write();
	outb(PS2_CTRL, 0xA8);
	// Enable interrupts (IRQ 12)
	mouse_wait_write();
	outb(PS2_CTRL, 0x20); // Read Compaq Status Register
	mouse_wait_read();
	uchar status = inb(PS2_DATA) | 2; // Set bit 1 to enable IRQ 12
	mouse_wait_write();
	outb(PS2_CTRL, 0x60); // Write Compaq Status Register
	mouse_wait_write();
	outb(PS2_DATA, status);
	// Set default settings and enable data reporting
	mouse_write(0xF6); // Set defaults
	mouse_write(0xF4); // Enable data reporting

	ioapicenable(IRQ_MOUSE, 0); // Enable the mouse interrupt in the APIC
}

void mouseintr(void) {
	uchar status = inb(PS2_CTRL);

	// Check if data is available (bit 0) and it's from the mouse (bit 5)

	while ((status & 1) && (status & 0x20)) {
		char data = inb(PS2_DATA);
		switch (mouse_cycle) {
			case 0:
				// Byte 0 must have bit 3 set to 1 in standard PS/2 protocol
				if ((data & 0x08) == 0) break;
				mouse_byte[0] = data;
			        mouse_cycle++;
				break;
			case 1:
				mouse_byte[1] = data;
				mouse_cycle++;
				break;
			case 2:
				mouse_byte[2] = data;
				int dx = mouse_byte[1];
				int dy = mouse_byte[2];
				int buttons = mouse_byte[0] & 0x07; // Left (bit 0), Right (bit 1), Middle (bit 2)

				// PS/2 dx/dy are 9-bit two's complement. Extend the sign bit if set in Byte 0.
				if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
				if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;

				int focused = windowgetfocus();
			        if (focused >= 0) {
					struct win_event ev;
					ev.type = WIN_EV_MOUSE;
					// Pack dx and dy into 'a' (lower 16 bits for dx, upper 16 for dy)
					ev.a = (dx & 0xFFFF) | ((dy & 0xFFFF) << 16);
					ev.b = buttons;
					windowpostevent(focused, &ev);
				}
				mouse_cycle = 0;
			        break;
		}
		status = inb(PS2_CTRL); // Check if more data is buffered
	}
}
