#include "types.h"
#include "user.h"
#include "window.h"

#define SCREEN_W 320
#define SCREEN_H 200
#define MAX_BENCH_WINDOWS 4
#define FOCUS_ITERS 100

static void
print_elapsed(char *label, int ticks)
{
  if(ticks == 0)
    printf(1, "%s: 0 ticks, <10 ms\n", label);
  else
    printf(1, "%s: %d ticks, approx %d ms\n", label, ticks, ticks * 10);
}

static void
draw_bench_desktop(void)
{
  draw_rect(0, 0, SCREEN_W, SCREEN_H, 9);
  draw_rect(0, 0, SCREEN_W, 12, 15);
  draw_rect(5, 2, 8, 8, 0);
  draw_rect(100, 180, 120, 20, 7);
  draw_rect(110, 182, 16, 16, 8);
  draw_rect(140, 182, 16, 16, 1);
}

static void
draw_bench_window(int x, int y, int w, int h, int focused)
{
  draw_rect(x, y, w, h, 7);
  draw_rect(x, y, w, 12, focused ? 1 : 8);
  draw_rect(x + 2, y + 2, 8, 8, 4);
  draw_rect(x + 6, y + 18, w - 12, h - 24, 15);
  draw_rect(x + 12, y + 28, w - 24, 8, focused ? 1 : 8);
}

static int
create_windows(int *wins, int count)
{
  int xs[MAX_BENCH_WINDOWS] = {20, 44, 68, 92};
  int ys[MAX_BENCH_WINDOWS] = {22, 38, 54, 70};
  int ws[MAX_BENCH_WINDOWS] = {160, 166, 172, 178};
  int hs[MAX_BENCH_WINDOWS] = {92, 92, 92, 92};

  for(int i = 0; i < count; i++) {
    wins[i] = win_create(xs[i], ys[i], ws[i], hs[i]);
    if(wins[i] < 0)
      return -1;
  }
  return 0;
}

static void
destroy_windows(int *wins, int count)
{
  for(int i = 0; i < count; i++) {
    if(wins[i] >= 0)
      win_destroy(wins[i]);
  }
}

static void
bench_redraw(int *wins, int count)
{
  int xs[MAX_BENCH_WINDOWS] = {20, 44, 68, 92};
  int ys[MAX_BENCH_WINDOWS] = {22, 38, 54, 70};
  int ws[MAX_BENCH_WINDOWS] = {160, 166, 172, 178};
  int hs[MAX_BENCH_WINDOWS] = {92, 92, 92, 92};
  int start;
  int end;

  start = uptime();
  draw_bench_desktop();
  for(int i = 0; i < count; i++)
    draw_bench_window(xs[i], ys[i], ws[i], hs[i], wins[i] == win_get_focus());
  end = uptime();

  if(count == 1)
    print_elapsed("redraw 1 window", end - start);
  else if(count == 2)
    print_elapsed("redraw 2 windows", end - start);
  else
    print_elapsed("redraw 4 windows", end - start);
}

static void
bench_focus(int *wins, int count)
{
  int start;
  int end;
  int ticks;
  int avg_ms;

  start = uptime();
  for(int i = 0; i < FOCUS_ITERS; i++)
    win_focus(wins[i % count]);
  end = uptime();

  ticks = end - start;
  avg_ms = (ticks * 10) / FOCUS_ITERS;

  if(count == 1)
    printf(1, "focus 1 window: ");
  else if(count == 2)
    printf(1, "focus 2 windows: ");
  else
    printf(1, "focus 4 windows: ");

  if(ticks == 0)
    printf(1, "0 ticks total, <10 ms total for %d switches\n", FOCUS_ITERS);
  else
    printf(1, "%d ticks total, approx %d ms total, approx %d ms/switch\n",
           ticks, ticks * 10, avg_ms);
}

int
main(void)
{
  int counts[3] = {1, 2, 4};
  int wins[MAX_BENCH_WINDOWS];

  printf(1, "bench: timing uses uptime ticks, approx 10 ms each\n");

  for(int t = 0; t < 3; t++) {
    int count = counts[t];

    for(int i = 0; i < MAX_BENCH_WINDOWS; i++)
      wins[i] = -1;

    if(create_windows(wins, count) < 0) {
      printf(1, "bench: window create failed for %d windows\n", count);
      destroy_windows(wins, count);
      exit();
    }

    bench_redraw(wins, count);
    bench_focus(wins, count);
    destroy_windows(wins, count);
  }

  printf(1, "bench: done\n");
  exit();
}
