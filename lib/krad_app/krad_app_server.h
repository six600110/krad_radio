#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>

#ifdef KR_LINUX
#include <ifaddrs.h>
#endif

#include "krad_radio_version.h"
#include "krad_system.h"
#include "krad_ring.h"
#include "krad_io2.h"
#include "krad_radio_ipc.h"

#include "krad_radio_client.h"

#ifndef KRAD_APP_SERVER_H
#define KRAD_APP_SERVER_H

#define KRAD_APP_CLIENT_DOCTYPE "krad_app_client"
#define KRAD_APP_SERVER_DOCTYPE "krad_app_server"
#define KRAD_APP_DOCTYPE_VERSION KRAD_VERSION
#define KRAD_APP_DOCTYPE_READ_VERSION KRAD_VERSION

#define EBML_ID_KRAD_APP_CMD 0x4444

#define MAX_REMOTES 16
#define KRAD_APP_SERVER_MAX_CLIENTS 16
#define MAX_BROADCASTS 128
#define MAX_BROADCASTERS 16

enum krad_app_shutdown {
  KRAD_APP_STARTING = -1,
  KRAD_APP_RUNNING,
  KRAD_APP_DO_SHUTDOWN,
  KRAD_APP_SHUTINGDOWN,
};

typedef struct krad_app_server_St krad_app_server_t;
typedef struct krad_app_server_client_St krad_app_server_client_t;
typedef struct krad_app_broadcaster_St krad_app_broadcaster_t;
typedef struct krad_broadcast_msg_St krad_broadcast_msg_t;

struct krad_broadcast_msg_St {
  unsigned char *buffer;
  uint32_t size;
  krad_app_server_client_t *skip_client;
};

struct krad_app_broadcaster_St {
  krad_app_server_t *app;
  krad_ringbuffer_t *msg_ring;
  int sockets[2];
};

struct krad_app_server_St {

  struct sockaddr_un saddr;
  struct utsname unixname;
  int on_linux;
  int sd;
  int tcp_sd[MAX_REMOTES];
  uint16_t tcp_port[MAX_REMOTES];
  char *tcp_interface[MAX_REMOTES];
  int shutdown;

  int socket_count;
  
  krad_control_t krad_control;
  
  krad_app_server_client_t *clients;
  krad_app_server_client_t *current_client;

  void *(*client_create)(void *);
  void (*client_destroy)(void *);
  int (*client_handler)(kr_io2_t *in, kr_io2_t *out, void *);
  void *pointer;

  pthread_t server_thread;

  struct pollfd sockets[KRAD_APP_SERVER_MAX_CLIENTS + MAX_BROADCASTERS + MAX_REMOTES + 2];
  krad_app_server_client_t *sockets_clients[KRAD_APP_SERVER_MAX_CLIENTS + MAX_BROADCASTERS + MAX_REMOTES + 2];  

  krad_app_broadcaster_t *sockets_broadcasters[MAX_BROADCASTERS + MAX_REMOTES + 2];
  krad_app_broadcaster_t *broadcasters[MAX_BROADCASTERS];
  int broadcasters_count;
  uint32_t broadcasts[MAX_BROADCASTS];
  int broadcasts_count;

  krad_app_broadcaster_t *app_broadcaster;
};

struct krad_app_server_client_St {
  int sd;
  void *ptr;
  int broadcasts;
  kr_io2_t *in;
  kr_io2_t *out;
};

void krad_app_server_add_client_to_broadcast ( krad_app_server_t *krad_app_server, uint32_t broadcast_ebml_id );
int krad_broadcast_msg_destroy (krad_broadcast_msg_t **broadcast_msg);
krad_broadcast_msg_t *krad_broadcast_msg_create (krad_app_broadcaster_t *broadcaster, unsigned char *buffer, uint32_t size);
int krad_app_server_broadcaster_broadcast ( krad_app_broadcaster_t *broadcaster, krad_broadcast_msg_t **broadcast_msg );
void krad_app_server_broadcaster_register_broadcast ( krad_app_broadcaster_t *broadcaster, uint32_t broadcast_ebml_id );
krad_app_broadcaster_t *krad_app_server_broadcaster_register ( krad_app_server_t *app_server );
int krad_app_server_broadcaster_unregister ( krad_app_broadcaster_t **broadcaster );
int krad_app_server_current_client_is_subscriber (krad_app_server_t *app);

int krad_app_server_recvfd (krad_app_server_client_t *client);

int krad_app_server_disable_remote (krad_app_server_t *app_server,
                                    char *interface,
                                    int port);
int krad_app_server_enable_remote (krad_app_server_t *app_server,
                                   char *interface,
                                   uint16_t port);
void krad_app_server_disable (krad_app_server_t *krad_app_server);
void krad_app_server_destroy (krad_app_server_t *app_server);
void krad_app_server_run (krad_app_server_t *krad_app_server);
krad_app_server_t *
krad_app_server_create (char *appname, char *sysname,
                        void *client_create (void *),
                        void client_destroy (void *),
                        int client_handler (kr_io2_t *in,
                                            kr_io2_t *out,
                                            void *),
                        void *pointer);

#endif

