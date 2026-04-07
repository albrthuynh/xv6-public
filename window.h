#pragma once

#include "types.h"

#define MAX_WINDOWS 32
#define WIN_EVENTQ_LEN 32

enum window_event_type {
  WIN_EV_NONE = 0,
  WIN_EV_TICK = 1,
  WIN_EV_CLOSE = 2,
};

struct win_event {
  int type;
  int window_id;
  int a;
  int b;
};

struct window {
  int used;
  int id;
  int owner_pid;
  int x;
  int y;
  int w;
  int h;
};
