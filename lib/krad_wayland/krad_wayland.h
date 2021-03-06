#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <signal.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "krad_system.h"

#ifndef KRAD_WAYLAND_H
#define KRAD_WAYLAND_H

#define KR_WL_MAX_WINDOWS 4
#define KR_WL_BUFFER_COUNT 1

typedef struct kr_wayland_st kr_wayland;
typedef struct kr_wayland_window_st kr_wayland_window;
typedef struct kr_wayland_window_params_st kr_wayland_window_params;
typedef struct kr_wayland_event_st kr_wayland_event;
typedef struct kr_wayland_pointer_event_st kr_wayland_pointer_event;
typedef struct kr_wayland_key_event_st kr_wayland_key_event;
typedef struct kr_wayland_frame_event_st kr_wayland_frame_event;

enum kr_wayland_event_type {
  KR_WL_FRAME,
  KR_WL_POINTER,
  KR_WL_KEY
};

struct kr_wayland_pointer_event_st {
  int x;
  int y;
  int click;
  int pointer_in;
  int pointer_out;
};

struct kr_wayland_key_event_st {
  int key;
  int down;
};

struct kr_wayland_frame_event_st {
  uint8_t *buffer;
};

struct kr_wayland_event_st {
  int type;
  kr_wayland_pointer_event pointer_event;
  kr_wayland_key_event key_event;
  kr_wayland_frame_event frame_event;
};

struct kr_wayland_window_params_st {
  uint32_t width;
  uint32_t height;
  int (*callback)(void *, kr_wayland_event *);
  void *user;
};

kr_wayland_window *kr_wayland_window_create(kr_wayland *wayland,
 kr_wayland_window_params *params);
int kr_wayland_window_destroy(kr_wayland_window **win);
int kr_wayland_get_fd(kr_wayland *wayland);
int kr_wayland_process(kr_wayland *wayland);
int kr_wayland_destroy(kr_wayland **wl);
kr_wayland *kr_wayland_create();
kr_wayland *kr_wayland_create_for_server(char *server);

#endif
