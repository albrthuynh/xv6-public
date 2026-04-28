#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"
#include "window.h"
#include "gui.h"

#define EXPLORER_X 8
#define EXPLORER_Y 24
#define EXPLORER_W 304
#define EXPLORER_H 150
#define SCREEN_W 320
#define SCREEN_H 200

#define EXPLORER_PATH_MAX 256
#define EXPLORER_MAX_ENTRIES 32
#define EXPLORER_PREVIEW_BYTES 320
#define EXPLORER_PREVIEW_LINES 14
#define EXPLORER_PREVIEW_COLS 36

#define EXP_LIST_X 6
#define EXP_LIST_Y 42
#define EXP_LIST_W 122
#define EXP_LIST_H 82
#define EXP_ROW_H 8
#define EXP_VISIBLE_ROWS (EXP_LIST_H / EXP_ROW_H)

#define EXP_PREVIEW_X 136
#define EXP_PREVIEW_Y 32
#define EXP_PREVIEW_W 160
#define EXP_PREVIEW_H 110

#define EXP_BUTTON_Y 132

struct explorer_entry {
  char name[DIRSIZ + 1];
  short type;
};

struct explorer_state {
  int window_id;
  int x;
  int y;
  int w;
  int h;
  char cwd[EXPLORER_PATH_MAX];
  struct explorer_entry entries[EXPLORER_MAX_ENTRIES];
  int nentries;
  int selected;
  int scroll;
  char preview[EXPLORER_PREVIEW_LINES][EXPLORER_PREVIEW_COLS + 1];
  int old_buttons;
  int tick_divider;
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
  *x = EXPLORER_X + slot * 12;
  *y = EXPLORER_Y + slot * 8;
  if(*x + EXPLORER_W > SCREEN_W)
    *x = SCREEN_W - EXPLORER_W;
  if(*y + EXPLORER_H > SCREEN_H)
    *y = SCREEN_H - EXPLORER_H;
}

static int
copy_bounded(char *dst, int max, const char *src)
{
  int i;

  if(max <= 0)
    return -1;
  for(i = 0; src[i]; i++){
    if(i + 1 >= max)
      return -1;
    dst[i] = src[i];
  }
  dst[i] = 0;
  return 0;
}

static void
dirent_name_to_cstr(char *dst, const char name[DIRSIZ])
{
  int i;

  for(i = 0; i < DIRSIZ; i++){
    dst[i] = name[i];
    if(name[i] == 0)
      return;
  }
  dst[DIRSIZ] = 0;
}

static int
path_join(char *out, int outsz, const char *base, const char *name)
{
  int i;
  int j;

  if(outsz <= 0)
    return -1;

  if(name[0] == '/')
    return copy_bounded(out, outsz, name);

  i = 0;
  for(; base[i]; i++){
    if(i + 1 >= outsz)
      return -1;
    out[i] = base[i];
  }
  if(i == 0){
    if(i + 1 >= outsz)
      return -1;
    out[i++] = '.';
  }
  if(out[i - 1] != '/'){
    if(i + 1 >= outsz)
      return -1;
    out[i++] = '/';
  }
  for(j = 0; name[j]; j++){
    if(i + 1 >= outsz)
      return -1;
    out[i++] = name[j];
  }
  out[i] = 0;
  return 0;
}

static void
path_parent(char *path)
{
  int n;

  n = strlen(path);
  if(n == 0 || (n == 1 && path[0] == '.'))
    return;
  if(n == 1 && path[0] == '/')
    return;

  while(n > 1 && path[n - 1] == '/'){
    path[n - 1] = 0;
    n--;
  }
  while(n > 0 && path[n - 1] != '/'){
    path[n - 1] = 0;
    n--;
  }
  while(n > 1 && path[n - 1] == '/'){
    path[n - 1] = 0;
    n--;
  }
  if(path[0] == 0){
    path[0] = '.';
    path[1] = 0;
  }
}

static char
safe_char(char c)
{
  if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
     (c >= '0' && c <= '9'))
    return c;
  if(c == ' ' || c == '.' || c == ':' || c == '/' || c == '-' ||
     c == '_' || c == '>' || c == '~' || c == '@' || c == '#')
    return c;
  return '.';
}

static int
is_elf_header(char *buf, int n)
{
  return n >= 4 && buf[0] == 0x7f && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F';
}

static int
is_probably_binary(char *buf, int n)
{
  int i;

  for(i = 0; i < n; i++){
    uchar c = buf[i];
    if(c == 0)
      return 1;
    if(c < 32 && c != '\n' && c != '\r' && c != '\t')
      return 1;
  }
  return 0;
}

