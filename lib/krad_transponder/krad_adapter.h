#ifndef KRAD_ADAPTER_H
#define KRAD_ADAPTER_H

#define KR_ADAPTER_PATHS_MAX 32

typedef struct kr_adapter kr_adapter;
typedef struct kr_adapter_setup kr_adapter_setup;
typedef struct kr_adapter_path kr_adapter_path;
typedef struct kr_adapter_path_setup kr_adapter_path_setup;

#include "krad_adapter_common.h"
#include "krad_jack.h"

typedef struct {
  void *user;
} kr_adapter_info_cb_arg;

typedef struct {
  void *user;
} kr_adapter_path_info_cb_arg;

typedef void (kr_adapter_info_cb)(kr_adapter_info_cb_arg *);
typedef void (kr_adapter_path_info_cb)(kr_adapter_path_info_cb_arg *);

struct kr_adapter_path_setup {
  kr_adapter_path_info info;
  void *user;
  kr_adapter_path_info_cb *cb;
};

struct kr_adapter_setup {
  kr_adapter_info info;
  void *user;
  kr_adapter_info_cb *cb;
};


int kr_adapter_unlink(kr_adapter_path *path);
kr_adapter_path *kr_adapter_mkpath(kr_adapter *adapter,
 kr_adapter_path_setup *setup);

int kr_adapter_get_info(kr_adapter *adapter, kr_adapter_info *info);
int kr_adapter_destroy(kr_adapter *adapter);
kr_adapter *kr_adapter_create(kr_adapter_setup *setup);


#endif
