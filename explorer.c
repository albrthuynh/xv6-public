#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"
#include "window.h"

#define EXPLORER_PATH_MAX 256
#define EXPLORER_LINE_MAX 128
#define EXPLORER_PREVIEW_BYTES 256

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
trim_newline(char *s)
{
  int n;

  n = strlen(s);
  while(n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')){
    s[n - 1] = 0;
    n--;
  }
}

static int
starts_with(const char *s, const char *prefix)
{
  int i;

  for(i = 0; prefix[i]; i++){
    if(s[i] != prefix[i])
      return 0;
  }
  return 1;
}

static char*
skip_spaces(char *s)
{
  while(*s == ' ')
    s++;
  return s;
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

static char
entry_kind(short type)
{
  switch(type){
  case T_DIR:
    return 'd';
  case T_FILE:
    return 'f';
  case T_DEV:
    return 'c';
  default:
    return '?';
  }
}

static int
list_directory(const char *path)
{
  int fd;
  int rc;
  int shown;
  struct stat st;
  struct dirent de;
  struct stat est;
  char entry_name[DIRSIZ + 1];
  char fullpath[EXPLORER_PATH_MAX];

  fd = open((char*)path, O_RDONLY);
  if(fd < 0){
    printf(2, "explorer: cannot open %s\n", path);
    return -1;
  }
  if(fstat(fd, &st) < 0){
    printf(2, "explorer: cannot stat %s\n", path);
    close(fd);
    return -1;
  }
  if(st.type != T_DIR){
    printf(2, "explorer: %s is not a directory\n", path);
    close(fd);
    return -1;
  }

  printf(1, "\nexplorer: listing %s\n", path);
  shown = 0;
  while((rc = readdir(fd, &de)) > 0){
    if(de.inum == 0)
      continue;

    dirent_name_to_cstr(entry_name, de.name);
    if(strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0)
      continue;

    if(path_join(fullpath, sizeof(fullpath), path, entry_name) < 0){
      printf(2, "explorer: path too long: %s/%s\n", path, entry_name);
      continue;
    }
    if(stat(fullpath, &est) < 0){
      printf(2, "explorer: cannot stat %s\n", fullpath);
      continue;
    }

    printf(1, "  [%c] %-14s ino=%d size=%d\n",
           entry_kind(est.type), entry_name, est.ino, est.size);
    shown++;
  }

  if(rc < 0)
    printf(2, "explorer: read error while listing %s\n", path);
  if(shown == 0)
    printf(1, "  <empty>\n");

  close(fd);
  return 0;
}

static int
change_directory(char *cwd, int cwdsz, const char *arg)
{
  char target[EXPLORER_PATH_MAX];
  struct stat st;

  if(arg[0] == '/'){
    if(copy_bounded(target, sizeof(target), arg) < 0)
      return -1;
  } else {
    if(path_join(target, sizeof(target), cwd, arg) < 0)
      return -1;
  }

  if(stat(target, &st) < 0 || st.type != T_DIR)
    return -1;

  return copy_bounded(cwd, cwdsz, target);
}

static void
preview_file(const char *cwd, const char *arg)
{
  char target[EXPLORER_PATH_MAX];
  char buf[64];
  int fd;
  int n;
  int total;
  int to_read;
  int have_last;
  char last_ch;

  if(arg[0] == '/'){
    if(copy_bounded(target, sizeof(target), arg) < 0){
      printf(2, "explorer: target path too long\n");
      return;
    }
  } else {
    if(path_join(target, sizeof(target), cwd, arg) < 0){
      printf(2, "explorer: target path too long\n");
      return;
    }
  }

  fd = open(target, O_RDONLY);
  if(fd < 0){
    printf(2, "explorer: cannot open %s\n", target);
    return;
  }

  printf(1, "\nexplorer: preview %s\n", target);
  total = 0;
  have_last = 0;
  while(total < EXPLORER_PREVIEW_BYTES){
    to_read = sizeof(buf);
    if(to_read > EXPLORER_PREVIEW_BYTES - total)
      to_read = EXPLORER_PREVIEW_BYTES - total;

    n = read(fd, buf, to_read);
    if(n < 0){
      printf(2, "explorer: read error on %s\n", target);
      break;
    }
    if(n == 0)
      break;
    write(1, buf, n);
    last_ch = buf[n - 1];
    have_last = 1;
    total += n;
  }
  if(total == 0)
    printf(1, "  <empty>\n");
  else if(have_last && last_ch != '\n')
    printf(1, "\n");
  if(total >= EXPLORER_PREVIEW_BYTES)
    printf(1, "... (truncated to %d bytes)\n", EXPLORER_PREVIEW_BYTES);
  close(fd);
}

static void
print_help(void)
{
  printf(1, "\nexplorer commands:\n");
  printf(1, "  ls | refresh    list current directory\n");
  printf(1, "  cd <path>       change directory\n");
  printf(1, "  open <file>     preview first 256 bytes\n");
  printf(1, "  pwd             print current directory\n");
  printf(1, "  help            show commands\n");
  printf(1, "  quit | exit     close explorer window\n");
}

int
main(int argc, char *argv[])
{
  int window_id;
  int r;
  char cwd[EXPLORER_PATH_MAX];
  char line[EXPLORER_LINE_MAX];
  char *arg;
  struct win_event ev;

  if(argc > 1){
    if(copy_bounded(cwd, sizeof(cwd), argv[1]) < 0){
      printf(2, "explorer: start path too long\n");
      exit();
    }
  } else {
    if(copy_bounded(cwd, sizeof(cwd), ".") < 0){
      printf(2, "explorer: internal path setup failed\n");
      exit();
    }
  }

  window_id = win_create(40, 30, 380, 260);
  if(window_id < 0){
    printf(2, "explorer: win_create failed\n");
    exit();
  }

  printf(1, "explorer: window id=%d\n", window_id);
  print_help();
  list_directory(cwd);

  for(;;){
    while((r = win_poll(window_id, &ev)) > 0){
      if(ev.type == WIN_EV_CLOSE){
        printf(1, "explorer: close event\n");
        goto done;
      }
    }
    if(r < 0){
      printf(2, "explorer: win_poll failed\n");
      break;
    }

    printf(1, "\nexplorer:%s> ", cwd);
    if(gets(line, sizeof(line)) == 0)
      break;
    trim_newline(line);
    if(line[0] == 0)
      continue;

    if(strcmp(line, "ls") == 0 || strcmp(line, "refresh") == 0){
      list_directory(cwd);
      continue;
    }
    if(strcmp(line, "pwd") == 0){
      printf(1, "%s\n", cwd);
      continue;
    }
    if(strcmp(line, "help") == 0){
      print_help();
      continue;
    }
    if(strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0)
      break;

    if(starts_with(line, "cd ")){
      arg = skip_spaces(line + 3);
      if(arg[0] == 0){
        printf(2, "explorer: missing path for cd\n");
        continue;
      }
      if(change_directory(cwd, sizeof(cwd), arg) < 0){
        printf(2, "explorer: cannot cd to %s\n", arg);
        continue;
      }
      list_directory(cwd);
      continue;
    }

    if(starts_with(line, "open ")){
      arg = skip_spaces(line + 5);
      if(arg[0] == 0){
        printf(2, "explorer: missing file for open\n");
        continue;
      }
      preview_file(cwd, arg);
      continue;
    }

    printf(2, "explorer: unknown command '%s'\n", line);
  }

done:
  if(win_destroy(window_id) < 0)
    printf(2, "explorer: win_destroy failed\n");
  exit();
}
