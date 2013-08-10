#include "krad_jack.h"

struct kr_jack_path {
  kr_jack *jack;
  kr_jack_path_info info;
  jack_port_t *ports[KR_JACK_CHANNELS_MAX];
  //float *samples[8];
  //callback / user pointer
};

struct kr_jack {
  jack_client_t *client;
  jack_status_t status;
  jack_options_t options;
  //callback / user pointer
  kr_jack_info info;
  int set_thread_name;
  //char *stay_connected[256];
  //char *stay_connected_to[256];
};

static int process_cb(jack_nframes_t nframes, void *arg);
static void shutdown_cb(void *arg);
static int xrun_cb(void *arg);

static int xrun_cb(void *arg) {

  kr_jack *jack = (kr_jack *)arg;

  jack->info.xruns++;
  //printke ("Krad Jack: %s xrun number %d!", jack->name, jack->xruns);

  return 0;
}

static int process_cb(jack_nframes_t nframes, void *arg) {

  kr_jack *jack = (kr_jack *)arg;

  if (jack->set_thread_name == 0) {
    krad_system_set_thread_name("kr_jack_io");
    jack->set_thread_name = 1;
  }

  //FIXME callback go here

  return 0;
}

static void shutdown_cb(void *arg) {

  kr_jack *jack = (kr_jack *)arg;

  jack->info.active = 0;
  printke("Krad Jack: shutdown callback, oh dear!");
}

/*
void kr_jack_portgroup_samples_callback(int frames, void *user, float **smpls);
void kr_jack_portgroup_samples_callback(int frames, void *userdata,
 float **samples) {

  kr_jack_portgroup *portgroup = (kr_jack_portgroup *)userdata;
  int c;
  float *temp;

  for (c = 0; c < portgroup->channels; c++) {
    if (portgroup->direction == KR_AIN) {
      temp = jack_port_get_buffer (portgroup->ports[c], frames);
      memcpy (portgroup->samples[c], temp, frames * 4);
      samples[c] = portgroup->samples[c];
    } else {
      samples[c] = jack_port_get_buffer (portgroup->ports[c], frames);
    }
  }
}
*/

