#include "krad_transponder_graph.h"

static char *transponder_subunit_type_to_string (xpdr_subunit_type_t type);
static int kr_xpdr_port_read (kr_xpdr_input_t *inport, void *msg);
static int kr_xpdr_port_write (kr_xpdr_input_t *input, void *msg);
static void kr_xpdr_port_disconnect (xpdr_subunit_t *xpdr_subunit,
                                     kr_xpdr_output_t *output,
                                     kr_xpdr_input_t *input);
static void kr_xpdr_port_connect (xpdr_subunit_t *xpdr_subunit,
                                  xpdr_subunit_t *from_xpdr_subunit,
                                  kr_xpdr_output_t *output,
                                  kr_xpdr_input_t *input);
static void kr_xpdr_input_disconnect (kr_xpdr_input_t *kr_xpdr_input);
static void kr_xpdr_input_destroy (kr_xpdr_input_t *kr_xpdr_input);
static void kr_xpdr_output_free (kr_xpdr_output_t *kr_xpdr_output);
static void kr_xpdr_output_destroy (kr_xpdr_output_t *kr_xpdr_output);
static kr_xpdr_output_t *kr_xpdr_output_create ();
static void xpdr_subunit_start (xpdr_subunit_t *xpdr_subunit);
static int kr_xpdr_output_set_header (kr_xpdr_output_t *outport,
                                      krad_codec_header_t *header);
static int xpdr_subunit_add (kr_xpdr_t *kr_xpdr,
                             xpdr_subunit_type_t type,
                             kr_xpdr_su_spec_t *spec);
static xpdr_subunit_t *xpdr_subunit_create (kr_xpdr_t *xpdr,
                                            xpdr_subunit_type_t type,
                                            kr_xpdr_su_spec_t *spec);
static void xpdr_subunit_destroy (xpdr_subunit_t **xpdr_subunit);
static void xpdr_subunit_stop (xpdr_subunit_t *xpdr_subunit);
static void xpdr_subunit_send_destroy_msg (xpdr_subunit_t *xpdr_subunit);
static void *xpdr_subunit_thread (void *arg);
static int xpdr_subunit_poll (xpdr_subunit_t *xpdr_subunit);
static void xpdr_subunit_handle_control_msg (xpdr_subunit_t *xpdr_subunit);
static void xpdr_subunit_disconnect_ports_actual (xpdr_subunit_t *xpdr_subunit,
                                                  kr_xpdr_output_t *output,
                                                  kr_xpdr_input_t *input);
static void xpdr_subunit_connect_ports_actual (xpdr_subunit_t *xpdr_subunit,
                                               kr_xpdr_output_t *output,
                                               kr_xpdr_input_t *input);

static char *transponder_subunit_type_to_string (xpdr_subunit_type_t type) {
  switch (type) {
    case DEMUXER:
      return "Demuxer";
    case MUXER:
      return "Muxer";
    case DECODER:
      return "Decoder";
    case ENCODER:
      return "Encoder";
    case RAW:
      return "RAW";
    case PLAYER:
      return "Player";
  }
  return "Unknown Subunit";
}

static int kr_xpdr_port_read (kr_xpdr_input_t *inport, void *msg) {

  int ret;
  uint8_t buffer[1];

  if (inport == NULL) {
    printke ("Krad XPDR subunit: kr_xpdr_port_read called with null subunit!");
    return 0;
  }
  if (msg == NULL) {
    printke ("Krad XPDR: kr_xpdr_port_read called with null msg ptr!");
    return 0;
  }

  ret = read (inport->socketpair[1], buffer, 1);
  if (ret != 1) {
    if (ret == 0) {
      printk ("Krad XPDR: port read got EOF");
      return 0;
    }
    printke ("Krad XPDR: port read unexpected read return value %d", ret);
    return 0;
  }  
  
  ret = krad_ringbuffer_read_space (inport->msg_ring);
  if (ret < sizeof(uint8_t *)) {
    printke ("Krad XPDR: Not enough in port buffer to read msg! %d bytes",
              ret); 
    return 0;
  }

  ret = krad_ringbuffer_read (inport->msg_ring,
                              (char *)msg,
                              sizeof(uint8_t *));
  if (ret != sizeof(uint8_t *)) {
    printke ("Krad XPDR: invalid msg read len %d", ret);
    return 0;
  }
  
  return 1;
}