static void
copy_clipped_safe(char *dst, int max, const char *src)
{
  int i;

  if(max <= 0)
    return;
  for(i = 0; i + 1 < max && src[i]; i++)
    dst[i] = safe_char(src[i]);
  dst[i] = 0;
}

static void
preview_clear(struct explorer_state *st, const char *title)
{
  int i;
  int j;

  for(i = 0; i < EXPLORER_PREVIEW_LINES; i++){
    for(j = 0; j < EXPLORER_PREVIEW_COLS; j++)
      st->preview[i][j] = 0;
  }
  if(title)
    copy_clipped_safe(st->preview[0], EXPLORER_PREVIEW_COLS + 1, title);
}

static void
preview_set_status(struct explorer_state *st, const char *line1, const char *line2)
{
  preview_clear(st, line1);
  if(line2)
    copy_clipped_safe(st->preview[1], EXPLORER_PREVIEW_COLS + 1, line2);
}

static void
preview_file(struct explorer_state *st, const char *name)
{
  char target[EXPLORER_PATH_MAX];
  char buf[64];
  int fd;
  int n;
  int line;
  int col;
  int total;
  int i;

  if(path_join(target, sizeof(target), st->cwd, name) < 0){
    preview_set_status(st, "PATH TOO LONG", 0);
    return;
  }

  fd = open(target, O_RDONLY);
  if(fd < 0){
    preview_set_status(st, "OPEN FAILED", name);
    return;
  }

  n = read(fd, buf, sizeof(buf));
  if(is_elf_header(buf, n)){
    close(fd);
    preview_clear(st, name);
    copy_clipped_safe(st->preview[1], EXPLORER_PREVIEW_COLS + 1, "BINARY ELF FILE");
    copy_clipped_safe(st->preview[2], EXPLORER_PREVIEW_COLS + 1, "PREVIEW UNAVAILABLE");
    return;
  }
  if(is_probably_binary(buf, n)){
    close(fd);
    preview_clear(st, name);
    copy_clipped_safe(st->preview[1], EXPLORER_PREVIEW_COLS + 1, "BINARY FILE");
    copy_clipped_safe(st->preview[2], EXPLORER_PREVIEW_COLS + 1, "PREVIEW UNAVAILABLE");
    return;
  }

  preview_clear(st, name);
  line = 1;
  col = 0;
  total = 0;
  for(;;){
    if(n <= 0)
      break;
    for(i = 0; i < n && total < EXPLORER_PREVIEW_BYTES; i++, total++){
      char c;

      c = buf[i];
      if(c == '\r')
        continue;
      if(c == '\n'){
        st->preview[line][col] = 0;
        line++;
        col = 0;
        if(line >= EXPLORER_PREVIEW_LINES)
          break;
        continue;
      }
      if(col >= EXPLORER_PREVIEW_COLS){
        st->preview[line][col] = 0;
        line++;
        col = 0;
        if(line >= EXPLORER_PREVIEW_LINES)
          break;
      }
      st->preview[line][col++] = safe_char(c);
      st->preview[line][col] = 0;
    }
    if(line >= EXPLORER_PREVIEW_LINES || total >= EXPLORER_PREVIEW_BYTES)
      break;
    n = read(fd, buf, sizeof(buf));
  }

  close(fd);
  if(line == 1 && col == 0)
    copy_clipped_safe(st->preview[1], EXPLORER_PREVIEW_COLS + 1, "EMPTY FILE");
}

static void
preview_entry(struct explorer_state *st, int idx)
{
  char line[EXPLORER_PREVIEW_COLS + 1];

  if(idx < 0 || idx >= st->nentries){
    preview_set_status(st, "NO ENTRY", 0);
    return;
  }

  if(st->entries[idx].type == T_DIR){
    preview_clear(st, st->entries[idx].name);
    copy_clipped_safe(st->preview[1], EXPLORER_PREVIEW_COLS + 1, "DIRECTORY");
    return;
  }

  line[0] = 'F';
  line[1] = 'I';
  line[2] = 'L';
  line[3] = 'E';
  line[4] = 0;
  preview_set_status(st, line, 0);
  preview_file(st, st->entries[idx].name);
}

