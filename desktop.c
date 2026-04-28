#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "window.h"

#define SCREEN_W 320
#define SCREEN_H 200
#define CURSOR_W 10
#define CURSOR_H 14

uchar wallpaper_buf[SCREEN_W * SCREEN_H];

static char *wallpaper_files[] = {
  "wallpaper",
  "wallpaper_grid",
  "wallpaper_party",
  "wallpaper_night",
};

#define WALLPAPER_COUNT (sizeof(wallpaper_files) / sizeof(wallpaper_files[0]))

static char *cursor_shape[CURSOR_H] = {
  "X         ",
  "XX        ",
  "X.X       ",
  "X..X      ",
  "X...X     ",
  "X....X    ",
  "X.....X   ",
  "X......X  ",
  "X...XXXX  ",
  "X..X      ",
  "X.X       ",
  "XX        ",
  "X         ",
  "          ",
};

static void
save_cursor_area(int x, int y, uchar backup[CURSOR_H][CURSOR_W])
{
  for(int i = 0; i < CURSOR_H; i++) {
    for(int j = 0; j < CURSOR_W; j++) {
      backup[i][j] = read_pixel(x + j, y + i);
    }
  }
}

static void
restore_cursor_area(int x, int y, uchar backup[CURSOR_H][CURSOR_W])
{
  for(int i = 0; i < CURSOR_H; i++) {
    for(int j = 0; j < CURSOR_W; j++) {
      draw_pixel(x + j, y + i, backup[i][j]);
    }
  }
}

static void
draw_cursor(int x, int y)
{
  char pixel;

  for(int i = 0; i < CURSOR_H; i++) {
    for(int j = 0; j < CURSOR_W; j++) {
      pixel = cursor_shape[i][j];
      if(pixel == 'X')
        draw_pixel(x + j, y + i, 0);
      else if(pixel == '.')
        draw_pixel(x + j, y + i, 15);
    }
  }
}

static void
draw_menu_apple(int x, int y)
{
  uchar mark[8] = {
    0b00100000,
    0b00010000,
    0b01111000,
    0b11111100,
    0b11101100,
    0b11111100,
    0b01111000,
    0b00110000,
  };

  for(int row = 0; row < 8; row++) {
    for(int col = 0; col < 8; col++) {
      if(mark[row] & (1 << (7 - col)))
        draw_pixel(x + col, y + row, 0);
    }
  }
}

static void
draw_terminal_icon(int x, int y)
{
  draw_rect(x, y, 16, 16, 8);
  draw_rect(x + 1, y + 1, 14, 14, 0);
  draw_rect(x + 3, y + 4, 2, 2, 10);
  draw_rect(x + 5, y + 6, 2, 2, 10);
  draw_rect(x + 3, y + 8, 2, 2, 10);
  draw_rect(x + 8, y + 10, 5, 2, 10);
}

static void
draw_finder_icon(int x, int y)
{
  draw_rect(x, y, 16, 16, 1);
  draw_rect(x + 1, y + 1, 7, 14, 11);
  draw_rect(x + 8, y + 1, 7, 14, 9);
  draw_rect(x + 1, y + 1, 14, 2, 15);

  draw_pixel(x + 5, y + 6, 0);
  draw_pixel(x + 11, y + 6, 0);
  draw_pixel(x + 8, y + 4, 0);
  draw_pixel(x + 7, y + 5, 0);
  draw_pixel(x + 8, y + 6, 0);
  draw_pixel(x + 7, y + 11, 0);
  draw_pixel(x + 8, y + 12, 0);
  draw_pixel(x + 9, y + 12, 0);
  draw_pixel(x + 10, y + 11, 0);
}

static void
draw_wallpaper_icon(int x, int y)
{
  draw_rect(x, y, 16, 16, 13);
  draw_rect(x + 1, y + 1, 14, 14, 15);
  draw_rect(x + 3, y + 3, 10, 5, 11);
  draw_rect(x + 3, y + 8, 10, 5, 10);
  draw_rect(x + 5, y + 10, 3, 3, 2);
  draw_rect(x + 9, y + 9, 3, 4, 4);
  draw_pixel(x + 11, y + 4, 14);
  draw_pixel(x + 12, y + 5, 14);
  draw_pixel(x + 10, y + 5, 14);
  draw_pixel(x + 11, y + 6, 14);
}