static int kr_xpdr_port_write (kr_xpdr_input_t *input, void *msg) {

  int wrote;
  int ret;
  
  wrote = 0;
  ret = 0;

  if (input == NULL) {
    printke ("Krad XPDR subunit: kr_xpdr_port_write called with null subunit!"); 
    return 0;
  }
  if (msg == NULL) {
    printke ("Krad XPDR: kr_xpdr_port_write called with null msg ptr!");
    return 0;
  }
  
  ret = krad_ringbuffer_write_space (input->msg_ring);
  if (ret < sizeof(uint8_t *)) {
    printke ("Krad XPDR: Not enough in port buffer to write msg! %d bytes",
              ret); 
    return 0;
  }
  
  krad_ringbuffer_write (input->msg_ring, (char *)msg, sizeof(uint8_t *));
  wrote = write (input->socketpair[0], "0", 1);
  if (wrote != 1) {
    printke ("Krad XPDR: port write unexpected write return value %d", wrote);
    return 0;
  }
  //printk ("Krad Transponder: port write");
  return 1;
}

static void kr_xpdr_port_disconnect (xpdr_subunit_t *xpdr_subunit,
                                     kr_xpdr_output_t *output,
                                     kr_xpdr_input_t *input) {

  printk ("Krad Transponder: Sending disconnect ports msg");

  kr_xpdr_control_msg_t *msg;
  msg = calloc (1, sizeof (kr_xpdr_control_msg_t));
  msg->type = DISCONNECTPORTS;
  msg->input = input;
  msg->output = output;
  kr_xpdr_port_write (xpdr_subunit->control, &msg);
}

static void kr_xpdr_port_connect (xpdr_subunit_t *xpdr_subunit,
                                  xpdr_subunit_t *from_xpdr_subunit,
                                  kr_xpdr_output_t *output,
                                  kr_xpdr_input_t *input) {

  printk ("Krad Transponder: Sending connecting ports msg");

  kr_xpdr_control_msg_t *msg;
  msg = calloc (1, sizeof (kr_xpdr_control_msg_t));
  msg->type = CONNECTPORTS;
  msg->input = input;
  msg->output = output;
  
  input->connected_to_subunit = xpdr_subunit;
  input->subunit = from_xpdr_subunit;
  kr_xpdr_port_write (xpdr_subunit->control, &msg);
}

static void kr_xpdr_input_disconnect (kr_xpdr_input_t *input) {
  printk ("Krad Transponder: Disconnecting ports");
  close (input->socketpair[1]);
}

static void kr_xpdr_input_free (kr_xpdr_input_t *input) {
  krad_ringbuffer_free ( input->msg_ring );
  free (input);
}

static void kr_xpdr_input_destroy (kr_xpdr_input_t *input) {
  kr_xpdr_input_disconnect (input);
  kr_xpdr_input_free (input);
  printk ("Krad Transponder: input port destroyed");
}

static void kr_xpdr_output_free (kr_xpdr_output_t *output) {

  int h;
  
  for (h = 0; h < output->headers; h++) {
    kr_slice_unref (output->slice[h]);
  }
  free (output->connections);
  free (output);
}

static void kr_xpdr_output_destroy (kr_xpdr_output_t *output) {
  //kr_xpdr_port_disconnect (kr_xpdr_port);
  kr_xpdr_output_free (output);
  printk ("Krad XPDR: output port destroyed");
}

