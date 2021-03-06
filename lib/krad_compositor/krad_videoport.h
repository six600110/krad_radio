#ifndef KRAD_COMPOSITOR_PORT_H
#define KRAD_COMPOSITOR_PORT_H

#include "krad_compositor_subunit.h"
#include "krad_perspective.h"
#include "krad_radio.h"

struct krad_compositor_port_St {

  krad_compositor_t *krad_compositor;

  char sysname[128];
  int direction;
  
  krad_frame_t *last_frame;
  krad_ringbuffer_t *frame_ring;

  int source_width;
  int source_height;
  
  int crop_x;
  int crop_y;
  
  int crop_width;
  int crop_height;
  
  krad_easing_t crop_x_easing;
  krad_easing_t crop_y_easing;
  krad_easing_t crop_width_easing;
  krad_easing_t crop_height_easing;
  
  int crop_start_pixel[4];
  
  struct SwsContext *sws_converter;
  int sws_algorithm;
  int yuv_color_depth;
  
  int io_params_updated;
  int comp_params_updated;
  
  uint64_t start_timecode;
  
  int local;
  int localframe_state;
  int shm_sd;
  int msg_sd;
  char *local_buffer;
  int local_buffer_size;
  krad_frame_t *local_frame;  
  
  krad_compositor_subunit_t subunit;

  krad_perspective_t *perspective;
  kr_perspective_view_t view;

  int socketpair[2];
};

void krad_compositor_videoport_set_view_top_left_x (krad_compositor_port_t 
 *videoport, int view_top_left_x); 
void krad_compositor_videoport_set_view_top_left_y (krad_compositor_port_t 
 *videoport, int view_top_left_y); 
void krad_compositor_videoport_set_view_top_right_x (krad_compositor_port_t 
 *videoport, int view_top_right_x); 
void krad_compositor_videoport_set_view_top_right_y (krad_compositor_port_t 
 *videoport, int view_top_right_y); 
void krad_compositor_videoport_set_view_bottom_left_x (krad_compositor_port_t 
 *videoport, int view_bottom_left_x); 
void krad_compositor_videoport_set_view_bottom_left_y (krad_compositor_port_t 
 *videoport, int view_bottom_left_y); 
void krad_compositor_videoport_set_view_bottom_right_x (krad_compositor_port_t 
 *videoport, int view_bottom_right_x); 
void krad_compositor_videoport_set_view_bottom_right_y (krad_compositor_port_t
 *videoport, int view_bottom_right_y); 


#endif // KRAD_COMPOSITOR_PORT_H
