#include "types.h"
#include "user.h"
#include "window.h"
#include "gui.h"

#define WALLPICK_X 70
#define WALLPICK_Y 34
#define WALLPICK_W 180
#define WALLPICK_H 118
#define SCREEN_W 320
#define SCREEN_H 200
#define PREVIEW_W 34
#define PREVIEW_H 14

struct wallpaper_choice {
  char *name;
  int color_a;
  int color_b;
  int id;
};

static struct wallpaper_choice choices[] = {
  { "DEFAULT", 3, 9, 0 },
  { "GRID", 11, 9, 1 },
  { "PARTY", 4, 14, 2 },
  { "NIGHT", 1, 2, 3 },
};

static void
choose_window_origin(int *x, int *y)
{
  struct window wins[MAX_WINDOWS];
  int n;
  int slot;

  n = win_snapshot(wins, MAX_WINDOWS);
  if(n < 1)
    n = 1;

  slot = (n - 1) % 4;
  *x = WALLPICK_X + slot * 10;
  *y = WALLPICK_Y + slot * 8;
  if(*x + WALLPICK_W > SCREEN_W)
    *x = SCREEN_W - WALLPICK_W;
  if(*y + WALLPICK_H > SCREEN_H)
    *y = SCREEN_H - WALLPICK_H;
}

static void
draw_choice_preview(int x, int y, struct wallpaper_choice *choice)
{
  int i;
  int j;

  draw_rect(x, y, PREVIEW_W, PREVIEW_H, choice->color_a);
  for(i = 0; i < PREVIEW_H; i++) {
    for(j = 0; j < PREVIEW_W; j++) {
      if(((i + j) % 7) < 3)
        draw_pixel(x + j, y + i, choice->color_b);
    }
  }
  draw_rect(x, y, PREVIEW_W, 1, 0);
  draw_rect(x, y + PREVIEW_H - 1, PREVIEW_W, 1, 0);
  draw_rect(x, y, 1, PREVIEW_H, 0);
  draw_rect(x + PREVIEW_W - 1, y, 1, PREVIEW_H, 0);
}

static void
draw_wallpick(int win, int x, int y)
{
  int title_color;
  int i;

  title_color = (win_get_focus() == win) ? 1 : 8;
  draw_rect(x, y, WALLPICK_W, WALLPICK_H, 7);
  draw_rect(x, y, WALLPICK_W, 12, title_color);
  draw_rect(x + 2, y + 2, 8, 8, 4);
  draw_string(x + 58, y + 4, "WALLS", 15);

  draw_string(x + 12, y + 20, "PICK A VIBE", 0);

  for(i = 0; i < 4; i++) {
    int row_y = y + 34 + i * 19;

    draw_rect(x + 10, row_y - 2, WALLPICK_W - 20, 17, 15);
    draw_choice_preview(x + 14, row_y, &choices[i]);
    draw_string(x + 56, row_y + 6, choices[i].name, 0);
  }
}

static void
send_wallpaper_choice(int id)
{
  int comp_id;
  struct win_event ev;

  comp_id = win_get_compositor();
  if(comp_id < 0)
    return;

  ev.type = WIN_EV_WALLPAPER;
  ev.window_id = comp_id;
  ev.a = id;
  ev.b = 0;
  win_post_event(comp_id, &ev);
}

static void
wake_desktop(void)
{
  int comp_id;
  struct win_event wake;

  comp_id = win_get_compositor();
  if(comp_id < 0)
    return;
  wake.type = WIN_EV_MOUSE;
  wake.window_id = comp_id;
  wake.a = 0;
  wake.b = 0;
  win_post_event(comp_id, &wake);
}

int
main(void)
{
  int x;
  int y;
  int win;
  int old_buttons;
  struct win_event ev;

  choose_window_origin(&x, &y);
  win = win_create(x, y, WALLPICK_W, WALLPICK_H);
  if(win < 0) {
    printf(2, "wallpick: win_create failed\n");
    exit();
  }
  win_focus(win);
  draw_wallpick(win, x, y);

  old_buttons = 0;
  for(;;) {
    if(win_poll(win, &ev) <= 0)
      continue;

    if(ev.type == WIN_EV_CLOSE)
      break;
    if(ev.type == WIN_EV_REDRAW) {
      draw_wallpick(win, x, y);
      continue;
    }
    if(ev.type == WIN_EV_MOVE) {
      x = ev.a;
      y = ev.b;
      continue;
    }
    if(ev.type == WIN_EV_TICK)
      continue;
    if(ev.type == WIN_EV_KEY && (ev.a == 'q' || ev.a == 27))
      break;

    if(ev.type == WIN_EV_MOUSE) {
      int rel_x = (short)(ev.a & 0xFFFF);
      int rel_y = (short)((ev.a >> 16) & 0xFFFF);
      int buttons = ev.b;

      if((buttons & 1) && !(old_buttons & 1)) {
        if(rel_x >= 0 && rel_x <= 15 && rel_y >= 0 && rel_y <= 15)
          break;

        for(int i = 0; i < 4; i++) {
          int row_y = 34 + i * 19;
          if(rel_x >= 10 && rel_x < WALLPICK_W - 10 &&
             rel_y >= row_y - 2 && rel_y < row_y + 15) {
            send_wallpaper_choice(choices[i].id);
            draw_wallpick(win, x, y);
          }
        }
      }
      old_buttons = buttons;
    }
  }

  win_destroy(win);
  wake_desktop();
  exit();
}