kr_xpdr_input_t *kr_xpdr_input_create () {

  kr_xpdr_input_t *input = calloc (1, sizeof(kr_xpdr_input_t));

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, input->socketpair)) {
    printk ("Krad XPDR: subunit could not create socketpair errno: %d", errno);
    free (input);
    return NULL;
  }

  input->msg_ring = krad_ringbuffer_create ( 200 * sizeof(kr_slice_t *) );

  return input;
}

static kr_xpdr_output_t *kr_xpdr_output_create () {
  kr_xpdr_output_t *output = calloc (1, sizeof(kr_xpdr_output_t));
  output->connections = calloc (KRAD_TRANSPONDER_PORT_CONNECTIONS,
                                sizeof(kr_xpdr_input_t *));
  return output;
}

static int
kr_xpdr_output_set_header (kr_xpdr_output_t *outport,
                           krad_codec_header_t *header) {

  int h;

  h = 0;
  if ((header != NULL) && (outport->headers == 0)) { 
    for (h = 0; h < MIN(4, header->count); h++) {
      outport->slice[h] =
        kr_slice_create_with_data (header->data[h],
                                   header->sz[h]);
      outport->slice[h]->header = h + 1;
      outport->headers++;
    }
    outport->header = header;
    return outport->headers;
  }
  return -1;
}

static void xpdr_subunit_connect_ports_actual (xpdr_subunit_t *xpdr_subunit,
                                               kr_xpdr_output_t *output,
                                               kr_xpdr_input_t *input) {

  int p;

  //printk ("Running ports actual connection su type %s", 
  //        transponder_subunit_type_to_string(xpdr_subunit->type));

  for (p = 0; p < KRAD_TRANSPONDER_PORT_CONNECTIONS; p++) {
    if (output->connections[p] == NULL) {
      output->connections[p] = input;
      printk ("Krad Transponder: Ports actually connected! %p to %p",
              output, input);
      break;
    }
  }
}

static void xpdr_subunit_disconnect_ports_actual (xpdr_subunit_t *xpdr_subunit,
                                                  kr_xpdr_output_t *output,
                                                  kr_xpdr_input_t *input) {

  int p;

  for (p = 0; p < KRAD_TRANSPONDER_PORT_CONNECTIONS; p++) {
    if (output->connections[p] == input) {
      output->connections[p] = NULL;
      
      kr_slice_t *slice;
      slice = kr_slice_create ();
      slice->final = 1;
      //slice->origin = xpdr_subunit;
      kr_xpdr_port_write (input, &slice);
      
      close (input->socketpair[0]);
      printk ("Krad Transponder: Ports actually disconnected!");
      break;
    }
  }
}

static void xpdr_subunit_handle_control_msg (xpdr_subunit_t *xpdr_subunit) {

  kr_xpdr_control_msg_t *msg;
  int ret;
  
  ret = 0;
  msg = NULL;

  ret = kr_xpdr_port_read (xpdr_subunit->control, (void **)&msg);

  if (ret != 1) {
    printke ("Krad XPDR Subunit: Could not read control message!");
    return;
  }

  switch (msg->type) {
    case CONNECTPORTS:
      xpdr_subunit_connect_ports_actual (xpdr_subunit, msg->output, msg->input);
      free (msg);
      return;
    case DISCONNECTPORTS:
      xpdr_subunit_disconnect_ports_actual (xpdr_subunit, msg->output, msg->input);    
      free (msg);
      return;
    case UPDATE:
      printk ("Krad XPDR Subunit: Got Update msg!");
      free (msg);
      return;
    case DESTROY:
      printk("Krad Transponder: Subunit Got Destroy MSG!");
      free (msg);
      xpdr_subunit->destroy = 1;
      if ((xpdr_subunit->type == MUXER) || (xpdr_subunit->type == DECODER)) {
        if (xpdr_subunit->inputs[0]->connected_to_subunit != NULL) {
          xpdr_subunit->destroy++;
          kr_xpdr_port_disconnect (xpdr_subunit->inputs[0]->connected_to_subunit,
                                            xpdr_subunit->inputs[0]->connected_to_subunit->outputs[0],
                                            xpdr_subunit->inputs[0]);
        }
        if (xpdr_subunit->type == MUXER) {
          if (xpdr_subunit->inputs[1]->connected_to_subunit != NULL) {
            xpdr_subunit->destroy++;
            kr_xpdr_port_disconnect (xpdr_subunit->inputs[1]->connected_to_subunit,
                                     xpdr_subunit->inputs[1]->connected_to_subunit->outputs[1],
                                     xpdr_subunit->inputs[1]);
          }
        }
      }
    return;
  }
}