static void
request_window_redraws(struct window *wins, int num_wins, int bg_win, int ordered)
{
  struct win_event redraw;

  redraw.type = WIN_EV_REDRAW;
  redraw.a = 0;
  redraw.b = 0;

  for(int i = 0; i < num_wins; i++) {
    if(wins[i].id == bg_win)
      continue;
    redraw.window_id = wins[i].id;
    win_post_event(wins[i].id, &redraw);
    if(ordered)
      sleep(1);
  }
}

static void
request_window_move(int window_id, int x, int y)
{
  struct win_event move;

  move.type = WIN_EV_MOVE;
  move.window_id = window_id;
  move.a = x;
  move.b = y;
  win_post_event(window_id, &move);
}

static void
request_one_redraw(int window_id)
{
  struct win_event redraw;

  redraw.type = WIN_EV_REDRAW;
  redraw.window_id = window_id;
  redraw.a = 0;
  redraw.b = 0;
  win_post_event(window_id, &redraw);
}

void draw_ui() {
  // 1. macOS Menu Bar (Top)
  draw_rect(0, 0, SCREEN_W, 12, 15); // White bar
  draw_menu_apple(5, 2);

  // 2. macOS Dock (Bottom)
  draw_rect(100, 180, 120, 20, 7);   // Light gray dock background

  draw_terminal_icon(110, 182);
  draw_finder_icon(140, 182);
  draw_wallpaper_icon(170, 182);
}

static void
fill_wallpaper(uchar color)
{
  for(int i = 0; i < SCREEN_W * SCREEN_H; i++)
    wallpaper_buf[i] = color;
  draw_bitmap(0, 0, SCREEN_W, SCREEN_H, wallpaper_buf);
}

void load_wallpaper(int wallpaper_id) {
  int fd;
  int n;

  if(wallpaper_id < 0 || wallpaper_id >= WALLPAPER_COUNT)
    wallpaper_id = 0;

  fd = open(wallpaper_files[wallpaper_id], O_RDONLY);
  if(fd < 0){
    // Fallback if no wallpaper file exists: solid Cyan background
    fill_wallpaper(3);
    return;
  }

  n = read(fd, wallpaper_buf, sizeof(wallpaper_buf));
  close(fd);
  if(n != sizeof(wallpaper_buf)) {
    fill_wallpaper(3);
    return;
  }

  // Read a 320x200 raw 8-bit color array from the xv6 filesystem
  draw_bitmap(0, 0, SCREEN_W, SCREEN_H, wallpaper_buf);
}

