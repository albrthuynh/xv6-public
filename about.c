#include "types.h"
#include "user.h"
#include "window.h"
#include "gui.h"

int main(void) {
  // 1. Request a window from the kernel
  int win = win_create(100, 50, 120, 80);
  if (win < 0) {
    printf(1, "about: failed to create window\n");
    exit();
  }
  
  // 2. Focus the window to receive events
  win_focus(win);

  // 3. Draw the Application UI (Absolute Coordinates)
  draw_rect(100, 50, 120, 80, 7); // Light gray application background
  draw_rect(100, 50, 120, 12, 1); // Blue title bar
  draw_rect(102, 52, 8, 8, 4);    // Red decorative "close" button
  
  // 4. Draw the Important Information
  draw_string(140, 54, "ABOUT", 15); // Draw white title text in the blue bar
  draw_string(110, 70, "CS 461", 0); // Draw black text in the body

  draw_rect(120, 125, 80, 15, 4);   // Big Red Button
  draw_string(135, 130, "SHUTDOWN", 15);

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
