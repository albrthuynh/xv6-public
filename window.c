#include "types.h"
#include "defs.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "window.h"

struct win_event_queue {
  struct win_event events[WIN_EVENTQ_LEN];
  uint r;
  uint w;
};

static struct {
  struct spinlock lock;
  int next_window_id;
  struct window windows[MAX_WINDOWS];
  struct win_event_queue queues[MAX_WINDOWS];
} wm;

static int
slot_for_window_id_nolock(int window_id)
{
  int i;
  for (i = 0; i < MAX_WINDOWS; i++) {
    if (wm.windows[i].used && wm.windows[i].id == window_id)
      return i;
  }
  return -1;
}

void
windowinit(void)
{
  initlock(&wm.lock, "window");
  wm.next_window_id = 1;
}

int
windowcreate(int x, int y, int w, int h)
{
  int i;
  int id;

  if (w <= 0 || h <= 0)
    return -1;

  acquire(&wm.lock);
  for (i = 0; i < MAX_WINDOWS; i++) {
    if (!wm.windows[i].used)
      break;
  }

  if (i == MAX_WINDOWS) {
    release(&wm.lock);
    return -1;
  }

  id = wm.next_window_id++;
  if (wm.next_window_id < 0)
    wm.next_window_id = 1;

  wm.windows[i].used = 1;
  wm.windows[i].id = id;
  wm.windows[i].owner_pid = proc ? proc->pid : -1;
  wm.windows[i].x = x;
  wm.windows[i].y = y;
  wm.windows[i].w = w;
  wm.windows[i].h = h;

  wm.queues[i].r = 0;
  wm.queues[i].w = 0;

  release(&wm.lock);
  return id;
}

int
windowdestroy(int window_id)
{
  int i;

  acquire(&wm.lock);
  i = slot_for_window_id_nolock(window_id);
  if (i < 0) {
    release(&wm.lock);
    return -1;
  }

  if (proc && wm.windows[i].owner_pid != proc->pid) {
    release(&wm.lock);
    return -1;
  }

  wm.windows[i].used = 0;
  wm.queues[i].r = 0;
  wm.queues[i].w = 0;
  release(&wm.lock);
  return 0;
}

int
windowpostevent(int window_id, struct win_event *ev)
{
  int i;
  uint next;

  if (ev == 0)
    return -1;

  acquire(&wm.lock);
  i = slot_for_window_id_nolock(window_id);
  if (i < 0) {
    release(&wm.lock);
    return -1;
  }

  next = wm.queues[i].w + 1;
  if (next - wm.queues[i].r > WIN_EVENTQ_LEN)
    wm.queues[i].r++;

  wm.queues[i].events[wm.queues[i].w % WIN_EVENTQ_LEN] = *ev;
  wm.queues[i].w = next;
  wakeup(&wm.queues[i]);
  release(&wm.lock);
  return 0;
}

int
windowpollevent(int window_id, struct win_event *out)
{
  int i;

  if (out == 0)
    return -1;

  acquire(&wm.lock);
  i = slot_for_window_id_nolock(window_id);
  if (i < 0) {
    release(&wm.lock);
    return -1;
  }

  if (proc && wm.windows[i].owner_pid != proc->pid) {
    release(&wm.lock);
    return -1;
  }

  if (wm.queues[i].r == wm.queues[i].w) {
    release(&wm.lock);
    return 0;
  }

  *out = wm.queues[i].events[wm.queues[i].r % WIN_EVENTQ_LEN];
  wm.queues[i].r++;
  release(&wm.lock);
  return 1;
}

void
windowtick(void)
{
  int i;
  struct win_event ev;

  ev.type = WIN_EV_TICK;
  ev.a = 0;
  ev.b = 0;

  acquire(&wm.lock);
  for (i = 0; i < MAX_WINDOWS; i++) {
    uint next;
    if (!wm.windows[i].used)
      continue;

    next = wm.queues[i].w + 1;
    if (next - wm.queues[i].r > WIN_EVENTQ_LEN)
      wm.queues[i].r++;

    ev.window_id = wm.windows[i].id;
    wm.queues[i].events[wm.queues[i].w % WIN_EVENTQ_LEN] = ev;
    wm.queues[i].w = next;
    wakeup(&wm.queues[i]);
  }
  release(&wm.lock);
}
