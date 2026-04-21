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
  int focused_window_id;
  int zorder[MAX_WINDOWS];
  int zcount;
  struct window windows[MAX_WINDOWS];
  struct win_event_queue queues[MAX_WINDOWS];
  int compositor_window_id;
} wm;

static int
slot_for_window_id_nolock(int window_id)
{
  int i;
  for (i = 0; i < MAX_WINDOWS; i++) {
    if (wm.windows[i].used && wm.windows[i].id == window_id) {
      return i;
    }
  }
  return -1;
}

static void
reset_queue_nolock(int slot)
{
  wm.queues[slot].r = 0;
  wm.queues[slot].w = 0;
}

static int
zpos_for_slot_nolock(int slot)
{
  int i;

  for (i = 0; i < wm.zcount; i++) {
    if (wm.zorder[i] == slot) {
      return i;
    }
  }
  return -1;
}

static void
zremove_slot_nolock(int slot)
{
  int pos;
  int i;

  pos = zpos_for_slot_nolock(slot);
  if (pos < 0) {
    return;
  }

  for (i = pos; i + 1 < wm.zcount; i++) {
    wm.zorder[i] = wm.zorder[i + 1];
  }
  if (wm.zcount > 0) {
    wm.zcount--;
  }
}

static void
zpush_top_slot_nolock(int slot)
{
  zremove_slot_nolock(slot);
  if (wm.zcount >= MAX_WINDOWS) {
    return;
  }
  wm.zorder[wm.zcount] = slot;
  wm.zcount++;
}

static void
focus_top_nolock(void)
{
  if (wm.zcount == 0) {
    wm.focused_window_id = -1;
    return;
  }
  wm.focused_window_id = wm.windows[wm.zorder[wm.zcount - 1]].id;
}

void
windowinit(void)
{
  int i;

  initlock(&wm.lock, "window");
  wm.next_window_id = 1;
  wm.focused_window_id = -1;
  wm.compositor_window_id = -1;
  wm.zcount = 0;
  for (i = 0; i < MAX_WINDOWS; i++) {
    wm.zorder[i] = -1;
  }
}

int
windowcreate(int x, int y, int w, int h)
{
  int i;
  int id;

  if (w <= 0 || h <= 0) {
    return -1;
  }

  acquire(&wm.lock);
  for (i = 0; i < MAX_WINDOWS; i++) {
    if (!wm.windows[i].used) {
      break;
    }
  }

  if (i == MAX_WINDOWS) {
    release(&wm.lock);
    return -1;
  }

  id = wm.next_window_id++;
  if (wm.next_window_id < 0) {
    wm.next_window_id = 1;
  }

  wm.windows[i].used = 1;
  wm.windows[i].id = id;
  wm.windows[i].owner_pid = proc ? proc->pid : -1;
  wm.windows[i].x = x;
  wm.windows[i].y = y;
  wm.windows[i].w = w;
  wm.windows[i].h = h;

  reset_queue_nolock(i);
  zpush_top_slot_nolock(i);
  wm.focused_window_id = id;

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

  zremove_slot_nolock(i);
  if (wm.focused_window_id == window_id) {
    focus_top_nolock();
  }

  wm.windows[i].used = 0;
  reset_queue_nolock(i);
  release(&wm.lock);
  return 0;
}

int
windowfocus(int window_id)
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

  zpush_top_slot_nolock(i);
  wm.focused_window_id = window_id;
  release(&wm.lock);
  return 0;
}

int
windowgetfocus(void)
{
  int focused;

  acquire(&wm.lock);
  focused = wm.focused_window_id;
  release(&wm.lock);
  return focused;
}

int
windowsnapshot(struct window *out, int max)
{
  int i;
  int n;

  if (max < 0) {
    return -1;
  }
  if (max == 0) {
    return 0;
  }
  if (out == 0) {
    return -1;
  }

  acquire(&wm.lock);
  n = wm.zcount;
  if (n > max) {
    n = max;
  }

  for (i = 0; i < n; i++) {
    out[i] = wm.windows[wm.zorder[i]];
  }
  release(&wm.lock);
  return n;
}

void
windowcleanupowner(int owner_pid)
{
  int i;
  int changed;

  if (owner_pid < 0) {
    return;
  }

  acquire(&wm.lock);
  changed = 0;
  for (i = 0; i < MAX_WINDOWS; i++) {
    if (!wm.windows[i].used) {
      continue;
    }
    if (wm.windows[i].owner_pid != owner_pid) {
      continue;
    }

    zremove_slot_nolock(i);
    if (wm.focused_window_id == wm.windows[i].id) {
      wm.focused_window_id = -1;
    }
    wm.windows[i].used = 0;
    reset_queue_nolock(i);
    changed = 1;
  }

  if (changed) {
    if (wm.focused_window_id < 0) {
      focus_top_nolock();
    }
  }
  release(&wm.lock);
}

int
windowpostevent(int window_id, struct win_event *ev)
{
  int i;
  uint next;

  if (ev == 0) {
    return -1;
  }

  acquire(&wm.lock);
  i = slot_for_window_id_nolock(window_id);
  if (i < 0) {
    release(&wm.lock);
    return -1;
  }

  next = wm.queues[i].w + 1;
  if (next - wm.queues[i].r > WIN_EVENTQ_LEN) {
    wm.queues[i].r++;
  }

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

  if (out == 0) {
    return -1;
  }

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
    if (!wm.windows[i].used) {
      continue;
    }

    next = wm.queues[i].w + 1;
    if (next - wm.queues[i].r > WIN_EVENTQ_LEN) {
      wm.queues[i].r++;
    }

    ev.window_id = wm.windows[i].id;
    wm.queues[i].events[wm.queues[i].w % WIN_EVENTQ_LEN] = ev;
    wm.queues[i].w = next;
    wakeup(&wm.queues[i]);
  }
  release(&wm.lock);
}

void windowsetcompositor(int id) {
	acquire(&wm.lock);
	wm.compositor_window_id = id;
	release(&wm.lock);
}

int windowgetcompositor(void) {
	int id;
	acquire(&wm.lock);
	id = wm.compositor_window_id;
	release(&wm.lock);
	return id;
}