static void
load_directory(struct explorer_state *st)
{
  int fd;
  int rc;
  struct stat stbuf;
  struct dirent de;
  char name[DIRSIZ + 1];
  char fullpath[EXPLORER_PATH_MAX];

  st->nentries = 0;
  st->selected = -1;
  st->scroll = 0;

  copy_bounded(st->entries[st->nentries].name, sizeof(st->entries[0].name), "..");
  st->entries[st->nentries].type = T_DIR;
  st->nentries++;

  fd = open(st->cwd, O_RDONLY);
  if(fd < 0){
    preview_set_status(st, "OPEN DIR FAILED", st->cwd);
    return;
  }
  if(fstat(fd, &stbuf) < 0 || stbuf.type != T_DIR){
    close(fd);
    preview_set_status(st, "NOT A DIR", st->cwd);
    return;
  }

  while((rc = readdir(fd, &de)) > 0 && st->nentries < EXPLORER_MAX_ENTRIES){
    if(de.inum == 0)
      continue;
    dirent_name_to_cstr(name, de.name);
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    if(path_join(fullpath, sizeof(fullpath), st->cwd, name) < 0)
      continue;
    if(stat(fullpath, &stbuf) < 0)
      continue;

    dirent_name_to_cstr(st->entries[st->nentries].name, de.name);
    st->entries[st->nentries].type = stbuf.type;
    st->nentries++;
  }

  close(fd);
  preview_set_status(st, "CLICK ENTRY", "DIR OR FILE");
}

static void
draw_label_clipped(int x, int y, const char *src, int limit, int color)
{
  char buf[64];
  int i;

  if(limit > (int)sizeof(buf) - 1)
    limit = sizeof(buf) - 1;
  for(i = 0; i < limit && src[i]; i++)
    buf[i] = safe_char(src[i]);
  buf[i] = 0;
  draw_string(x, y, buf, color);
}

static void
draw_entry_row(struct explorer_state *st, int row, int idx)
{
  int x;
  int y;
  int text_color;
  char label[30];
  int i;
  int pos;

  x = st->x + EXP_LIST_X;
  y = st->y + EXP_LIST_Y + row * EXP_ROW_H;
  if(idx >= st->nentries){
    draw_rect(x, y, EXP_LIST_W, EXP_ROW_H - 1, 15);
    return;
  }

  if(idx == st->selected){
    draw_rect(x, y, EXP_LIST_W, EXP_ROW_H - 1, 1);
    text_color = 15;
  } else {
    draw_rect(x, y, EXP_LIST_W, EXP_ROW_H - 1, 15);
    text_color = 0;
  }

  if(st->entries[idx].type == T_DIR)
    copy_bounded(label, sizeof(label), "DIR  ");
  else
    copy_bounded(label, sizeof(label), "FILE ");

  pos = strlen(label);
  for(i = 0; pos + 1 < (int)sizeof(label) && st->entries[idx].name[i]; i++)
    label[pos++] = safe_char(st->entries[idx].name[i]);
  label[pos] = 0;
  draw_string(x + 2, y + 1, label, text_color);
}

static void
draw_preview_panel(struct explorer_state *st)
{
  int x;
  int y;
  int i;

  x = st->x + EXP_PREVIEW_X;
  y = st->y + EXP_PREVIEW_Y;

  draw_rect(x, y, EXP_PREVIEW_W, EXP_PREVIEW_H, 15);
  draw_rect(x, y, EXP_PREVIEW_W, 1, 0);
  draw_rect(x, y + EXP_PREVIEW_H - 1, EXP_PREVIEW_W, 1, 0);
  draw_rect(x, y, 1, EXP_PREVIEW_H, 0);
  draw_rect(x + EXP_PREVIEW_W - 1, y, 1, EXP_PREVIEW_H, 0);

  draw_rect(x + 1, y + 1, EXP_PREVIEW_W - 2, 10, 8);
  draw_string(x + 4, y + 4, "PREVIEW", 15);

  for(i = 0; i < EXPLORER_PREVIEW_LINES; i++){
    int line_y = y + 15 + i * 6;
    if(line_y + 5 >= y + EXP_PREVIEW_H)
      break;
    draw_string(x + 4, line_y, st->preview[i], 0);
  }
}

