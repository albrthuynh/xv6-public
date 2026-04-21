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

int read_pixel(int x, int y) {
	if (x < 0 || x >= 320 || y < 0 || y >= 200) return 0;
	return framebuffer[y * 320 + x];
}

void draw_bitmap(int x, int y, int w, int h, uchar *buf) {
	for(int i = 0; i < h; i++) {
		for(int j = 0; j < w; j++) {
			if (x + j < 320 && y + i < 200) {
				// Use color 255 as a "transparent" pixel mask
				if (buf[i * w + j] != 255) {
					framebuffer[(y + i) * 320 + (x + j)] = buf[i * w + j];
				}
			}
		}
	}
}