static int xpdr_subunit_poll (xpdr_subunit_t *xpdr_subunit) {

  int n;
  int nfds;
  int ret;
  int timeout;
  int port;
  kr_slice_t *slice;
  struct pollfd pollfds[4];
  
  slice = NULL;
  port = 0;
  timeout = -1;
  n = 0;
  nfds = 0;

  pollfds[nfds].fd = xpdr_subunit->control->socketpair[1];
  pollfds[nfds].events = POLLIN;
  nfds++;
  
  if (xpdr_subunit->type == MUXER) {
    //if (xpdr_subunit->inputs[0]->connected_to_subunit != NULL) {
      pollfds[nfds].fd = xpdr_subunit->inputs[0]->socketpair[1];
      pollfds[nfds].events = POLLIN;
      nfds++;
    //}
    //if (xpdr_subunit->inputs[1]->connected_to_subunit != NULL) {
      pollfds[nfds].fd = xpdr_subunit->inputs[1]->socketpair[1];
      pollfds[nfds].events = POLLIN;
      nfds++;
    //}
  }
  
  if (xpdr_subunit->type == DECODER) {
    if (xpdr_subunit->inputs[0]->connected_to_subunit != NULL) {
      pollfds[nfds].fd = xpdr_subunit->inputs[0]->socketpair[1];
      pollfds[nfds].events = POLLIN;
      nfds++;
    }
  }
  
  if (xpdr_subunit->spec.fd > -1) {
    pollfds[nfds].fd = xpdr_subunit->spec.fd;
    pollfds[nfds].events = POLLIN;
    nfds++;
  }
  if (xpdr_subunit->spec.idle_callback_interval > 0) {
    timeout = xpdr_subunit->spec.idle_callback_interval;
  }
  
  ret = poll (pollfds, nfds, timeout);
  if (ret < 0) {
    printke ("Krad XPDR: Error polling!");
    return 0;
  }
  
  if (ret == 0) {
    if (xpdr_subunit->spec.idle_callback_interval > 0) {
      xpdr_subunit->spec.readable_callback (xpdr_subunit->spec.ptr);
    }
    return 1;
  }  

  if (ret > 0) {
    for (n = 0; n < nfds; n++) {
      if (!pollfds[n].revents) {
        continue;
      }
      if (pollfds[n].revents & POLLIN) {

        if (pollfds[n].fd == xpdr_subunit->control->socketpair[1]) {
          xpdr_subunit_handle_control_msg (xpdr_subunit);
          continue;
        }
      
        if (xpdr_subunit->type != MUXER) {
          if ((xpdr_subunit->spec.fd > -1) &&
              (pollfds[n].fd == xpdr_subunit->spec.fd)) {
            xpdr_subunit->spec.readable_callback (xpdr_subunit->spec.ptr);          
          }
        } else {

          slice = NULL;
          port = -1;

          if (pollfds[n].fd == xpdr_subunit->inputs[0]->socketpair[1]) {
            port = 0;
          }
          if (pollfds[n].fd == xpdr_subunit->inputs[1]->socketpair[1]) {
            port = 1;
          }

          if (port == -1) {
            printke ("Krad XPDR: Unknown port something on muxer");
            return 0;
          }

          ret = kr_xpdr_port_read (xpdr_subunit->inputs[port], &slice);
          if (ret != 1) {
            printke ("Krad XPDR: read port error ret %d on muxer", ret);
            return 0;
          }
          if (slice == NULL) {
            printke ("Krad XPDR: read port error null slice on muxer");
            return 0;
          }
          //printk ("Krad Transponder Subunit: Got a packet!");

          if (slice->final == 1) {
            printk ("Krad XPDR Subunit: Got FINAL packet!");
          } else {
            //printk ("Krad XPDR Subunit: packet size %u", slice->size);
          }
          xpdr_subunit->slice = slice;
          ret = xpdr_subunit->spec.readable_callback (xpdr_subunit->spec.ptr);
          xpdr_subunit->slice = NULL;
          if (ret != 0) {
            printke ("Krad XPDR: Muxer callback error!");
            return 0;
          }
        }
      }

      if (pollfds[n].revents & POLLOUT) {
        printke ("Krad XPDR Subunit: Unexpected POLLOUT");
        return 0;
      }
      
      if (pollfds[n].revents & POLLHUP) {
        printk ("Krad Transponder Subunit: Got Hangup");
        if (pollfds[n].fd == xpdr_subunit->control->socketpair[1]) {
          printk("Err! Hangup on control socket!");
          return 0;
        }
        if (xpdr_subunit->type == MUXER) {
          if (pollfds[n].fd == xpdr_subunit->inputs[0]->socketpair[1]) {
            printk ("Krad XPDR Subunit: Encoded Video Input Disconnected");
            xpdr_subunit->inputs[0]->connected_to_subunit = NULL;
            if (xpdr_subunit->destroy > 1) {
              xpdr_subunit->destroy--;
            }
          }
          if (pollfds[n].fd == xpdr_subunit->inputs[1]->socketpair[1]) {
            printk ("Krad XPDR Subunit: Encoded Audio Input Disconnected");
            xpdr_subunit->inputs[1]->connected_to_subunit = NULL;
            if (xpdr_subunit->destroy > 1) {
              xpdr_subunit->destroy--;
            }
          }
        }
      }
      if (pollfds[n].revents & POLLERR) {
        printke ("Krad XPDR Subunit: Err we got Err in Hrr!");
        if (pollfds[n].fd == xpdr_subunit->control->socketpair[1]) {
          printke ("Krad XPDR Subunit: Err on control socket!");
        }
        return 0;
      }
    }
  }
  return 1;
}

