#include "types.h"
#include "user.h"
#include "window.h"

#define SCREEN_W 320
#define SCREEN_H 200

int main(void) {
  int bg_win;
  struct win_event ev;
  int mouse_x = SCREEN_W / 2;
  int mouse_y = SCREEN_H / 2;
  int old_x = mouse_x, old_y = mouse_y;

  // 1. Create the background desktop window
  bg_win = win_create(0, 0, SCREEN_W, SCREEN_H);
  if (bg_win < 0) {
    printf(1, "desktop: window creation failed\n");
    exit();
  }

  // 2. Give it focus so your kernel mouse driver routes events to it
  win_focus(bg_win);

  // 3. Clear the screen to blue (VGA color 1)
  draw_rect(0, 0, SCREEN_W, SCREEN_H, 1);

  // 4. Draw the initial cursor (VGA white is 15)
  draw_rect(mouse_x, mouse_y, 5, 5, 15);

  // 5. The Event Loop
  while (1) {
    // Poll Albert's event queue!
    if (win_poll(bg_win, &ev) > 0) {
      
      if (ev.type == WIN_EV_MOUSE) {
        // Unpack dx and dy from ev.a (lower 16 bits = dx, upper 16 = dy)
        short dx = (short)(ev.a & 0xFFFF);
        short dy = (short)((ev.a >> 16) & 0xFFFF);

        // Update coordinates (Note: PS/2 hardware sends +dy for UP, but VGA +Y is DOWN)
        mouse_x += dx;
        mouse_y -= dy; 

        // Clamp the cursor to the screen edges so it doesn't disappear
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= SCREEN_W - 5) mouse_x = SCREEN_W - 5;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= SCREEN_H - 5) mouse_y = SCREEN_H - 5;

        // Erase the old cursor by drawing a blue square over it
        draw_rect(old_x, old_y, 5, 5, 1);
        
        // Draw the new cursor in white
        draw_rect(mouse_x, mouse_y, 5, 5, 15);

        old_x = mouse_x;
        old_y = mouse_y;
      }
    }
  }
  exit();
}
