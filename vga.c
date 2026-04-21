#include "types.h"
#include "defs.h"
#include "memlayout.h"

// Map to the virtual address of the VGA buffer
uchar *framebuffer = (uchar *)P2V(0xA0000);

void draw_pixel(int x, int y, uchar color) {
	    if (x < 0 || x >= 320 || y < 0 || y >= 200) return;
	        framebuffer[y * 320 + x] = color;
}

void draw_rect(int x, int y, int w, int h, uchar color) {
	    for (int i = y; i < y + h; i++) {
		            for (int j = x; j < x + w; j++) {
				                draw_pixel(j, i, color);
						        }
			        }
}