/*
void kr_jack_check_connection(kr_jack *jack, char *remote_port) {

  int p;
  int flags;
  jack_port_t *port;

  if (strlen(remote_port)) {
    for (p = 0; p < 256; p++) {
      if ((jack->stay_connected[p] != NULL) && (jack->stay_connected_to[p] != NULL)) {
        if ((strncmp(jack->stay_connected_to[p], remote_port, strlen(remote_port))) == 0) {

          port = jack_port_by_name (jack->client, remote_port);
          flags = jack_port_flags (port);

          if (flags == JackPortIsOutput) {
            //printk ("Krad Jack: Replugging %s to %s", remote_port, jack->stay_connected[p]);
            jack_connect (jack->client, remote_port, jack->stay_connected[p]);
          } else {
            //printk ("Krad Jack: Replugging %s to %s", jack->stay_connected[p], remote_port);
            jack_connect (jack->client, jack->stay_connected[p], remote_port);
          }
        }
      }
    }
  }
}

void kr_jack_stay_connection (kr_jack *jack, char *port, char *remote_port) {

  int p;

  if (strlen(port) && strlen(remote_port)) {
    for (p = 0; p < 256; p++) {
      if (jack->stay_connected[p] == NULL) {
        jack->stay_connected[p] = strdup(port);
        jack->stay_connected_to[p] = strdup(remote_port);
        break;
      }
    }
  }
}

void kr_jack_unstay_connection (kr_jack *jack, char *port, char *remote_port) {

  int p;

  for (p = 0; p < 256; p++) {
    if (jack->stay_connected[p] != NULL) {
      if ((jack->stay_connected[p] != NULL) && (jack->stay_connected_to[p] != NULL)) {
        if (((strncmp(jack->stay_connected[p], port, strlen(port))) == 0) &&
           ((strlen(remote_port) == 0) || (strncmp(jack->stay_connected_to[p], remote_port, strlen(remote_port)) == 0))) {
          free (jack->stay_connected[p]);
          free (jack->stay_connected_to[p]);
          break;
        }
      }
    }
  }
}

void kr_jack_port_registration_callback (jack_port_id_t portid, int regged, void *arg) {

  kr_jack *jack = (kr_jack *)arg;

  jack_port_t *port;

  port = jack_port_by_id (jack->client, portid);

  if (regged == 1) {
    //printk ("Krad Jack: %s registered", jack_port_name (port));
    if (jack_port_is_mine(jack->client, port) == 0) {
      kr_jack_check_connection (jack, (char *)jack_port_name(port));
    }
  } else {
    //printk ("Krad Jack: %s unregistered", jack_port_name (port));
  }
}

void kr_jack_port_connection_callback (jack_port_id_t a, jack_port_id_t b, int connect, void *arg) {

  kr_jack *jack = (kr_jack *)arg;

  jack_port_t *ports[2];

  ports[0] = jack_port_by_id (jack->client, a);
  ports[1] = jack_port_by_id (jack->client, b);

  if (connect == 1) {
    //printk ("Krad Jack: %s connected to %s ", jack_port_name (ports[0]), jack_port_name (ports[1]));
  } else {
    //printk ("Krad Jack: %s disconnected from %s ", jack_port_name (ports[0]), jack_port_name (ports[1]));
  }

}
*/
/*
void kr_jack_portgroup_plug(kr_jack_portgroup *portgroup, char *remote_name) {

  const char **ports;
  int flags;
  int c;

  flags = 0;

  if (strlen(remote_name) == 0) {
    return;
  }

  ports = jack_get_ports(portgroup->kr_jack->client, remote_name, JACK_DEFAULT_AUDIO_TYPE, flags);

  if (ports) {
    for (c = 0; c < portgroup->channels; c++) {
      if (ports[c]) {
        //kr_jack_stay_connection (portgroup->kr_jack, (char *)jack_port_name(portgroup->ports[c]), (char *)ports[c]);
        if (portgroup->direction == KR_AIN) {
          //printk ("Krad Jack: Plugging %s to %s", ports[c], jack_port_name(portgroup->ports[c]));
          jack_connect (portgroup->kr_jack->client, ports[c], jack_port_name(portgroup->ports[c]));
        } else {
          //printk ("Krad Jack: Plugging %s to %s", jack_port_name(portgroup->ports[c]), ports[c]);
          jack_connect (portgroup->kr_jack->client, jack_port_name(portgroup->ports[c]), ports[c]);
        }
      } else {
        return;
      }
    }
  }
}

void kr_jack_portgroup_unplug(kr_jack_portgroup *portgroup, char *remote_name) {

  int c;

  for (c = 0; c < portgroup->channels; c++) {
    //kr_jack_unstay_connection (portgroup->kr_jack, (char *)jack_port_name(portgroup->ports[c]), remote_name);
    jack_port_disconnect (portgroup->kr_jack->client, portgroup->ports[c]);
  }
}
*/

int kr_jack_path_unlink(kr_jack_path *path) {

  int c;
  int ret;

  if (path == NULL) return -1;

  for (c = 0; c < path->info.channels; c++) {
    ret = jack_port_unregister(path->jack->client, path->ports[c]);
    if (ret != 0) {
      //FIXME deal with it?
    }
    path->ports[c] = NULL;
  }

  free(path);
  return 0;
}

static int path_setup_valid(kr_jack_path_setup *setup) {
  if ((setup->channels < 1) || (setup->channels > KR_JACK_CHANNELS_MAX)) {
    return -1;
  }
  if ((setup->direction != KR_JACK_INPUT)
   && (setup->direction != KR_JACK_OUTPUT)) {
    return -2;
  }

  if (memchr(setup->name + 1, '\0', sizeof(setup->name) - 1) == NULL) {
    return -3;
  }
  if (strlen(setup->name) == 0) return -4;

  return 0;
}

