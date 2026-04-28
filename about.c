#include "types.h"
#include "user.h"
#include "window.h"
#include "gui.h"

#define ABOUT_X 100
#define ABOUT_Y 50
#define ABOUT_W 120
#define ABOUT_H 100
#define SCREEN_W 320
#define SCREEN_H 200

static void
choose_window_origin(int *x, int *y)
{
  struct window wins[MAX_WINDOWS];
  int n;
  int slot;

  n = win_snapshot(wins, MAX_WINDOWS);
  if(n < 1)
    n = 1;

  slot = (n - 1) % 5;
  *x = ABOUT_X + slot * 10;
  *y = ABOUT_Y + slot * 8;
  if(*x + ABOUT_W > SCREEN_W)
    *x = SCREEN_W - ABOUT_W;
  if(*y + ABOUT_H > SCREEN_H)
    *y = SCREEN_H - ABOUT_H;
}

static void
draw_about(int win, int x, int y)
{
  int title_color;

  title_color = (win_get_focus() == win) ? 1 : 8;
  draw_rect(x, y, ABOUT_W, ABOUT_H, 7);
  draw_rect(x, y, ABOUT_W, 12, title_color);
  draw_rect(x + 2, y + 2, 8, 8, 4);
  draw_string(x + 40, y + 4, "ABOUT", 15);
  draw_string(x + 10, y + 20, "CS 461", 0);

  draw_rect(x + 20, y + 75, 80, 15, 4);
  draw_string(x + 35, y + 80, "SHUTDOWN", 15);
}

int main(void) {
  int x;
  int y;

  choose_window_origin(&x, &y);

  // 1. Request a window from the kernel
  int win = win_create(x, y, ABOUT_W, ABOUT_H);
  if (win < 0) {
    printf(1, "about: failed to create window\n");
    exit();
  }
  
  // 2. Focus the window to receive events
  win_focus(win);

  draw_about(win, x, y);

  // 5. The Application Event Loop
  struct win_event ev;
  int old_buttons = 0;

  while (1) {
    if (win_poll(win, &ev) > 0) {
      
	      // Keep the keyboard shortcut as a fallback
	      if (ev.type == WIN_EV_KEY && ev.a == 'q') {
		printf(1, "ABOUT: 'q' pressed! Initiating shutdown...\n");
        break;
      }

	      if (ev.type == WIN_EV_REDRAW) {
	        draw_about(win, x, y);
	        continue;
	      }
      
	      if (ev.type == WIN_EV_TICK)
	        continue;

	      // --- CLICK DETECTION ---
	      if (ev.type == WIN_EV_MOUSE) {
        // Unpack the relative X and Y coordinates sent by desktop.c
        short rel_x = (short)(ev.a & 0xFFFF);
        short rel_y = (short)((ev.a >> 16) & 0xFFFF);
        int buttons = ev.b;

        // Detect a fresh left-click
        if ((buttons & 1) && !(old_buttons & 1)) {
          printf(1, "ABOUT: Received Click at Rel(%d, %d)\n", rel_x, rel_y);
          // The window frame starts at absolute (100, 50).
          // The red button is drawn at absolute (102, 52).
          // Therefore, its RELATIVE position is x=2, y=2. Its width/height is 8.
          if (rel_x >= 0 && rel_x <= 15 && rel_y >= 0 && rel_y <= 15) {
	    printf(1, "ABOUT: Close button hit! Initiating shutdown...\n");
            break; // Break the loop to destroy the window!
          }

		  if (rel_x >= 20 && rel_x <= 100 && rel_y >= 75 && rel_y <= 90) {
			  printf(1, "SYSTEM: Initiating ACPI Shutdown...\n");
			  halt(); // Call your new system call!
		  }
	        }
	        if ((buttons & 1) && !(old_buttons & 1))
	          draw_about(win, x, y);
	        old_buttons = buttons;
	      }
    }
  }

  printf(1, "ABOUT: Destroying window and sending flare...\n");
  // 6. Clean up and terminate
  win_destroy(win);
  int comp_id = win_get_compositor();
  printf(1, "ABOUT: Dropping flare to Desktop (ID: %d)\n", comp_id);
  //Wake up Desktop
  struct win_event wake;
  wake.type = WIN_EV_MOUSE;
  wake.a = 0;
  wake.b = 0;
  win_post_event(comp_id, &wake);

  //Terminate process
  exit();
}
