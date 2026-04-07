#include "types.h"
#include "defs.h"
#include "param.h"
#include "proc.h"
#include "window.h"

addr_t
sys_win_create(void)
{
  int x;
  int y;
  int w;
  int h;

  if (argint(0, &x) < 0 || argint(1, &y) < 0 ||
      argint(2, &w) < 0 || argint(3, &h) < 0)
    return -1;

  return windowcreate(x, y, w, h);
}

addr_t
sys_win_destroy(void)
{
  int window_id;

  if (argint(0, &window_id) < 0)
    return -1;
  return windowdestroy(window_id);
}

addr_t
sys_win_poll(void)
{
  int window_id;
  addr_t user_event_ptr;
  struct win_event ev;
  int r;

  if (argint(0, &window_id) < 0 || argaddr(1, &user_event_ptr) < 0)
    return -1;

  r = windowpollevent(window_id, &ev);
  if (r <= 0)
    return r;

  if (copyout(proc->pgdir, user_event_ptr, &ev, sizeof(ev)) < 0)
    return -1;

  return 1;
}
