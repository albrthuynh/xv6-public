#include "types.h"
#include "user.h"
#include "window.h"

int
main(void)
{
  int w1;
  int w2;
  int w3;
  int focused;
  int n;
  int i;
  struct window wins[MAX_WINDOWS];
  struct win_event ev;

  w1 = win_create(10, 10, 200, 120);
  w2 = win_create(30, 25, 220, 140);
  w3 = win_create(50, 40, 240, 160);

  if (w1 < 0 || w2 < 0 || w3 < 0) {
    printf(1, "wm: create failed\n");
    exit();
  }

  focused = win_get_focus();
  printf(1, "wm: created ids=%d,%d,%d focused=%d\n", w1, w2, w3, focused);

  n = win_snapshot(wins, MAX_WINDOWS);
  if (n < 0) {
    printf(1, "wm: snapshot failed\n");
    exit();
  }

  printf(1, "wm: z-order back->front");
  for (i = 0; i < n; i++) {
    printf(1, " %d", wins[i].id);
  }
  printf(1, "\n");

  if (win_focus(w1) < 0) {
    printf(1, "wm: focus failed\n");
    exit();
  }

  focused = win_get_focus();
  printf(1, "wm: focused after focus(w1)=%d\n", focused);

  n = win_snapshot(wins, MAX_WINDOWS);
  printf(1, "wm: z-order after focus(w1)");
  for (i = 0; i < n; i++) {
    printf(1, " %d", wins[i].id);
  }
  printf(1, "\n");

  if (win_poll(w1, &ev) > 0) {
    printf(1, "wm: event type=%d id=%d\n", ev.type, ev.window_id);
  }

  if (win_destroy(w1) < 0) {
    printf(1, "wm: destroy w1 failed\n");
  }
  focused = win_get_focus();
  printf(1, "wm: focused after destroy(w1)=%d\n", focused);

  if (win_destroy(w2) < 0) {
    printf(1, "wm: destroy w2 failed\n");
  }
  if (win_destroy(w3) < 0) {
    printf(1, "wm: destroy w3 failed\n");
  }

  exit();
}
