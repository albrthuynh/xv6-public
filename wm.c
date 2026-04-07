#include "types.h"
#include "user.h"
#include "window.h"

int
main(void)
{
  int winid;
  int seen_ticks;
  int loops;
  struct win_event ev;

  winid = win_create(20, 20, 320, 200);
  if (winid < 0) {
    printf(1, "wm: win_create failed\n");
    exit();
  }

  printf(1, "wm: created window id=%d\n", winid);
  printf(1, "wm: event loop started\n");

  seen_ticks = 0;
  loops = 0;
  for (;;) {
    int r;

    r = win_poll(winid, &ev);
    if (r < 0) {
      printf(1, "wm: win_poll failed\n");
      break;
    }
    if (r == 0) {
      sleep(5);
      continue;
    }

    if (ev.type == WIN_EV_TICK) {
      seen_ticks++;
      if ((seen_ticks % 100) == 0) {
        printf(1, "wm: tick events=%d\n", seen_ticks);
      }
    } else if (ev.type == WIN_EV_CLOSE) {
      printf(1, "wm: close event\n");
      break;
    }

    loops++;
    if (loops > 2000)
      break;
  }

  if (win_destroy(winid) < 0)
    printf(1, "wm: win_destroy failed\n");
  else
    printf(1, "wm: destroyed window id=%d\n", winid);

  exit();
}