static void *xpdr_subunit_thread (void *arg) {

  xpdr_subunit_t *xpdr_subunit = (xpdr_subunit_t *)arg;
  int ret;
  
  ret = 0;
  krad_system_set_thread_name ("kr_xpdr_sunit");

  while (1) {
    ret = xpdr_subunit_poll (xpdr_subunit);
    if (ret != 1) {
      printke ("Krad XPDR Subunit: Error in poll thread exiting!");
      break;
    }  
    if (xpdr_subunit->destroy == 1) {
      break;
    }
  }
  return NULL;
}

static void xpdr_subunit_send_destroy_msg (xpdr_subunit_t *xpdr_subunit) {
  kr_xpdr_control_msg_t *msg;
  msg = calloc (1, sizeof (kr_xpdr_control_msg_t));
  msg->type = DESTROY;
  kr_xpdr_port_write (xpdr_subunit->control, &msg);
}

static void xpdr_subunit_start (xpdr_subunit_t *subunit) {
  subunit->control = kr_xpdr_input_create ();
  pthread_create (&subunit->thread, NULL, xpdr_subunit_thread, subunit);
}

static void xpdr_subunit_stop (xpdr_subunit_t *xpdr_subunit) {

  int p;
  int m;

  // Recursive Dep Destroy

  if ((xpdr_subunit->type == ENCODER) || (xpdr_subunit->type == DEMUXER)) {
    for (p = 0; p < KRAD_TRANSPONDER_PORT_CONNECTIONS; p++) {
      if (xpdr_subunit->outputs[0] != NULL) {
        if (xpdr_subunit->outputs[0]->connections[p] != NULL) {
          for (m = 0; m < KRAD_TRANSPONDER_SUBUNITS; m++) {
            if (xpdr_subunit->xpdr->subunits[m] == xpdr_subunit->outputs[0]->connections[p]->subunit) {
              xpdr_subunit_destroy (&xpdr_subunit->xpdr->subunits[m]);
              break;
            }
          }
        }
      }
      if (xpdr_subunit->outputs[1] != NULL) {
        if (xpdr_subunit->outputs[1]->connections[p] != NULL) {
          for (m = 0; m < KRAD_TRANSPONDER_SUBUNITS; m++) {
            if (xpdr_subunit->xpdr->subunits[m] == xpdr_subunit->outputs[1]->connections[p]->subunit) {
              xpdr_subunit_destroy (&xpdr_subunit->xpdr->subunits[m]);
              break;
            }
          }
        }
      }
    }
  }

  xpdr_subunit_send_destroy_msg (xpdr_subunit);
  pthread_join (xpdr_subunit->thread, NULL);
  kr_xpdr_input_destroy (xpdr_subunit->control);  
}

