#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "window.h"
#include "gui.h"

#define TERMINAL_X 56
#define TERMINAL_Y 28
#define TERMINAL_W 252
#define TERMINAL_H 148

#define TERM_HISTORY_LINES 72
#define TERM_COLS 58
#define TERM_ROWS 16
#define TERM_INPUT_MAX 128
#define TERM_PATH_MAX 256
#define TERM_MAX_ARGS 10

struct terminal_state {
  int window_id;
  char history[TERM_HISTORY_LINES][TERM_COLS + 1];
  int history_count;
  char input[TERM_INPUT_MAX];
  int input_len;
  char cwd[TERM_PATH_MAX];
  int old_buttons;
  int tick_divider;
};

static struct terminal_state terminal_state;

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

static void
history_clear(struct terminal_state *st)
{
  int i;

  for(i = 0; i < TERM_HISTORY_LINES; i++)
    st->history[i][0] = 0;
  st->history_count = 1;
}

static void
history_push_empty(struct terminal_state *st)
{
  int i;

  if(st->history_count < TERM_HISTORY_LINES){
    st->history[st->history_count][0] = 0;
    st->history_count++;
    return;
  }

  for(i = 1; i < TERM_HISTORY_LINES; i++)
    memmove(st->history[i - 1], st->history[i], TERM_COLS + 1);
  st->history[TERM_HISTORY_LINES - 1][0] = 0;
}

static void
history_putc(struct terminal_state *st, char c)
{
  int len;

  if(c == '\r')
    return;
  if(c == '\n'){
    history_push_empty(st);
    return;
  }
  if(c == '\t'){
    history_putc(st, ' ');
    history_putc(st, ' ');
    return;
  }

  len = strlen(st->history[st->history_count - 1]);
  if(len >= TERM_COLS){
    history_push_empty(st);
    len = 0;
  }
  st->history[st->history_count - 1][len] = safe_char(c);
  st->history[st->history_count - 1][len + 1] = 0;
}

static void
history_write(struct terminal_state *st, const char *s)
{
  int i;

  for(i = 0; s[i]; i++)
    history_putc(st, s[i]);
}

static void
draw_input_tail(int x, int y, const char *prefix, const char *input)
{
  char line[TERM_COLS + 1];
  int prefix_len;
  int input_len;
  int avail;
  int start;
  int i;
  int pos;

  prefix_len = strlen((char*)prefix);
  input_len = strlen((char*)input);
  avail = TERM_COLS - prefix_len;
  if(avail < 0)
    avail = 0;
  start = 0;
  if(input_len > avail)
    start = input_len - avail;

  pos = 0;
  for(i = 0; prefix[i] && pos < TERM_COLS; i++)
    line[pos++] = safe_char(prefix[i]);
  for(i = start; input[i] && pos < TERM_COLS; i++)
    line[pos++] = safe_char(input[i]);
  if(pos < TERM_COLS)
    line[pos++] = '_';
  line[pos] = 0;
  draw_string(x, y, line, 15);
}

