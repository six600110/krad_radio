#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>
#include <sys/stat.h>

#include "krad_ipc_server.h"
#include "krad_radio_ipc.h"
#include "krad_websocket.h"
#include "krad_http.h"

#include "krad_link.h"
#include "krad_mixer.h"
#include "krad_tags.h"

typedef struct krad_radio_St krad_radio_t;

struct krad_radio_St {

	char *sysname;
	
	krad_ipc_server_t *krad_ipc;
	krad_websocket_t *krad_websocket;
	krad_http_t *krad_http;
	krad_link_t *krad_link;
	krad_mixer_t *krad_mixer;
	krad_tags_t *krad_tags;

};



void krad_radio_destroy(krad_radio_t *krad_radio);
krad_radio_t *krad_radio_create(char *sysname);
void krad_radio_daemonize ();
void krad_radio_run (krad_radio_t *krad_radio);
void krad_radio (char *sysname);

int krad_radio_handler ( void *output, int *output_len, void *ptr );