static void xpdr_subunit_destroy (xpdr_subunit_t **xpdr_subunit) {

  int p;
  
  if (*xpdr_subunit != NULL) {
  
    xpdr_subunit_stop (*xpdr_subunit);
    
    // maybe this happens elsewere
    for (p = 0; p < 2; p++) {
      if ((*xpdr_subunit)->inputs[p] != NULL) {
        kr_xpdr_input_destroy ((*xpdr_subunit)->inputs[p]);
      }
      if ((*xpdr_subunit)->outputs[p] != NULL) {
        kr_xpdr_output_destroy ((*xpdr_subunit)->outputs[p]);
      }
    }
    
    if ((*xpdr_subunit)->spec.destroy_callback != NULL) {
      (*xpdr_subunit)->spec.destroy_callback ((*xpdr_subunit)->spec.ptr);
    }
    
    printk ("Krad Transponder: %s subunit destroyed",
            transponder_subunit_type_to_string((*xpdr_subunit)->type));

    free ((*xpdr_subunit)->inputs);
    free ((*xpdr_subunit)->outputs);
    
    free (*xpdr_subunit);
    *xpdr_subunit = NULL;
  }
}

static xpdr_subunit_t *xpdr_subunit_create (kr_xpdr_t *xpdr,
                                               xpdr_subunit_type_t type,
                                               kr_xpdr_su_spec_t *spec) {

  int p;
  xpdr_subunit_t *xpdr_subunit = calloc (1, sizeof (xpdr_subunit_t));
  xpdr_subunit->inputs = calloc (2, sizeof (kr_xpdr_input_t *));
  xpdr_subunit->outputs = calloc (2, sizeof (kr_xpdr_output_t *));
  xpdr_subunit->xpdr = xpdr;
  xpdr_subunit->type = type;

  printk ("Krad Transponder: creating %s subunit",
          transponder_subunit_type_to_string(xpdr_subunit->type));

  switch (type) {
    case DEMUXER:
      for (p = 0; p < 2; p++) {
        xpdr_subunit->outputs[p] = kr_xpdr_output_create ();
      }
      break;
    case MUXER:
      for (p = 0; p < 2; p++) {
        xpdr_subunit->inputs[p] = kr_xpdr_input_create ();
      }
      break;
    case DECODER:
      xpdr_subunit->inputs[0] = kr_xpdr_input_create ();
      break;      
    case ENCODER:
      xpdr_subunit->outputs[0] = kr_xpdr_output_create ();
      break;
    case RAW:
      break;
    case PLAYER:
      break;
  }

  memcpy (&xpdr_subunit->spec, spec, sizeof(kr_xpdr_su_spec_t));        
  
  if ((xpdr_subunit->type == ENCODER) &&
      (xpdr_subunit->spec.encoder_header_callback != NULL)) {
    kr_xpdr_set_header (xpdr_subunit,
          xpdr_subunit->spec.encoder_header_callback (xpdr_subunit->spec.ptr));
  }

  xpdr_subunit_start (xpdr_subunit);
  printk ("Krad Transponder: %s subunit created",
          transponder_subunit_type_to_string(xpdr_subunit->type));

  return xpdr_subunit;
}