int main(void) {
  int bg_win;
  struct win_event ev;
  int mouse_x = SCREEN_W / 2, mouse_y = SCREEN_H / 2;
  int old_x = mouse_x, old_y = mouse_y;
  int old_buttons = 0;
  uchar cursor_backup[CURSOR_H][CURSOR_W];
  int last_num_wins = 0;
  int dragging_win = -1;
  int drag_dx = 0;
  int drag_dy = 0;
  int drag_w = 0;
  int drag_h = 0;

  bg_win = win_create(0, 0, SCREEN_W, SCREEN_H);
  if (bg_win < 0) exit();

  win_set_compositor(bg_win);

  load_wallpaper(0);
  draw_ui();
  save_cursor_area(mouse_x, mouse_y, cursor_backup);
  draw_cursor(mouse_x, mouse_y);

  while (1) {
    if (win_poll(bg_win, &ev) > 0) {
      if (ev.type == WIN_EV_WALLPAPER) {
	struct window wins[MAX_WINDOWS];
	int num_wins;

	load_wallpaper(ev.a);
	draw_ui();
	num_wins = win_snapshot(wins, MAX_WINDOWS);
	request_window_redraws(wins, num_wins, bg_win, 1);
	save_cursor_area(mouse_x, mouse_y, cursor_backup);
	draw_cursor(mouse_x, mouse_y);
	last_num_wins = num_wins;
	continue;
      }

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
	        if (mouse_x >= SCREEN_W - CURSOR_W) mouse_x = SCREEN_W - CURSOR_W;
	        if (mouse_y < 0) mouse_y = 0;
	        if (mouse_y >= SCREEN_H - CURSOR_H) mouse_y = SCREEN_H - CURSOR_H;

		struct window wins[MAX_WINDOWS];
		int num_wins = win_snapshot(wins, MAX_WINDOWS);
		int handled = 0;
		int screen_redrawn = 0;

		if (num_wins < last_num_wins) {
		  printf(1, "DESKTOP: Window closed! Wiping screen...\n");

		  draw_bitmap(0, 0, SCREEN_W, SCREEN_H, wallpaper_buf);
		  
		  draw_ui();
		  request_window_redraws(wins, num_wins, bg_win, 1);
		  screen_redrawn = 1;

			  save_cursor_area(old_x, old_y, cursor_backup);
			}

		  last_num_wins = num_wins;

		if (dragging_win >= 0 && !(buttons & 1)) {
		  draw_bitmap(0, 0, SCREEN_W, SCREEN_H, wallpaper_buf);
		  draw_ui();
		  num_wins = win_snapshot(wins, MAX_WINDOWS);
		  request_window_redraws(wins, num_wins, bg_win, 1);
		  dragging_win = -1;
		  screen_redrawn = 1;
		  handled = 1;
		}

		if (dragging_win >= 0 && (buttons & 1)) {
		  int new_x;
		  int new_y;

		  new_x = mouse_x - drag_dx;
		  new_y = mouse_y - drag_dy;
		  if (new_x < 0) new_x = 0;
		  if (new_y < 12) new_y = 12;
		  if (drag_w > 0 && new_x > SCREEN_W - drag_w)
		    new_x = SCREEN_W - drag_w;
		  if (drag_h > 0 && new_y > SCREEN_H - drag_h)
		    new_y = SCREEN_H - drag_h;

		  if (win_move(dragging_win, new_x, new_y) == 0) {
		    draw_bitmap(0, 0, SCREEN_W, SCREEN_H, wallpaper_buf);
		    draw_ui();
		    request_window_move(dragging_win, new_x, new_y);
		    request_one_redraw(dragging_win);
		    screen_redrawn = 1;
		  }
		  handled = 1;
		}

		// Iterate backwards (from front of the screen to the back Z-order)
		for (int i = num_wins - 1; i >= 0 && !handled; i--) {
			if (wins[i].id == bg_win) continue; // Skip the desktop background
			// Check if the mouse is hovering over this application window
			if (mouse_x >= wins[i].x && mouse_x <= wins[i].x + wins[i].w &&
			    mouse_y >= wins[i].y && mouse_y <= wins[i].y + wins[i].h) {
					int target_id = wins[i].id;
					int target_x = wins[i].x;
					int target_y = wins[i].y;
					int target_w = wins[i].w;
					int target_h = wins[i].h;

					// 1. If left-clicked, bring this window to focus
					if ((buttons & 1) && !(old_buttons & 1)) {
						if (win_focus(target_id) == 0) {
							num_wins = win_snapshot(wins, MAX_WINDOWS);
							draw_bitmap(0, 0, SCREEN_W, SCREEN_H, wallpaper_buf);
							draw_ui();
						request_window_redraws(wins, num_wins, bg_win, 1);
						screen_redrawn = 1;
					}
					}
					struct win_event fw_ev;
				fw_ev.type = WIN_EV_MOUSE;
				int rel_x = mouse_x - target_x;
				int rel_y = mouse_y - target_y;
				fw_ev.a = (rel_x & 0xFFFF) | ((rel_y & 0xFFFF) << 16);
				fw_ev.b = buttons;

					if ((buttons & 1) && !(old_buttons & 1)) {
						printf(1, "DESKTOP: Routed click to Win %d at Rel(%d, %d)\n", target_id, rel_x, rel_y);
					}

					if ((buttons & 1) && !(old_buttons & 1) && rel_y <= 12 && rel_x > 15) {
						dragging_win = target_id;
						drag_dx = rel_x;
						drag_dy = rel_y;
						drag_w = target_w;
						drag_h = target_h;
					} else {
						win_post_event(target_id, &fw_ev);
					}
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
		// Wallpaper Icon bounding box
		if (mouse_x >= 170 && mouse_x <= 186) {
			if (fork() == 0) {
				char *args[] = {"wallpick", 0};
				exec("wallpick", args);
				exit();
			}
		}
		}
        }
        old_buttons = buttons;

	        if (!screen_redrawn)
		  restore_cursor_area(old_x, old_y, cursor_backup);

        // If the cursor wiped out the UI, quickly redraw it
        if (old_y >= 180 || old_y <= 12) draw_ui(); 

	old_x = mouse_x;
	old_y = mouse_y;

		save_cursor_area(mouse_x, mouse_y, cursor_backup);

	        // Draw new cursor
	        draw_cursor(mouse_x, mouse_y);
      }
    }
  }
  exit();
}
