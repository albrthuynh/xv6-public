#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "window.h"

#define TERM_LINE_MAX 128

static char *shell_argv[] = { "sh", 0 };

static void
ensure_stdio(void)
{
  int fd;

  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }
}

static int
spawn_shell(int *shell_in_fd, int *shell_out_fd, int *shell_pid)
{
  int to_shell[2];
  int from_shell[2];
  int pid;

  if(pipe(to_shell) < 0)
    return -1;
  if(pipe(from_shell) < 0){
    close(to_shell[0]);
    close(to_shell[1]);
    return -1;
  }

  pid = fork();
  if(pid < 0){
    close(to_shell[0]);
    close(to_shell[1]);
    close(from_shell[0]);
    close(from_shell[1]);
    return -1;
  }

  if(pid == 0){
    close(to_shell[1]);
    close(from_shell[0]);

    close(0);
    if(dup(to_shell[0]) < 0)
      exit();
    close(to_shell[0]);

    close(1);
    if(dup(from_shell[1]) < 0)
      exit();
    close(2);
    if(dup(from_shell[1]) < 0)
      exit();
    close(from_shell[1]);

    exec("sh", shell_argv);
    printf(2, "terminal: exec sh failed\n");
    exit();
  }

  close(to_shell[0]);
  close(from_shell[1]);
  *shell_in_fd = to_shell[1];
  *shell_out_fd = from_shell[0];
  *shell_pid = pid;
  return 0;
}

static int
spawn_output_pump(int shell_out_fd)
{
  int pid;

  pid = fork();
  if(pid < 0)
    return -1;
  if(pid == 0){
    char buf[128];
    int n;

    while((n = read(shell_out_fd, buf, sizeof(buf))) > 0){
      if(write(1, buf, n) != n)
        break;
    }
    close(shell_out_fd);
    exit();
  }
  return pid;
}

static void
wait_for_children(int shell_pid, int pump_pid)
{
  int got_shell;
  int got_pump;
  int pid;

  got_shell = 0;
  got_pump = (pump_pid < 0);
  while((!got_shell || !got_pump) && (pid = wait()) >= 0){
    if(pid == shell_pid)
      got_shell = 1;
    if(pid == pump_pid)
      got_pump = 1;
  }
}

int
main(void)
{
  int window_id;
  int shell_in_fd;
  int shell_out_fd;
  int shell_pid;
  int pump_pid;
  int r;
  char line[TERM_LINE_MAX];
  struct win_event ev;

  ensure_stdio();

  window_id = win_create(80, 50, 520, 320);
  if(window_id < 0){
    printf(2, "terminal: win_create failed\n");
    exit();
  }

  if(spawn_shell(&shell_in_fd, &shell_out_fd, &shell_pid) < 0){
    printf(2, "terminal: failed to launch shell\n");
    win_destroy(window_id);
    exit();
  }

  pump_pid = spawn_output_pump(shell_out_fd);
  if(pump_pid < 0){
    printf(2, "terminal: failed to launch output pump\n");
    close(shell_in_fd);
    close(shell_out_fd);
    wait_for_children(shell_pid, -1);
    win_destroy(window_id);
    exit();
  }
  close(shell_out_fd);

  printf(1, "terminal: window id=%d shell pid=%d\n", window_id, shell_pid);
  printf(1, "terminal: type :quit to close\n");

  for(;;){
    while((r = win_poll(window_id, &ev)) > 0){
      if(ev.type == WIN_EV_CLOSE)
        goto shutdown;
    }
    if(r < 0){
      printf(2, "terminal: win_poll failed\n");
      break;
    }

    if(gets(line, sizeof(line)) == 0)
      break;
    if(strcmp(line, ":quit\n") == 0 || strcmp(line, ":quit\r\n") == 0 ||
       strcmp(line, ":quit") == 0)
      break;

    if(write(shell_in_fd, line, strlen(line)) < 0){
      printf(2, "terminal: failed to write to shell\n");
      break;
    }
  }

shutdown:
  close(shell_in_fd);
  wait_for_children(shell_pid, pump_pid);

  if(win_destroy(window_id) < 0)
    printf(2, "terminal: win_destroy failed\n");
  exit();
}