kr_jack_path *kr_jack_mkpath(kr_jack *jack, kr_jack_path_setup *setup) {

  int c;
  kr_jack_path *path;
  int port_flags;
  char port_name[192];

  if ((jack == NULL) || (setup == NULL)) return NULL;

  if (!path_setup_valid(setup)) return NULL;

  path = calloc (1, sizeof(kr_jack_path));

  path->jack = jack;
  path->info.channels = setup->channels;
  path->info.direction = setup->direction;
  strncpy(path->info.name, setup->name, sizeof(setup->name));

  if (path->info.direction == KR_JACK_INPUT) {
    port_flags = JackPortIsInput;
  } else {
    port_flags = JackPortIsOutput;
  }

  for (c = 0; c < path->info.channels; c++) {
    if (path->info.channels == 1) {
      strncpy(port_name, path->info.name, sizeof(path->info.name));
    } else {
      snprintf(port_name, sizeof(port_name), "%s_%d", path->info.name, c + 1);
      //strcat(port_name, kr_mixer_channeltostr(c));
    }

    path->ports[c] = jack_port_register(path->jack->client, port_name,
     JACK_DEFAULT_AUDIO_TYPE, port_flags, 0);

    if (path->ports[c] == NULL) {
      printke("Krad Jack: Could not reg port, prolly a dupe reg: %s",
       port_name);
      //FIXME fail better
      //free(portgroup);
      //return NULL;
    }
  }

  return path;
}


int kr_jack_destroy(kr_jack *jack) {

  //int p;

  if (jack == NULL) return -1;
  jack_client_close(jack->client);
  /*
  for (p = 0; p < 256; p++) {
    if (jack->stay_connected[p] != NULL) {
      free(jack->stay_connected[p]);
    }
    if (jack->stay_connected_to[p] != NULL) {
      free(jack->stay_connected_to[p]);
    }
  }
  */

  free(jack);
  return 0;
}

int kr_jack_detect() {
  return kr_jack_detect_server_name(NULL);
}

int kr_jack_detect_server_name(char *name) {

  jack_client_t *client;
  jack_options_t options;
  jack_status_t status;
  char client_name[128];

  sprintf(client_name, "kr_jack_detect_%d", rand());

  if (name != NULL) {
    options = JackNoStartServer | JackServerName;
  } else {
    options = JackNoStartServer;
  }

  client = jack_client_open(client_name, options, &status, name);

  if (client == NULL) {
    return 0;
  } else {
    jack_client_close(client);
    return 1;
  }
}

kr_jack *kr_jack_create(kr_jack_setup *setup) {

  kr_jack *jack;
  char *name;

  if (setup == NULL) return NULL;

  jack = calloc(1, sizeof(kr_jack));

  krad_system_set_thread_name("kr_jack");

  strncpy(jack->info.client_name, setup->client_name,
   sizeof(jack->info.client_name));

  if ((setup->server_name[0] != '\0')
    && (memchr(setup->server_name + 1, '\0', sizeof(setup->server_name) - 1)
    != NULL)) {
    strncpy(jack->info.server_name, setup->server_name,
     sizeof(jack->info.server_name));
    jack->options = JackNoStartServer | JackServerName;
  } else {
    jack->info.server_name[0] = '\0';
    jack->options = JackNoStartServer;
  }

  jack->client = jack_client_open(jack->info.client_name, jack->options,
   &jack->status, jack->info.server_name);
  if (jack->client == NULL) {
    //FIXME fail err msg
    printke("Krad Jack: jack_client_open() failed, status = 0x%2.0x",
     jack->status);
    if (jack->status & JackServerFailed) {
      printke("Krad Jack: Unable to connect to JACK server");
    }
    return jack;
  }

  if (jack->status & JackNameNotUnique) {
    name = jack_get_client_name(jack->client);
    strncpy(jack->info.client_name, name, sizeof(jack->info.client_name));
    printke("Krad Jack: unique name `%s' assigned", jack->info.client_name);
  }

  jack->info.sample_rate = jack_get_sample_rate(jack->client);
  jack->info.period_size = jack_get_buffer_size(jack->client);
  jack_set_process_callback(jack->client, process_cb, jack);
  jack_on_shutdown(jack->client, shutdown_cb, jack);
  jack_set_xrun_callback (jack->client, xrun_cb, jack);
  //jack_set_port_registration_callback(jack->client, port_registration, jack);
  //jack_set_port_connect_callback(jack->client, port_connection, jack);

  if (jack_activate(jack->client)) {
    jack->info.active = 0;
  } else {
    jack->info.active = 1;
  }

  return jack;
}
