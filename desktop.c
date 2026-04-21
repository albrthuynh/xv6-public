#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "window.h"

#define SCREEN_W 320
#define SCREEN_H 200

uchar wallpaper_buf[SCREEN_W * SCREEN_H];

void draw_ui() {
  // 1. macOS Menu Bar (Top)
  draw_rect(0, 0, SCREEN_W, 12, 15); // White bar
  draw_rect(5, 2, 8, 8, 0);          // Fake "Apple" logo in black

  // 2. macOS Dock (Bottom)
  draw_rect(100, 180, 120, 20, 7);   // Light gray dock background

  // Dock Icon 1: Terminal (Dark Gray)
  draw_rect(110, 182, 16, 16, 8); 
  // Dock Icon 2: File Explorer (Blue)
  draw_rect(140, 182, 16, 16, 1); 
}

void load_wallpaper() {
  int fd = open("wallpaper", O_RDONLY);
  if(fd < 0){
    // Fallback if no wallpaper file exists: solid Cyan background
    draw_rect(0, 0, SCREEN_W, SCREEN_H, 3); 
    return;
  }else{
    read(fd, wallpaper_buf, sizeof(wallpaper_buf));
    close(fd);
  }

  // Read a 320x200 raw 8-bit color array from the xv6 filesystem
  // We read line-by-line to save user-space memory
  for(int y = 0; y < SCREEN_H; y++) {
    for(int x = 0; x < SCREEN_W; x++) {
	draw_pixel(x, y, wallpaper_buf[y * SCREEN_W + x]);
    }
  }
}

int main(void) {
  int bg_win;
  struct win_event ev;
  int mouse_x = SCREEN_W / 2, mouse_y = SCREEN_H / 2;
  int old_x = mouse_x, old_y = mouse_y;
  int old_buttons = 0;
  uchar cursor_backup[5][5];
  int last_num_wins = 0;

  bg_win = win_create(0, 0, SCREEN_W, SCREEN_H);
  if (bg_win < 0) exit();

  win_set_compositor(bg_win);

  load_wallpaper();
  draw_ui();
  for(int i = 0; i < 5; i++) {
    for(int j = 0; j < 5; j++) {
	cursor_backup[i][j] = read_pixel(mouse_x + j, mouse_y + i);
    }
  }
  draw_rect(mouse_x, mouse_y, 5, 5, 15); // Draw initial cursor

  while (1) {
    if (win_poll(bg_win, &ev) > 0) {
      if (ev.type == WIN_EV_KEY) {
	int focused = win_get_focus();
	// Forward the keystroke to whatever app currently holds focus
	if (focused >= 0 && focused != bg_win) {
		win_post_event(focused, &ev);
	}
      }

      if (ev.type == WIN_EV_MOUSE) {
        short dx = (short)(ev.a & 0xFFFF);
        short dy = (short)((ev.a >> 16) & 0xFFFF);
        int buttons = ev.b;

        mouse_x += dx;
        mouse_y -= dy; 
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= SCREEN_W - 5) mouse_x = SCREEN_W - 5;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= SCREEN_H - 5) mouse_y = SCREEN_H - 5;

	struct window wins[MAX_WINDOWS];
	int num_wins = win_snapshot(wins, MAX_WINDOWS);
	int handled = 0;

	if (num_wins < last_num_wins) {
	  printf(1, "DESKTOP: Window closed! Wiping screen...\n");

	  draw_bitmap(0, 0, SCREEN_W, SCREEN_H, wallpaper_buf);
	  
	  draw_ui();

	  for (int i = 0; i < 5; i++) {
	    for (int j = 0; j < 5; j++) {
		    cursor_backup[i][j] = read_pixel(old_x + j, old_y + i);
	    }
	  }
	}

	  last_num_wins = num_wins;
	// Iterate backwards (from front of the screen to the back Z-order)
	for (int i = num_wins - 1; i >= 0; i--) {
		if (wins[i].id == bg_win) continue; // Skip the desktop background
		// Check if the mouse is hovering over this application window
		if (mouse_x >= wins[i].x && mouse_x <= wins[i].x + wins[i].w &&
		    mouse_y >= wins[i].y && mouse_y <= wins[i].y + wins[i].h) {
			// 1. If left-clicked, bring this window to focus
			if ((buttons & 1) && !(old_buttons & 1)) {
				win_focus(wins[i].id);
			}
			struct win_event fw_ev;
			fw_ev.type = WIN_EV_MOUSE;
			int rel_x = mouse_x - wins[i].x;
			int rel_y = mouse_y - wins[i].y;
			fw_ev.a = (rel_x & 0xFFFF) | ((rel_y & 0xFFFF) << 16);
			fw_ev.b = buttons;

			if ((buttons & 1) && !(old_buttons & 1)) {
				printf(1, "DESKTOP: Routed click to Win %d at Rel(%d, %d)\n", wins[i].id, rel_x, rel_y);
			}

			win_post_event(wins[i].id, &fw_ev);
			handled = 1;
		break;
		}
	}

        // --- CLICK DETECTION LOGIC ---
        // Check if Left Click just transitioned from unpressed (0) to pressed (1)
	if (!handled && (buttons & 1) && !(old_buttons & 1)) { 
	// Top Bar
		if (mouse_y >= 2 && mouse_y <= 10 && mouse_x >= 5 && mouse_x <= 13) {
			if (fork() == 0) {
				char *args[] = {"about", 0};
				exec("about", args);
				exit();
			}
                }

		// Check if the Y-coordinate falls within the dock height
		if (mouse_y >= 182 && mouse_y <= 198) {

		// Terminal Icon bounding box
		if (mouse_x >= 110 && mouse_x <= 126) {
			if (fork() == 0) {
				char *args[] = {"terminal", 0};
				exec("terminal", args);
				exit();
			}
		}
		// Explorer Icon bounding box
		if (mouse_x >= 140 && mouse_x <= 156) {
			if (fork() == 0) {
				char *args[] = {"explorer", 0};
				exec("explorer", args);
				exit();
			}
		}
		}
        }
        old_buttons = buttons;

        // Erase old cursor (fallback to cyan)
	for (int i = 0; i < 5; i++) {
           for (int j = 0; j < 5; j++) {
		draw_pixel(old_x + j, old_y + i, cursor_backup[i][j]);
	   }
	}

        // If the cursor wiped out the UI, quickly redraw it
        if (old_y >= 180 || old_y <= 12) draw_ui(); 

	old_x = mouse_x;
	old_y = mouse_y;

	for (int i = 0; i < 5; i++) {
	  for (int j = 0; j < 5; j++) {
	    cursor_backup[i][j] = read_pixel(mouse_x + j, mouse_y + i);
	  }
	}

        // Draw new cursor
        draw_rect(mouse_x, mouse_y, 5, 5, 15); 
      }
    }
  }
  exit();
}