static int xpdr_subunit_add (kr_xpdr_t *kr_xpdr,
                             xpdr_subunit_type_t type,
                             kr_xpdr_su_spec_t *spec) {

  int m;

  for (m = 0; m < KRAD_TRANSPONDER_SUBUNITS; m++) {
    if (kr_xpdr->subunits[m] == NULL) {
      kr_xpdr->subunits[m] = xpdr_subunit_create (kr_xpdr, type, spec);
      return m;
    }
  }
  return -1;
}

/* Public API */

kr_slice_t *
kr_xpdr_get_slice (xpdr_subunit_t *xpdr_subunit) {
  return xpdr_subunit->slice;
}

krad_codec_header_t *
kr_xpdr_get_header (xpdr_subunit_t *xpdr_subunit) {
  return kr_xpdr_get_subunit_output_header (xpdr_subunit, 0);
}

krad_codec_header_t *
kr_xpdr_get_audio_header (xpdr_subunit_t *xpdr_subunit) {
  return kr_xpdr_get_subunit_output_header (xpdr_subunit, 1);
}

krad_codec_header_t *
kr_xpdr_get_subunit_output_header (xpdr_subunit_t *xpdr_subunit,
                                   int port) {
  if (xpdr_subunit != NULL) {
    if (xpdr_subunit->type == ENCODER) {
      return xpdr_subunit->outputs[0]->header;
    }
    if (xpdr_subunit->type == DEMUXER) {
      if (xpdr_subunit->outputs[port] != NULL) {
        return xpdr_subunit->outputs[port]->header;
      }
    }
  }
  return NULL;
}

int kr_xpdr_set_header (xpdr_subunit_t *xpdr_subunit,
                        krad_codec_header_t *header) {

  if (xpdr_subunit->type == ENCODER) {
    return kr_xpdr_output_set_header (xpdr_subunit->outputs[0], header);
  }

  if (xpdr_subunit->type == DEMUXER) {
    if (header->codec == THEORA) {
      return kr_xpdr_output_set_header (xpdr_subunit->outputs[0], header);
    }
    if ((header->codec == VORBIS) ||
        (header->codec == FLAC) ||
        (header->codec == OPUS)) {
      return kr_xpdr_output_set_header (xpdr_subunit->outputs[1], header);
    }
  }
  return -1;
}

int kr_xpdr_slice_broadcast (xpdr_subunit_t *xpdr_subunit,
                             kr_slice_t **slice) {

  int p;
  int port;
  int broadcasted;
  
  port = 0;
  broadcasted = 0;
  
  //FIXME this is because demuxers have multiple outports
  // we need to match this some other way
  if ((xpdr_subunit->type == DEMUXER) &&
      (krad_codec_is_audio((*slice)->codec))) {
    port = 1;
  }
  
  for (p = 0; p < KRAD_TRANSPONDER_PORT_CONNECTIONS; p++) {
    if (xpdr_subunit->outputs[port]->connections[p] != NULL) {
      kr_slice_ref (*slice);
      kr_xpdr_port_write (xpdr_subunit->outputs[port]->connections[p], slice);
      broadcasted++;
    }
  }

  //printk ("Krad Transponder: output port broadcasted to %d ports",
  //         broadcasted);

  return p;
}

void kr_xpdr_subunit_remove (kr_xpdr_t *xpdr, int sid) {
  if ((sid > -1) && (sid < KRAD_TRANSPONDER_SUBUNITS)) {
    if (xpdr->subunits[sid] != NULL) {
      printk ("Krad Transponder: removing subunit %d", sid);
      xpdr_subunit_destroy (&xpdr->subunits[sid]);    
    } else {
      printke ("Krad Transponder: can't remove subunit %d, not found", sid);
    }
  }
}