static void
draw_terminal(struct terminal_state *st)
{
  int first;
  int i;
  int y;

  draw_rect(TERMINAL_X, TERMINAL_Y, TERMINAL_W, TERMINAL_H, 8);
  draw_rect(TERMINAL_X, TERMINAL_Y, TERMINAL_W, 12, 0);
  draw_rect(TERMINAL_X + 2, TERMINAL_Y + 2, 8, 8, 4);
  draw_string(TERMINAL_X + 98, TERMINAL_Y + 4, "TERMINAL", 15);

  draw_rect(TERMINAL_X + 4, TERMINAL_Y + 16, TERMINAL_W - 8, 104, 0);
  draw_rect(TERMINAL_X + 4, TERMINAL_Y + 124, TERMINAL_W - 8, 18, 1);

  first = 0;
  if(st->history_count > TERM_ROWS)
    first = st->history_count - TERM_ROWS;

  y = TERMINAL_Y + 20;
  for(i = first; i < st->history_count && i < first + TERM_ROWS; i++){
    draw_rect(TERMINAL_X + 6, y - 1, TERMINAL_W - 12, 7, 0);
    draw_string(TERMINAL_X + 8, y, st->history[i], 15);
    y += 6;
  }

  draw_rect(TERMINAL_X + 6, TERMINAL_Y + 128, TERMINAL_W - 12, 10, 1);
  draw_input_tail(TERMINAL_X + 8, TERMINAL_Y + 130, "TERM> ", st->input);
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

static int
split_words(char *line, char **argv, int maxargs)
{
  int argc;

  argc = 0;
  while(*line){
    while(*line == ' ')
      *line++ = 0;
    if(*line == 0)
      break;
    if(argc + 1 >= maxargs)
      break;
    argv[argc++] = line;
    while(*line && *line != ' ')
      line++;
  }
  argv[argc] = 0;
  return argc;
}

static void
run_exec_capture(struct terminal_state *st, char *line)
{
  int p[2];
  int pid;
  int n;
  char *argv[TERM_MAX_ARGS];
  char buf[65];
  int argc;

  argc = split_words(line, argv, TERM_MAX_ARGS);
  if(argc == 0)
    return;

  if(pipe(p) < 0){
    history_write(st, "pipe failed\n");
    return;
  }

  pid = fork();
  if(pid < 0){
    close(p[0]);
    close(p[1]);
    history_write(st, "fork failed\n");
    return;
  }

  if(pid == 0){
    close(p[0]);
    close(1);
    if(dup(p[1]) < 0)
      exit();
    close(2);
    if(dup(p[1]) < 0)
      exit();
    close(p[1]);
    exec(argv[0], argv);
    printf(2, "exec %s failed\n", argv[0]);
    exit();
  }

  close(p[1]);
  while((n = read(p[0], buf, sizeof(buf) - 1)) > 0){
    buf[n] = 0;
    history_write(st, buf);
  }
  close(p[0]);
  wait();
}

static void
run_builtin(struct terminal_state *st, char *line, int *handled, int *should_exit)
{
  char temp[TERM_PATH_MAX];

  *handled = 1;
  if(strcmp(line, "clear") == 0){
    history_clear(st);
    return;
  }
  if(strcmp(line, "help") == 0){
    history_write(st, "help clear pwd cd exit\n");
    history_write(st, "exec commands without pipes\n");
    return;
  }
  if(strcmp(line, "pwd") == 0){
    history_write(st, st->cwd);
    history_putc(st, '\n');
    return;
  }
  if(strcmp(line, "exit") == 0 || strcmp(line, ":quit") == 0){
    *should_exit = 1;
    return;
  }
  if(line[0] == 'c' && line[1] == 'd' && line[2] == ' '){
    line += 3;
    while(*line == ' ')
      line++;
    if(*line == 0){
      history_write(st, "cd needs path\n");
      return;
    }
    if(chdir(line) < 0){
      history_write(st, "cannot cd ");
      history_write(st, line);
      history_putc(st, '\n');
      return;
    }
    if(line[0] == '/'){
      if(copy_bounded(st->cwd, sizeof(st->cwd), line) < 0)
        copy_bounded(st->cwd, sizeof(st->cwd), "/");
      return;
    }
    if(strcmp(line, "..") == 0){
      path_parent(st->cwd);
      return;
    }
    if(path_join(temp, sizeof(temp), st->cwd, line) < 0){
      history_write(st, "cwd too long\n");
      return;
    }
    if(copy_bounded(st->cwd, sizeof(st->cwd), temp) < 0)
      copy_bounded(st->cwd, sizeof(st->cwd), ".");
    return;
  }

  *handled = 0;
}

static void
execute_input(struct terminal_state *st, int *should_exit)
{
  char line[TERM_INPUT_MAX];
  int handled;

  if(copy_bounded(line, sizeof(line), st->input) < 0){
    history_write(st, "line too long\n");
    st->input_len = 0;
    st->input[0] = 0;
    return;
  }

  history_write(st, "TERM> ");
  history_write(st, st->input);
  history_putc(st, '\n');

  run_builtin(st, line, &handled, should_exit);
  if(!handled && line[0])
    run_exec_capture(st, line);

  st->input_len = 0;
  st->input[0] = 0;
}

int
main(void)
{
  struct terminal_state *st;
  struct win_event ev;
  int r;
  int rel_x;
  int rel_y;
  int buttons;
  int should_exit;
  int c;

  st = &terminal_state;
  memset(st, 0, sizeof(*st));
  should_exit = 0;
  history_clear(st);
  if(copy_bounded(st->cwd, sizeof(st->cwd), ".") < 0)
    exit();

  st->window_id = win_create(TERMINAL_X, TERMINAL_Y, TERMINAL_W, TERMINAL_H);
  if(st->window_id < 0){
    printf(2, "terminal: win_create failed\n");
    exit();
  }
  win_focus(st->window_id);

  history_write(st, "type help for commands\n");
  draw_terminal(st);

  for(;;){
    r = win_poll(st->window_id, &ev);
    if(r < 0)
      break;
    if(r == 0)
      continue;

    if(ev.type == WIN_EV_CLOSE)
      break;

    if(ev.type == WIN_EV_TICK){
      st->tick_divider++;
      if(st->tick_divider >= 8){
        draw_terminal(st);
        st->tick_divider = 0;
      }
      continue;
    }

    if(ev.type == WIN_EV_MOUSE){
      rel_x = (short)(ev.a & 0xFFFF);
      rel_y = (short)((ev.a >> 16) & 0xFFFF);
      buttons = ev.b;

      if((buttons & 1) && !(st->old_buttons & 1)){
        if(rel_x >= 0 && rel_x <= 15 && rel_y >= 0 && rel_y <= 15)
          break;
      }
      st->old_buttons = buttons;
      continue;
    }

    if(ev.type != WIN_EV_KEY)
      continue;

    c = ev.a;
    if(c == 0)
      continue;
    if(c == 'q' && st->input_len == 0)
      break;
    if(c == '\r' || c == '\n'){
      execute_input(st, &should_exit);
      draw_terminal(st);
      if(should_exit)
        break;
      continue;
    }
    if(c == '\b' || c == 0x7f){
      if(st->input_len > 0){
        st->input_len--;
        st->input[st->input_len] = 0;
      }
      draw_terminal(st);
      continue;
    }
    if(c >= 32 && c <= 126 && st->input_len + 1 < TERM_INPUT_MAX){
      st->input[st->input_len++] = c;
      st->input[st->input_len] = 0;
      draw_terminal(st);
    }
  }

  win_destroy(st->window_id);
  wake_desktop();
  exit();
}