static void
draw_explorer(struct explorer_state *st)
{
  int i;
  int title_color;

  title_color = (win_get_focus() == st->window_id) ? 1 : 8;

  draw_rect(st->x, st->y, st->w, st->h, 7);
  draw_rect(st->x, st->y, st->w, 12, title_color);
  draw_rect(st->x + 2, st->y + 2, 8, 8, 4);
  draw_string(st->x + 120, st->y + 4, "EXPLORER", 15);

  draw_rect(st->x + 4, st->y + 16, st->w - 8, 10, 15);
  draw_label_clipped(st->x + 6, st->y + 18, st->cwd, 60, 0);

  draw_rect(st->x + EXP_LIST_X, st->y + EXP_LIST_Y, EXP_LIST_W, EXP_LIST_H, 15);
  draw_string(st->x + EXP_LIST_X, st->y + 34, "FILES", 0);

  for(i = 0; i < EXP_VISIBLE_ROWS; i++)
    draw_entry_row(st, i, st->scroll + i);

  draw_preview_panel(st);

  draw_rect(st->x + EXP_LIST_X, st->y + EXP_BUTTON_Y, 18, 10, 2);
  draw_rect(st->x + EXP_LIST_X + 22, st->y + EXP_BUTTON_Y, 24, 10, 8);
  draw_string(st->x + EXP_LIST_X + 4, st->y + EXP_BUTTON_Y + 3, "UP", 15);
  draw_string(st->x + EXP_LIST_X + 26, st->y + EXP_BUTTON_Y + 3, "DOWN", 15);
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

static void
open_selected(struct explorer_state *st, int idx)
{
  char next[EXPLORER_PATH_MAX];

  if(idx < 0 || idx >= st->nentries)
    return;

  st->selected = idx;
  if(st->entries[idx].type == T_DIR){
    if(strcmp(st->entries[idx].name, "..") == 0){
      if(copy_bounded(next, sizeof(next), st->cwd) < 0)
        return;
      path_parent(next);
    } else if(path_join(next, sizeof(next), st->cwd, st->entries[idx].name) < 0){
      preview_set_status(st, "PATH TOO LONG", 0);
      return;
    }
    if(copy_bounded(st->cwd, sizeof(st->cwd), next) < 0)
      return;
    load_directory(st);
    return;
  }

  preview_entry(st, idx);
}

int
main(int argc, char *argv[])
{
  struct explorer_state st;
  struct win_event ev;
  int r;
  int rel_x;
  int rel_y;
  int buttons;
  int idx;

  memset(&st, 0, sizeof(st));
  st.w = EXPLORER_W;
  st.h = EXPLORER_H;
  choose_window_origin(&st.x, &st.y);
  if(argc > 1){
    if(copy_bounded(st.cwd, sizeof(st.cwd), argv[1]) < 0){
      printf(2, "explorer: start path too long\n");
      exit();
    }
  } else if(copy_bounded(st.cwd, sizeof(st.cwd), ".") < 0){
    exit();
  }

  st.window_id = win_create(st.x, st.y, st.w, st.h);
  if(st.window_id < 0){
    printf(2, "explorer: win_create failed\n");
    exit();
  }
  win_focus(st.window_id);

  load_directory(&st);
  draw_explorer(&st);

  for(;;){
    r = win_poll(st.window_id, &ev);
    if(r < 0)
      break;
    if(r == 0)
      continue;

    if(ev.type == WIN_EV_CLOSE)
      break;

    if(ev.type == WIN_EV_REDRAW){
      draw_explorer(&st);
      continue;
    }

    if(ev.type == WIN_EV_MOVE){
      st.x = ev.a;
      st.y = ev.b;
      continue;
    }

    if(ev.type == WIN_EV_TICK)
      continue;

    if(ev.type == WIN_EV_KEY){
      if(ev.a == 'q')
        break;
      if(ev.a == 'r'){
        load_directory(&st);
        draw_explorer(&st);
      }
      if(ev.a == 'b'){
        idx = 0;
        open_selected(&st, idx);
        draw_explorer(&st);
      }
      continue;
    }

    if(ev.type != WIN_EV_MOUSE)
      continue;

    rel_x = (short)(ev.a & 0xFFFF);
    rel_y = (short)((ev.a >> 16) & 0xFFFF);
    buttons = ev.b;

    if((buttons & 1) && !(st.old_buttons & 1)){
      int did_redraw;

      did_redraw = 0;
      if(rel_x >= 0 && rel_x <= 15 && rel_y >= 0 && rel_y <= 15)
        break;

      if(rel_x >= EXP_LIST_X && rel_x < EXP_LIST_X + EXP_LIST_W &&
         rel_y >= EXP_LIST_Y && rel_y < EXP_LIST_Y + EXP_LIST_H){
        idx = st.scroll + (rel_y - EXP_LIST_Y) / EXP_ROW_H;
        if(idx >= 0 && idx < st.nentries){
          open_selected(&st, idx);
          draw_explorer(&st);
          did_redraw = 1;
        }
      }

      if(rel_y >= EXP_BUTTON_Y && rel_y <= EXP_BUTTON_Y + 10){
        if(rel_x >= EXP_LIST_X && rel_x <= EXP_LIST_X + 18){
          if(st.scroll > 0)
            st.scroll--;
          draw_explorer(&st);
          did_redraw = 1;
        } else if(rel_x >= EXP_LIST_X + 22 && rel_x <= EXP_LIST_X + 46){
          if(st.scroll + EXP_VISIBLE_ROWS < st.nentries)
            st.scroll++;
          draw_explorer(&st);
          did_redraw = 1;
        }
      }
      if(!did_redraw)
        draw_explorer(&st);
	    }
	    st.old_buttons = buttons;
	  }

  win_destroy(st.window_id);
  wake_desktop();
  exit();
}