xpdr_subunit_t *kr_xpdr_get_subunit (kr_xpdr_t *xpdr, int sid) {
  if ((sid > -1) && (sid < KRAD_TRANSPONDER_SUBUNITS)) {
    if (xpdr->subunits[sid] != NULL) {
      printk ("Krad Transponder: found subunit %d", sid);
      return xpdr->subunits[sid];
    } else {
      printke ("Krad Transponder: can't find subunit %d", sid);
    }
  }
  return NULL;
}

int kr_xpdr_add_raw (kr_xpdr_t *xpdr, kr_xpdr_su_spec_t *spec) {
  return xpdr_subunit_add (xpdr, RAW, spec);
}

int kr_xpdr_add_decoder (kr_xpdr_t *xpdr, kr_xpdr_su_spec_t *spec) {
  return xpdr_subunit_add (xpdr, DECODER, spec);
}

int kr_xpdr_add_encoder (kr_xpdr_t *xpdr, kr_xpdr_su_spec_t *spec) {
  return xpdr_subunit_add (xpdr, ENCODER, spec);
}

int kr_xpdr_add_demuxer (kr_xpdr_t *xpdr, kr_xpdr_su_spec_t *spec) {
  return xpdr_subunit_add (xpdr, DEMUXER, spec);
}

int kr_xpdr_add_muxer (kr_xpdr_t *xpdr, kr_xpdr_su_spec_t *spec) {
  return xpdr_subunit_add (xpdr, MUXER, spec);
}

int kr_xpdr_add_player (kr_xpdr_t *xpdr, kr_xpdr_su_spec_t *spec) {
  return xpdr_subunit_add (xpdr, PLAYER, spec);
}

void kr_xpdr_subunit_connect_mux_to_video (xpdr_subunit_t *mux_subunit,
                                           xpdr_subunit_t *from) {
  kr_xpdr_port_connect (from,
                        mux_subunit,
                        from->outputs[0],
                        mux_subunit->inputs[0]);
}

void kr_xpdr_subunit_connect_mux_to_audio (xpdr_subunit_t *mux_subunit,
                                           xpdr_subunit_t *from) {
  kr_xpdr_port_connect (from,
                        mux_subunit,
                        from->outputs[0],
                        mux_subunit->inputs[1]);
}

int kr_xpdr_count (kr_xpdr_t *xpdr) {
  
  int m;
  int c;
  
  c = 0;

  for (m = 0; m < KRAD_TRANSPONDER_SUBUNITS; m++) {
    if (xpdr->subunits[m] != NULL) {
      c++;
    }
  }
  return c;
}

int kr_xpdr_get_info (kr_xpdr_t *xpdr, int num, char *string) {
  
  if (xpdr->subunits[num] != NULL) {
    sprintf (string, "Krad Transponder: Subunit %d - %s",
             num,
             transponder_subunit_type_to_string(xpdr->subunits[num]->type));      
    return 1;
  }

  return -1;
}

void *kr_xpdr_get_ptr (kr_xpdr_t *xpdr, int num) {
  
  if (xpdr->subunits[num] != NULL) {
    return xpdr->subunits[num]->spec.ptr;
  }

  return NULL;
}

void krad_xpdr_destroy (kr_xpdr_t **xpdr) {

  int m;

  if (*xpdr != NULL) {
    printk ("Krad XPDR: Destroying");
    for (m = 0; m < KRAD_TRANSPONDER_SUBUNITS; m++) {
      if ((*xpdr)->subunits[m] != NULL) {
        xpdr_subunit_destroy (&(*xpdr)->subunits[m]);
      }
    }
    free ((*xpdr)->subunits);
    free (*xpdr);
    *xpdr = NULL;
    printk ("Krad XPDR: Destroyed");
  }
}

kr_xpdr_t *krad_xpdr_create () {

  kr_xpdr_t *xpdr;
  xpdr = calloc (1, sizeof(kr_xpdr_t));
  xpdr->subunits = calloc (KRAD_TRANSPONDER_SUBUNITS,
                           sizeof(xpdr_subunit_t *));
  return xpdr;
}
