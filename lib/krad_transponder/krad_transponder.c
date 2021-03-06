#include "krad_transponder.h"

#ifdef KRAD_USE_WAYLAND

int wayland_display_unit_render_callback(void *user, kr_wayland_event *event) {

  krad_link_t *krad_link = (krad_link_t *)user;

  int ret;
  char buffer[1];
  int updated;
  krad_frame_t *krad_frame;
  
  updated = 0;
  
  krad_frame = krad_compositor_port_pull_frame (krad_link->krad_compositor_port2);

  if (krad_frame != NULL) {
    //FIXME do this first etc
    ret = read (krad_link->krad_compositor_port2->socketpair[1], buffer, 1);
    if (ret != 1) {
      if (ret == 0) {
        printk ("Krad OTransponder: port read got EOF");
        return updated;
      }
      printk ("Krad OTransponder: port read unexpected read return value %d", ret);
    }

    memcpy (event->frame_event.buffer,
            krad_frame->pixels,
            krad_link->composite_width * krad_link->composite_height * 4);

    krad_framepool_unref_frame (krad_frame);
    updated = 1;
  }
  return updated;
}

int wayland_display_unit_callback(void *user, kr_wayland_event *event) {
  switch (event->type) {
    case KR_WL_FRAME:
      return wayland_display_unit_render_callback(user, event);
    case KR_WL_POINTER:
      break;
    case KR_WL_KEY:
      break;
  }
  return 0;
}

void wayland_display_unit_create (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  kr_wayland_window_params window_params;

  krad_system_set_thread_name ("kr_wl_dsp");
  
  printk ("Wayland display thread begins");
  
  krad_link->wayland = kr_wayland_create();

  krad_link->krad_compositor_port2 = krad_compositor_port_create (krad_link->krad_radio->krad_compositor, "WLOut", OUTPUT,
                                                                  krad_link->composite_width,
                                                                  krad_link->composite_height);

  window_params.width = krad_link->composite_width;
  window_params.height = krad_link->composite_height;
  window_params.callback = wayland_display_unit_callback;
  window_params.user = krad_link;

  krad_link->window = kr_wayland_window_create(krad_link->wayland, &window_params);

  printk("Wayland display running");
}

int wayland_display_unit_process (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  
  kr_wayland_process(krad_link->wayland);
  
  return 0;
}

void wayland_display_unit_destroy (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  
  kr_wayland_window_destroy(&krad_link->window);
  kr_wayland_destroy(&krad_link->wayland);
  krad_compositor_port_destroy (krad_link->krad_radio->krad_compositor,
                                krad_link->krad_compositor_port2);

  printk ("Wayland display thread exited");
}

#endif

#ifndef __MACH__
/*
void v4l2_loopout_unit_create (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  krad_system_set_thread_name ("kr_v4l2_lo");
  
  printk ("V4L2 Loop Output thread begins");
  

}

int v4l2_loopout_unit_process (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  
  return 0;
}

void v4l2_loopout_unit_destroy (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  


  printk ("V4L2 Loop Output thread exited");
  
}
*/

void v4l2_capture_unit_create (void *arg) {
  //krad_system_set_thread_name ("kr_cap_v4l2");

  krad_link_t *krad_link = (krad_link_t *)arg;

  printk ("Video capture creating..");
  
  //krad_link->video_codec = MJPEG;
  
  krad_link->krad_v4l2 = krad_v4l2_create ();

  if (krad_link->video_codec != NOCODEC) {
    if (krad_link->video_codec == MJPEG) {
      krad_v4l2_mjpeg_mode (krad_link->krad_v4l2);
    }
    if (krad_link->video_codec == H264) {
      krad_v4l2_h264_mode (krad_link->krad_v4l2);
    }
  }

  if ((krad_link->fps_numerator == 0) || (krad_link->fps_denominator == 0)) {
    krad_compositor_get_frame_rate (krad_link->krad_radio->krad_compositor,
                                    &krad_link->fps_numerator,
                                    &krad_link->fps_denominator);
  }
  
  if ((krad_link->capture_width == 0) || (krad_link->capture_height == 0)) {
    krad_compositor_get_resolution (krad_link->krad_radio->krad_compositor,
                                    &krad_link->capture_width,
                                    &krad_link->capture_height);
  }

  krad_v4l2_open (krad_link->krad_v4l2, krad_link->device, krad_link->capture_width, 
           krad_link->capture_height, 30);

  if ((krad_link->capture_width != krad_link->krad_v4l2->width) ||
      (krad_link->capture_height != krad_link->krad_v4l2->height)) {

    printke ("Got a different resolution from V4L2 than requested.");
    printk ("Wanted: %dx%d Got: %dx%d",
        krad_link->capture_width, krad_link->capture_height,
        krad_link->krad_v4l2->width, krad_link->krad_v4l2->height
        );
         
    krad_link->capture_width = krad_link->krad_v4l2->width;
    krad_link->capture_height = krad_link->krad_v4l2->height;
  }

  if (krad_link->video_passthru == 1) {
    krad_link->krad_framepool = krad_framepool_create_for_passthru (350000, DEFAULT_CAPTURE_BUFFER_FRAMES * 3);
  } else {

    krad_link->krad_framepool = krad_framepool_create_for_upscale ( krad_link->capture_width,
                              krad_link->capture_height,
                              DEFAULT_CAPTURE_BUFFER_FRAMES,
                              krad_link->composite_width, krad_link->composite_height);
  }

  if (krad_link->video_passthru == 1) {
    //FIXME
  } else {
    krad_link->krad_compositor_port = 
    krad_compositor_port_create (krad_link->krad_radio->krad_compositor, "V4L2In", INPUT,
                   krad_link->capture_width, krad_link->capture_height);
  }

  krad_v4l2_start_capturing (krad_link->krad_v4l2);

  printk ("Video capture started..");
}

int v4l2_capture_unit_process (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  void *captured_frame;
  krad_frame_t *krad_frame;
  
  captured_frame = NULL;
  krad_frame = NULL;

  captured_frame = krad_v4l2_read (krad_link->krad_v4l2);    

  if (captured_frame != NULL) {
    krad_frame = krad_framepool_getframe (krad_link->krad_framepool);
    if (krad_frame != NULL) {
      if (krad_link->video_passthru == 1) {
        memcpy (krad_frame->pixels, captured_frame, krad_link->krad_v4l2->encoded_size);
        krad_frame->encoded_size = krad_link->krad_v4l2->encoded_size;
        krad_compositor_port_push_frame (krad_link->krad_compositor_port, krad_frame);
      } else {
        if (krad_link->video_codec == MJPEG) {
          krad_v4l2_mjpeg_to_rgb (krad_link->krad_v4l2, (unsigned char *)krad_frame->pixels,
                                 captured_frame, krad_link->krad_v4l2->encoded_size);      
          krad_compositor_port_push_rgba_frame (krad_link->krad_compositor_port, krad_frame);
        } else {
          krad_frame->format = PIX_FMT_YUYV422;
          krad_frame->yuv_pixels[0] = captured_frame;
          krad_frame->yuv_pixels[1] = NULL;
          krad_frame->yuv_pixels[2] = NULL;
          krad_frame->yuv_strides[0] = krad_link->capture_width + (krad_link->capture_width/2) * 2;
          krad_frame->yuv_strides[1] = 0;
          krad_frame->yuv_strides[2] = 0;
          krad_frame->yuv_strides[3] = 0;
          krad_compositor_port_push_yuv_frame (krad_link->krad_compositor_port, krad_frame);
        }
      }
      krad_framepool_unref_frame (krad_frame);
    }
    krad_v4l2_frame_done (krad_link->krad_v4l2);
  }

  return 0;
}

void v4l2_capture_unit_destroy (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  krad_v4l2_stop_capturing (krad_link->krad_v4l2);
  krad_v4l2_close(krad_link->krad_v4l2);
  krad_v4l2_destroy(krad_link->krad_v4l2);

  krad_compositor_port_destroy (krad_link->krad_radio->krad_compositor, krad_link->krad_compositor_port);

  printk ("v4l2 capture unit destroy");
}

#endif

#ifdef KR_LINUX
#ifdef KRAD_USE_X11
void x11_capture_unit_create (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  krad_system_set_thread_name ("kr_x11_cap");
  
  printk ("X11 capture thread begins");
  
  krad_link->krad_x11 = krad_x11_create();
  
  if (krad_link->video_source == X11) {
    krad_link->krad_framepool = krad_framepool_create ( krad_link->krad_x11->screen_width,
                              krad_link->krad_x11->screen_height,
                              DEFAULT_CAPTURE_BUFFER_FRAMES);
  }
  
  krad_x11_enable_capture (krad_link->krad_x11, 0);
  
  krad_link->krad_compositor_port = krad_compositor_port_create (krad_link->krad_radio->krad_compositor,
                                   "X11In",
                                   INPUT,
                                   krad_link->krad_x11->screen_width, krad_link->krad_x11->screen_height);
}

int x11_capture_unit_process (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  
  krad_frame_t *krad_frame;

  if (krad_link->krad_ticker == NULL) {
    krad_link->krad_ticker = krad_ticker_create (krad_link->krad_radio->krad_compositor->fps_numerator,
                      krad_link->krad_radio->krad_compositor->fps_denominator);
    krad_ticker_start (krad_link->krad_ticker);
  } else {
    krad_ticker_wait (krad_link->krad_ticker);
  }
  
  krad_frame = krad_framepool_getframe (krad_link->krad_framepool);

  krad_x11_capture (krad_link->krad_x11, (unsigned char *)krad_frame->pixels);
  
  krad_compositor_port_push_rgba_frame (krad_link->krad_compositor_port, krad_frame);

  krad_framepool_unref_frame (krad_frame);

  return 0;
}

void x11_capture_unit_destroy (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  
  krad_compositor_port_destroy (krad_link->krad_radio->krad_compositor, krad_link->krad_compositor_port);

  krad_ticker_destroy (krad_link->krad_ticker);
  krad_link->krad_ticker = NULL;

  krad_x11_destroy (krad_link->krad_x11);

  printk ("X11 capture thread exited");
}

#endif
#endif

#ifdef KRAD_USE_FLYCAP
void flycap_capture_unit_create (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  printk ("Flycap capture begins");
  
  krad_link->fc = kr_fc2_create ();
  
  krad_link->capture_width = 640;
  krad_link->capture_height = 480;
  
  //krad_link->krad_framepool = krad_framepool_create ( krad_link->capture_width,
  //                          krad_link->capture_height,
  //                          DEFAULT_CAPTURE_BUFFER_FRAMES);

  if ((krad_link->composite_width == 0) || (krad_link->composite_height == 0)) {
    krad_compositor_get_resolution (krad_link->krad_radio->krad_compositor,
                                    &krad_link->composite_width,
                                    &krad_link->composite_height);
  }

  krad_link->krad_framepool = krad_framepool_create_for_upscale ( krad_link->capture_width,
                            krad_link->capture_height,
                            DEFAULT_CAPTURE_BUFFER_FRAMES,
                            krad_link->composite_width, krad_link->composite_height);
 
  krad_link->krad_compositor_port = krad_compositor_port_create (krad_link->krad_radio->krad_compositor,
                                   "FC2in",
                                   INPUT,
                                   krad_link->capture_width,
                                   krad_link->capture_height);

  kr_fc2_capture_start (krad_link->fc);
}

int flycap_capture_unit_process (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  if (krad_link->krad_ticker == NULL) {
    krad_link->krad_ticker = krad_ticker_create (krad_link->krad_radio->krad_compositor->fps_numerator,
                      krad_link->krad_radio->krad_compositor->fps_denominator);
    krad_ticker_start (krad_link->krad_ticker);
  } else {
    krad_ticker_wait (krad_link->krad_ticker);
  }

  krad_frame_t *frame;

  frame = krad_framepool_getframe (krad_link->krad_framepool);

  kr_fc2_capture_image (krad_link->fc, frame);

  krad_compositor_port_push_yuv_frame (krad_link->krad_compositor_port, frame);

  krad_framepool_unref_frame (frame);

  return 0;
}

void flycap_capture_unit_destroy (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  
  kr_fc2_capture_stop (krad_link->fc);
  
  krad_compositor_port_destroy (krad_link->krad_radio->krad_compositor,
                                krad_link->krad_compositor_port);

  krad_ticker_destroy (krad_link->krad_ticker);
  krad_link->krad_ticker = NULL;

  kr_fc2_destroy (&krad_link->fc);

  printk ("Flycap capture done");
}

#endif

int krad_link_decklink_video_callback (void *arg, void *buffer, int length) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  int stride;
  krad_frame_t *krad_frame;
  
  stride = krad_link->capture_width + ((krad_link->capture_width/2) * 2);
  //printk ("krad link decklink frame received %d bytes", length);

  krad_frame = krad_framepool_getframe (krad_link->krad_framepool);

  if (krad_frame != NULL) {

    krad_frame->format = PIX_FMT_UYVY422;

    krad_frame->yuv_pixels[0] = buffer;
    krad_frame->yuv_pixels[1] = NULL;
    krad_frame->yuv_pixels[2] = NULL;

    krad_frame->yuv_strides[0] = stride;
    krad_frame->yuv_strides[1] = 0;
    krad_frame->yuv_strides[2] = 0;
    krad_frame->yuv_strides[3] = 0;

    krad_compositor_port_push_yuv_frame (krad_link->krad_compositor_port, krad_frame);
/*
    krad_frame->format = PIX_FMT_RGB32;
    krad_frame->pixels = buffer;    
    krad_compositor_port_push_rgba_frame (krad_link->krad_compositor_port, krad_frame);
*/
    krad_framepool_unref_frame (krad_frame);
    //krad_compositor_process (krad_link->krad_transponder->krad_radio->krad_compositor);
  } else {
    //failfast ("Krad Decklink underflow");
  }

  return 0;
}

#define SAMPLE_16BIT_SCALING 32767.0f

void krad_link_int16_to_float (float *dst, char *src, unsigned long nsamples, unsigned long src_skip) {

  const float scaling = 1.0/SAMPLE_16BIT_SCALING;
  while (nsamples--) {
    *dst = (*((short *) src)) * scaling;
    dst++;
    src += src_skip;
  }
}

int krad_link_decklink_audio_callback (void *arg, void *buffer, int frames) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  int c;

  for (c = 0; c < 2; c++) {
    krad_link_int16_to_float ( krad_link->krad_decklink->samples[c], (char *)buffer + (c * 2), frames, 4);
    krad_ringbuffer_write (krad_link->audio_capture_ringbuffer[c], (char *)krad_link->krad_decklink->samples[c], frames * 4);
  }
  krad_link->audio_frames_captured += frames;
  return 0;
}

void decklink_capture_unit_create (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  int c;

  krad_system_set_thread_name ("kr_decklink");

  krad_link->krad_decklink = krad_decklink_create (krad_link->device);
  
  if ((krad_link->fps_numerator == 0) || (krad_link->fps_denominator == 0)) {
    krad_compositor_get_frame_rate (krad_link->krad_radio->krad_compositor,
                                    &krad_link->fps_numerator,
                                    &krad_link->fps_denominator);
  }
  
  if ((krad_link->capture_width == 0) || (krad_link->capture_height == 0)) {
    krad_compositor_get_resolution (krad_link->krad_radio->krad_compositor,
                                    &krad_link->capture_width,
                                    &krad_link->capture_height);
  }
  
  //krad_decklink_set_video_mode (krad_link->krad_decklink,
  //                              krad_link->capture_width, krad_link->capture_height,
  //                              krad_link->fps_numerator, krad_link->fps_denominator);


  krad_decklink_set_video_mode (krad_link->krad_decklink,
                                krad_link->capture_width, krad_link->capture_height,
                                60000, 1001);


  krad_decklink_set_audio_input (krad_link->krad_decklink, krad_link->audio_input);
  krad_decklink_set_video_input (krad_link->krad_decklink, "hdmi");

  for (c = 0; c < krad_link->channels; c++) {
    krad_link->audio_capture_ringbuffer[c] = krad_ringbuffer_create (1000000);    
  }

  krad_link->krad_framepool = krad_framepool_create ( krad_link->capture_width,
                                                      krad_link->capture_height,
                                                      DEFAULT_CAPTURE_BUFFER_FRAMES);

  krad_link->krad_mixer_portgroup = krad_mixer_portgroup_create (krad_link->krad_radio->krad_mixer,
                                                                 krad_link->krad_decklink->simplename,
                                                                 INPUT, NOTOUTPUT, 2, 0.0f,
                                                                 krad_link->krad_radio->krad_mixer->master_mix,
                                                                 KRAD_LINK, krad_link, 0);  
  
  krad_mixer_set_portgroup_control (krad_link->krad_radio->krad_mixer,
                                    krad_link->krad_decklink->simplename,
                                    "volume", 100.0f, 500, NULL);
  
  krad_link->krad_compositor_port = krad_compositor_port_create (krad_link->krad_radio->krad_compositor,
                                                                 krad_link->krad_decklink->simplename,
                                                                 INPUT, krad_link->capture_width,
                                                                 krad_link->capture_height);

  krad_link->krad_decklink->callback_pointer = krad_link;
  krad_link->krad_decklink->audio_frames_callback = krad_link_decklink_audio_callback;
  krad_link->krad_decklink->video_frame_callback = krad_link_decklink_video_callback;
  krad_decklink_start (krad_link->krad_decklink);
}

void decklink_capture_unit_destroy (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;
  int c;

  if (krad_link->krad_decklink != NULL) {
    krad_decklink_destroy ( krad_link->krad_decklink );
    krad_link->krad_decklink = NULL;
  }

  krad_mixer_portgroup_destroy (krad_link->krad_radio->krad_mixer, krad_link->krad_mixer_portgroup);
  krad_compositor_port_destroy (krad_link->krad_radio->krad_compositor, krad_link->krad_compositor_port);
  
  for (c = 0; c < krad_link->channels; c++) {
    krad_ringbuffer_free ( krad_link->audio_capture_ringbuffer[c] );
  }
}

void video_encoding_unit_create (void *arg) {
  //krad_system_set_thread_name ("kr_video_enc");

  krad_link_t *krad_link = (krad_link_t *)arg;

  //printk ("Video encoding thread started");

  krad_link->color_depth = PIX_FMT_YUV420P;

  /* CODEC SETUP */

  if (krad_link->codec == VP8) {

  krad_link->krad_vpx_encoder = krad_vpx_encoder_create (krad_link->encoding_width,
                                                         krad_link->encoding_height,
                                                         krad_link->encoding_fps_numerator,
                                                         krad_link->encoding_fps_denominator,                                 
                                                         krad_link->vp8_bitrate);

    //if (krad_link->type == TRANSMIT) {
    krad_link->krad_vpx_encoder->cfg.kf_min_dist = 10;
    krad_link->krad_vpx_encoder->cfg.kf_max_dist = 90;
    //}

    //if (krad_link->type == RECORD) {
    //  krad_link->krad_vpx_encoder->cfg.rc_min_quantizer = 5;
    //  krad_link->krad_vpx_encoder->cfg.rc_max_quantizer = 35;          
    //}

    krad_vpx_encoder_config_set (krad_link->krad_vpx_encoder, &krad_link->krad_vpx_encoder->cfg);

    krad_vpx_encoder_deadline_set (krad_link->krad_vpx_encoder, 5000);

    krad_vpx_encoder_print_config (krad_link->krad_vpx_encoder);
  }

  if (krad_link->codec == THEORA) {
    krad_link->krad_theora_encoder = krad_theora_encoder_create (krad_link->encoding_width, 
                                                                 krad_link->encoding_height,
                                                                 krad_link->encoding_fps_numerator,
                                                                 krad_link->encoding_fps_denominator,
                                                                 krad_link->color_depth,
                                                                 krad_link->theora_quality);
  }

  if (krad_link->codec == Y4M) {
    krad_link->krad_y4m = krad_y4m_create (krad_link->encoding_width, krad_link->encoding_height, krad_link->color_depth);
  }

  if (krad_link->codec == KVHS) {
    krad_link->krad_vhs = krad_vhs_create_encoder (krad_link->encoding_width, krad_link->encoding_height);
  }

  /* COMPOSITOR CONNECTION */

  krad_link->krad_compositor_port = krad_compositor_port_create (krad_link->krad_radio->krad_compositor,
                                                                 "VIDEnc",
                                                                 OUTPUT,
                                                                 krad_link->encoding_width, 
                                                                 krad_link->encoding_height);

  krad_link->krad_compositor_port_fd = krad_compositor_port_get_fd (krad_link->krad_compositor_port);

}

krad_codec_header_t *video_encoding_unit_get_header (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  switch (krad_link->codec) {
    case THEORA:
      return &krad_link->krad_theora_encoder->krad_codec_header;
      break;
    default:
      break;
  }
  return NULL;
}

int video_encoding_unit_process (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  int ret;
  char buffer[1];
  krad_frame_t *krad_frame;
  kr_slice_t *kr_slice;  
  void *video_packet;
  int keyframe;
  int packet_size;
  unsigned char *planes[3];
  int strides[3];

  packet_size = 0;
  krad_frame = NULL;
  kr_slice = NULL;

  if (krad_link->codec == VP8) {
    planes[0] = krad_link->krad_vpx_encoder->image->planes[0];
    planes[1] = krad_link->krad_vpx_encoder->image->planes[1];
    planes[2] = krad_link->krad_vpx_encoder->image->planes[2];
    strides[0] = krad_link->krad_vpx_encoder->image->stride[0];
    strides[1] = krad_link->krad_vpx_encoder->image->stride[1];
    strides[2] = krad_link->krad_vpx_encoder->image->stride[2];
  }

  if (krad_link->codec == THEORA) {
    planes[0] = krad_link->krad_theora_encoder->ycbcr[0].data;
    planes[1] = krad_link->krad_theora_encoder->ycbcr[1].data;
    planes[2] = krad_link->krad_theora_encoder->ycbcr[2].data;
    strides[0] = krad_link->krad_theora_encoder->ycbcr[0].stride;
    strides[1] = krad_link->krad_theora_encoder->ycbcr[1].stride;
    strides[2] = krad_link->krad_theora_encoder->ycbcr[2].stride;  
  }

  if (krad_link->codec == Y4M) {
    planes[0] = krad_link->krad_y4m->planes[0];
    planes[1] = krad_link->krad_y4m->planes[1];
    planes[2] = krad_link->krad_y4m->planes[2];
    strides[0] = krad_link->krad_y4m->strides[0];
    strides[1] = krad_link->krad_y4m->strides[1];
    strides[2] = krad_link->krad_y4m->strides[2];
  }
  
  ret = read (krad_link->krad_compositor_port->socketpair[1], buffer, 1);
  if (ret != 1) {
    if (ret == 0) {
      printk ("Krad OTransponder: port read got EOF");
      return -1;
    }
    printk ("Krad OTransponder: port read unexpected read return value %d", ret);
  }

  if (krad_link->codec == KVHS) {
    krad_frame = krad_compositor_port_pull_frame (krad_link->krad_compositor_port);
  } else {
    krad_frame = krad_compositor_port_pull_yuv_frame (krad_link->krad_compositor_port, planes, strides, krad_link->color_depth);
  }

  if (krad_frame != NULL) {

    /* ENCODE FRAME */

    if (krad_link->codec == VP8) {
      packet_size = krad_vpx_encoder_write (krad_link->krad_vpx_encoder,
                                            (unsigned char **)&video_packet,
                                            &keyframe);
    }

    if (krad_link->codec == THEORA) {
      packet_size = krad_theora_encoder_write (krad_link->krad_theora_encoder,
                                               (unsigned char **)&video_packet,
                                               &keyframe);
    }

    if (krad_link->codec == KVHS) {
      packet_size = krad_vhs_encode (krad_link->krad_vhs, (unsigned char *)krad_frame->pixels);
      if (krad_link->subunit != NULL) {
        kr_slice = kr_slice_create_with_data (krad_link->krad_vhs->enc_buffer, packet_size);
        kr_slice->frames = 1;
        kr_slice->codec = krad_link->codec;
        kr_slice->keyframe = 1;
        kr_xpdr_slice_broadcast (krad_link->subunit, &kr_slice);
        kr_slice_unref (kr_slice);
      }
    } else {

      if (krad_link->codec == Y4M) {
        /*
        keyframe_char[0] = 1;

        packet_size = krad_link->krad_y4m->frame_size + Y4M_FRAME_HEADER_SIZE;

        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, (char *)&packet_size, 4);
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, keyframe_char, 1);
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, (char *)Y4M_FRAME_HEADER, Y4M_FRAME_HEADER_SIZE);
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, (char *)krad_link->krad_y4m->planes[0], krad_link->krad_y4m->size[0]);
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, (char *)krad_link->krad_y4m->planes[1], krad_link->krad_y4m->size[1]);
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, (char *)krad_link->krad_y4m->planes[2], krad_link->krad_y4m->size[2]);
        */
      } else {

        if ((packet_size) || (krad_link->codec == THEORA)) {
          if (krad_link->subunit != NULL) {
            kr_slice = kr_slice_create_with_data (video_packet, packet_size);
            kr_slice->frames = 1;
            kr_slice->codec = krad_link->codec;
            kr_slice->keyframe = keyframe;
            kr_xpdr_slice_broadcast (krad_link->subunit, &kr_slice);
            kr_slice_unref (kr_slice);
          }
        }
      }
    }
    krad_framepool_unref_frame (krad_frame);
  }

  return 0;
}
  
void video_encoding_unit_destroy (void *arg) {
  
  krad_link_t *krad_link = (krad_link_t *)arg;  
  
//  void *video_packet;
//  int keyframe;
//  int packet_size;
  
  printk ("Video encoding unit destroying");  

  krad_compositor_port_destroy (krad_link->krad_radio->krad_compositor, krad_link->krad_compositor_port);

  if (krad_link->codec == VP8) {
    /*
    krad_vpx_encoder_finish (krad_link->krad_vpx_encoder);
    do {
      packet_size = krad_vpx_encoder_write (krad_link->krad_vpx_encoder,
                                            (unsigned char **)&video_packet,
                                            &keyframe);
      if (packet_size) {
        //FIXME goes with un needed memcpy above
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, (char *)&packet_size, 4);
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, keyframe_char, 1);
        krad_ringbuffer_write (krad_link->encoded_video_ringbuffer, (char *)video_packet, packet_size);
      }
    } while (packet_size);
    */
    krad_vpx_encoder_destroy (&krad_link->krad_vpx_encoder);
  }

  if (krad_link->codec == THEORA) {
    krad_theora_encoder_destroy (krad_link->krad_theora_encoder);  
  }

  if (krad_link->codec == Y4M) {
    krad_y4m_destroy (krad_link->krad_y4m);  
  }

  if (krad_link->codec == KVHS) {
    krad_vhs_destroy (krad_link->krad_vhs);  
  }  

  printk ("Video encoding unit exited");
}

void audio_encoding_unit_create (void *arg) {

  //krad_system_set_thread_name ("kr_audio_enc");

  krad_link_t *krad_link = (krad_link_t *)arg;

  int c;

  printk ("Audio unit create");
  
  if (krad_link->codec != VORBIS) {
    krad_link->au_buffer = malloc (300000);
  }
  
  krad_link->au_interleaved_samples = malloc (8192 * 4 * KRAD_MIXER_MAX_CHANNELS);
  
  for (c = 0; c < krad_link->channels; c++) {
    krad_link->au_samples[c] = malloc (8192 * 4);
    krad_link->samples[c] = malloc (8192 * 4);
    krad_link->audio_input_ringbuffer[c] = krad_ringbuffer_create (2000000);    
  }
  
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, krad_link->socketpair)) {
    printk ("Krad Compositor: subunit could not create socketpair errno: %d", errno);
    return;
  }
  
  krad_link->mixer_portgroup = krad_mixer_portgroup_create (krad_link->krad_radio->krad_mixer, krad_link->sysname, 
                                                            OUTPUT, DIRECT, krad_link->channels, 0.0f,
                                                            krad_link->krad_radio->krad_mixer->master_mix,
                                                            KRAD_LINK, krad_link, 0);    
    
  switch (krad_link->codec) {
    case VORBIS:
      krad_link->krad_vorbis = krad_vorbis_encoder_create (krad_link->channels,
                                 krad_link->krad_radio->krad_mixer->sample_rate,
                                 krad_link->vorbis_quality);
      //krad_link->au_framecnt = KRAD_DEFAULT_VORBIS_FRAME_SIZE;
      break;
    case FLAC:
      krad_link->krad_flac = krad_flac_encoder_create (krad_link->channels,
                               krad_link->krad_radio->krad_mixer->sample_rate,
                               krad_link->flac_bit_depth);
      krad_link->au_framecnt = KRAD_DEFAULT_FLAC_FRAME_SIZE;
      break;
    case OPUS:
      krad_link->krad_opus = krad_opus_encoder_create (krad_link->channels,
                               krad_link->krad_radio->krad_mixer->sample_rate,
                               krad_link->opus_bitrate,
                               OPUS_APPLICATION_AUDIO);
      krad_link->au_framecnt = KRAD_MIN_OPUS_FRAME_SIZE;
      break;      
    default:
      failfast ("Krad Link Audio Encoder: Unknown Audio Codec");
  }
  krad_link->audio_encoder_ready = 1;
}

krad_codec_header_t *audio_encoding_unit_get_header (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  switch (krad_link->codec) {
    case VORBIS:
      return &krad_link->krad_vorbis->header;
      break;
    case FLAC:
      return &krad_link->krad_flac->krad_codec_header;
      break;
    case OPUS:
      return &krad_link->krad_opus->krad_codec_header;
      break;      
    default:
      failfast ("Krad Link Audio Encoder: Unknown Audio Codec");
  }
  return NULL;
}
  
int audio_encoding_unit_process (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  int c;
  int s;
  int bytes;
  int frames;
  int ret;
  char buffer[1];
  kr_slice_t *kr_slice;

  bytes = 0;
  kr_slice = NULL;
  
  ret = read (krad_link->socketpair[1], buffer, 1);
  if (ret != 1) {
    if (ret == 0) {
      printk ("Krad AU Transponder: port read got EOF");
      return -1;
    }
    printk ("Krad AU Transponder: port read unexpected read return value %d", ret);
  }
  
  if (krad_link->codec != VORBIS) {
    frames = krad_link->au_framecnt;
  }

  while (krad_ringbuffer_read_space(krad_link->audio_input_ringbuffer[krad_link->channels - 1]) >= krad_link->au_framecnt * 4) {

    if (krad_link->codec == OPUS) {
      for (c = 0; c < krad_link->channels; c++) {
        krad_ringbuffer_read (krad_link->audio_input_ringbuffer[c], (char *)krad_link->au_samples[c], (krad_link->au_framecnt * 4) );
        krad_opus_encoder_write (krad_link->krad_opus, c + 1, (char *)krad_link->au_samples[c], krad_link->au_framecnt * 4);
      }
      bytes = krad_opus_encoder_read (krad_link->krad_opus, krad_link->au_buffer, &krad_link->au_framecnt);
    }
    if (krad_link->codec == FLAC) {
      for (c = 0; c < krad_link->channels; c++) {
        krad_ringbuffer_read (krad_link->audio_input_ringbuffer[c], (char *)krad_link->au_samples[c], (krad_link->au_framecnt * 4) );
      }
      for (s = 0; s < krad_link->au_framecnt; s++) {
        for (c = 0; c < krad_link->channels; c++) {
          krad_link->au_interleaved_samples[s * krad_link->channels + c] = krad_link->au_samples[c][s];
        }
      }
      bytes = krad_flac_encode (krad_link->krad_flac, krad_link->au_interleaved_samples, krad_link->au_framecnt, krad_link->au_buffer);
    }
    if (krad_link->codec == VORBIS) {
    /*


      medium = kr_medium_kludge_create ();
      codeme = kr_codeme_kludge_create ();


      for (c = 0; c < krad_link->channels; c++) {
        krad_ringbuffer_read (krad_link->audio_input_ringbuffer[c], (char *)float_buffer[c], krad_link->au_framecnt * 4);
      }      
      
                ret = kr_vorbis_encode (vorbis_enc, codeme, medium);
          if (ret == 1) {
            kr_mkv_add_audio (new_mkv, 2, codeme->data, codeme->sz, codeme->count);
          }
          kr_medium_kludge_destroy (&medium);
          kr_codeme_kludge_destroy (&codeme);

      bytes = 
    */
    }

    while (bytes > 0) {
      if (krad_link->subunit != NULL) {
        kr_slice = kr_slice_create_with_data (krad_link->au_buffer, bytes);
        kr_slice->frames = frames;
        kr_slice->codec = krad_link->codec;
        kr_xpdr_slice_broadcast (krad_link->subunit, &kr_slice);
        kr_slice_unref (kr_slice);
      }
      bytes = 0;
      if (krad_link->codec == VORBIS) {
      //  bytes = krad_vorbis_encoder_read (krad_link->krad_vorbis, &frames, &krad_link->au_buffer);
      }
      if (krad_link->codec == OPUS) {
        bytes = krad_opus_encoder_read (krad_link->krad_opus, krad_link->au_buffer, &krad_link->au_framecnt);
      }
    }
  }

  return 0;
}

void audio_encoding_unit_destroy (void *arg) {
  
  krad_link_t *krad_link = (krad_link_t *)arg;

  krad_mixer_portgroup_destroy (krad_link->krad_radio->krad_mixer, krad_link->mixer_portgroup);
  
  int c;
  //unsigned char *vorbis_buffer;
  //int bytes;
  //int frames;
  
  if (krad_link->krad_vorbis != NULL) {
  
//    krad_vorbis_encoder_finish (krad_link->krad_vorbis);
/*
    // DUPEY
    bytes = krad_vorbis_encoder_read (krad_link->krad_vorbis, &frames, &vorbis_buffer);
    while (bytes > 0) {
      krad_ringbuffer_write (krad_link->encoded_audio_ringbuffer, (char *)&bytes, 4);
      krad_ringbuffer_write (krad_link->encoded_audio_ringbuffer, (char *)&frames, 4);
      krad_ringbuffer_write (krad_link->encoded_audio_ringbuffer, (char *)vorbis_buffer, bytes);
      bytes = krad_vorbis_encoder_read (krad_link->krad_vorbis, &frames, &vorbis_buffer);
    }
*/
    krad_vorbis_encoder_destroy (&krad_link->krad_vorbis);
  }
  
  if (krad_link->krad_flac != NULL) {
    krad_flac_encoder_destroy (krad_link->krad_flac);
    krad_link->krad_flac = NULL;
  }

  if (krad_link->krad_opus != NULL) {
    krad_opus_encoder_destroy (krad_link->krad_opus);
    krad_link->krad_opus = NULL;
  }
  
  close (krad_link->socketpair[0]);
  close (krad_link->socketpair[1]);
  
  for (c = 0; c < krad_link->channels; c++) {
    free (krad_link->samples[c]);
    free (krad_link->au_samples[c]);
    krad_ringbuffer_free (krad_link->audio_input_ringbuffer[c]);    
  }  
  
  free (krad_link->au_interleaved_samples);

  if (krad_link->codec != VORBIS) {
    free (krad_link->au_buffer);
  }
  printk ("Audio encoding thread exiting");
}

void muxer_unit_create (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  //krad_system_set_thread_name ("kr_stream_out");
  
  printk ("Output/Muxing thread starting");

  if (krad_link->host[0] != '\0') {
  /*
    if ((strcmp(krad_link->host, "transmitter") == 0) &&
      (krad_link->krad_transponder->krad_transmitter->listening == 1)) {
      
        failfast ("temp disabled transission");
      
      krad_link->muxer_krad_transmission = krad_transmitter_transmission_create (krad_link->krad_transponder->krad_transmitter,
                                    krad_link->mount + 1,
                                    krad_container_select_mimetype (krad_link->mount + 1));

      krad_link->port = krad_link->krad_transponder->krad_transmitter->port;

      //krad_link->krad_container = krad_container_open_transmission (krad_link->muxer_krad_transmission);
  
    } else {
  */
      krad_link->krad_container = krad_container_create_stream (krad_link->host,
                                  krad_link->port,
                                  krad_link->mount,
                                  krad_link->password);
    //}                                  
  } else {
    printk ("Outputing to file: %s", krad_link->output);
    krad_link->krad_container = krad_container_open_file (krad_link->output, KRAD_IO_WRITEONLY);
  }

  printk ("Muxing setup.");
}

static int connect_muxer_to_encoders (krad_link_t *link) {

  char *pch;
  char *save;
  int t;
  int conns;
  krad_codec_header_t *codec_header;
  int track_num;

  track_num = 0;
  codec_header = NULL;
  conns = 0;
	kr_xpdr_subunit_t *subunit;
  t = 0;
  save = NULL;

  pch = strtok_r (link->input, ", ", &save);
  while (pch != NULL) {
    if (pch[0] == '/') {
      break;
    }
    t = atoi (pch);
    subunit = kr_xpdr_get_subunit (link->krad_transponder->xpdr, t);
    if (subunit != NULL) {
      if (conns == 0) {
        printk ("calling mux to video conn");
        kr_xpdr_subunit_connect_mux_to_video (link->subunit, subunit);
      } else {
        printk ("calling mux to audio conn");
        kr_xpdr_subunit_connect_mux_to_audio (link->subunit, subunit);
      }
      conns++;
      codec_header = kr_xpdr_get_header (subunit);
      if (codec_header == NULL) {
        //FIXME this isn't exactly solid evidence
        track_num = krad_container_add_video_track (link->krad_container,
                                                    VP8,
                                                    link->encoding_fps_numerator,
                                                    link->encoding_fps_denominator,
                                                    link->encoding_width,
                                                    link->encoding_height);
      } else {
        if (krad_codec_is_video(codec_header->codec)) {
          if (codec_header->codec == THEORA) {
            track_num = krad_container_add_video_track_with_private_data (link->krad_container,
                                                                          codec_header,
                                                                          link->encoding_fps_numerator,
                                                                          link->encoding_fps_denominator,
                                                                          link->encoding_width,
                                                                          link->encoding_height);
          }
        } else {
          if (krad_codec_is_audio(codec_header->codec)) {
            track_num = krad_container_add_audio_track (link->krad_container,
                                                        codec_header->codec,
                                                        link->krad_radio->krad_mixer->sample_rate,
                                                        2, 
                                                        codec_header);
          }
        }
      }
      link->track_sources[track_num] = subunit;
    }
    pch = strtok_r (NULL, ", ", &save);
  }
  
  return conns;
}

int muxer_unit_process (void *arg) {

  krad_system_set_thread_name ("kr_muxer");

  krad_link_t *link = (krad_link_t *)arg;

  kr_slice_t *slice;
  slice = NULL;
  
  slice = kr_xpdr_get_slice (link->subunit);

  if (slice == NULL) {
    printke ("Muxer got a null slice!");
    return -1;
  }
  
  if (krad_codec_is_video(slice->codec)) {
    krad_container_add_video (link->krad_container, 
                              1,
                              slice->data,
                              slice->size,
                              slice->keyframe);
  } else {
    if (krad_codec_is_audio(slice->codec)) {
      krad_container_add_audio (link->krad_container,
                                2,
                                slice->data,
                                slice->size,
                                slice->frames);
    } else {
      printke ("Muxer Could not match codec to track!");
      return -1;
    }
  }
  kr_slice_unref (slice);
  
  return 0;
}

void muxer_unit_destroy (void *arg) {

  krad_link_t *krad_link = (krad_link_t *)arg;

  krad_container_destroy (&krad_link->krad_container);

  if (krad_link->muxer_krad_transmission != NULL) {
    krad_transmitter_transmission_destroy (krad_link->muxer_krad_transmission);
  }

  printk ("Muxer Destroyed");
}

void krad_link_audio_samples_callback (int frames, void *userdata, float **samples) {

  krad_link_t *link = (krad_link_t *)userdata;
  
  int c;
  int wrote;

  if (link->type == ENCODE) {
    for (c = 0; c < link->channels; c++ ) {
      if (krad_ringbuffer_write_space (link->audio_input_ringbuffer[c]) < frames * 4) {
        return;
      }
    }

    for (c = 0; c < link->channels; c++ ) {
      krad_ringbuffer_write (link->audio_input_ringbuffer[c], (char *)samples[c], frames * 4);
    }

    wrote = write (link->socketpair[0], "0", 1);
    if (wrote != 1) {
      printk ("Krad Transponder: au port write unexpected write return value %d", wrote);
    }
  }
}

krad_tags_t *krad_link_get_tags (krad_link_t *krad_link) {
  return krad_link->krad_tags;
}

void krad_link_destroy (krad_link_t *krad_link) {

  printk ("Link shutting down");

  kr_xpdr_subunit_remove (krad_link->krad_transponder->xpdr, krad_link->graph_id);
  
  if (krad_link->krad_framepool != NULL) {
    krad_framepool_destroy (&krad_link->krad_framepool);
  }

  krad_tags_destroy (krad_link->krad_tags);  
  
  printk ("Krad Link Closed Clean");
  
  free (krad_link);
}

krad_link_t *krad_link_prepare (int linknum) {

  krad_link_t *krad_link;
  
  krad_link = calloc (1, sizeof(krad_link_t));

  krad_link->capture_buffer_frames = DEFAULT_CAPTURE_BUFFER_FRAMES;
  krad_link->vp8_bitrate = DEFAULT_VPX_BITRATE;
  strncpy (krad_link->device, DEFAULT_V4L2_DEVICE, sizeof(krad_link->device));
  krad_link->vorbis_quality = DEFAULT_VORBIS_QUALITY;
  krad_link->flac_bit_depth = KRAD_DEFAULT_FLAC_BIT_DEPTH;
  krad_link->opus_bitrate = KRAD_DEFAULT_OPUS_BITRATE;
  krad_link->theora_quality = DEFAULT_THEORA_QUALITY;
  krad_link->video_source = NOVIDEO;
  krad_link->transport_mode = TCP;
  krad_link->channels = 2;
  sprintf (krad_link->sysname, "link%d", linknum);
  krad_link->krad_tags = krad_tags_create (krad_link->sysname);

  return krad_link;
}

void krad_link_start (krad_link_t *link) {

  kr_xpdr_su_spec_t spec;
  
  memset (&spec, 0, sizeof(kr_xpdr_su_spec_t));
  spec.fd = -1;
  spec.ptr = link;
  
  if ((link->encoding_fps_numerator == 0) || (link->encoding_fps_denominator == 0)) {
    krad_compositor_get_frame_rate (link->krad_radio->krad_compositor,
                                    &link->encoding_fps_numerator,
                                    &link->encoding_fps_denominator);
  }

  if ((link->encoding_width == 0) || (link->encoding_height == 0)) {
    krad_compositor_get_resolution (link->krad_radio->krad_compositor,
                                    &link->encoding_width,
                                    &link->encoding_height);
  }
  
  krad_compositor_get_resolution (link->krad_radio->krad_compositor,
                                  &link->composite_width,
                                  &link->composite_height);

  switch ( link->type ) {
    case ENCODE:
      if (link->av_mode == AUDIO_ONLY) {
        audio_encoding_unit_create (link);
        spec.fd = link->socketpair[1];
        spec.readable_callback = audio_encoding_unit_process;
        spec.encoder_header_callback = audio_encoding_unit_get_header;
        spec.destroy_callback = audio_encoding_unit_destroy;
      } else {
        video_encoding_unit_create (link);
        spec.fd = link->krad_compositor_port_fd;
        spec.encoder_header_callback = video_encoding_unit_get_header;
        spec.readable_callback = video_encoding_unit_process;
        spec.destroy_callback = video_encoding_unit_destroy;
      }
      link->graph_id = kr_xpdr_add_encoder (link->krad_transponder->xpdr, &spec);
      break;  
    case MUX:
      muxer_unit_create (link);
      spec.readable_callback = muxer_unit_process;
      spec.destroy_callback = muxer_unit_destroy;
      link->graph_id = kr_xpdr_add_muxer (link->krad_transponder->xpdr, &spec);
      break;
    case RAWOUT:
#ifdef KRAD_USE_WAYLAND
      wayland_display_unit_create (link);
      spec.fd = kr_wayland_get_fd(link->wayland);
      spec.readable_callback = wayland_display_unit_process;
      spec.destroy_callback = wayland_display_unit_destroy;
      link->graph_id = kr_xpdr_add_raw (link->krad_transponder->xpdr, &spec);
#endif
      break;
    case RAWIN:
      switch ( link->video_source ) {
        case NOVIDEO:
          return;
        case X11:
#ifdef KR_LINUX
          x11_capture_unit_create (link);
          spec.idle_callback_interval = 5;
          spec.readable_callback = x11_capture_unit_process;
          spec.destroy_callback = x11_capture_unit_destroy;
          break;
#endif
        case V4L2:
#ifdef KR_LINUX
          v4l2_capture_unit_create (link);
          spec.fd = link->krad_v4l2->fd;
          spec.readable_callback = v4l2_capture_unit_process;
          spec.destroy_callback = v4l2_capture_unit_destroy;
          break; 
#endif
        case DECKLINK:
          decklink_capture_unit_create (link);
          spec.readable_callback = NULL;
          spec.destroy_callback = decklink_capture_unit_destroy;
          break;
#ifdef KRAD_USE_FLYCAP
        case FLYCAP:
          flycap_capture_unit_create (link);
          spec.idle_callback_interval = 2;
          spec.readable_callback = flycap_capture_unit_process;
          spec.destroy_callback = flycap_capture_unit_destroy;
          break;
#endif
      }
      link->graph_id = kr_xpdr_add_raw (link->krad_transponder->xpdr, &spec);
      break;
    default:
      return;
  }
  
  link->subunit = kr_xpdr_get_subunit (link->krad_transponder->xpdr, link->graph_id);
  
  if (link->type == MUX) {
    connect_muxer_to_encoders (link);
  }
}

krad_link_t *krad_transponder_get_link_from_sysname (krad_transponder_t *krad_transponder, char *sysname) {

  int i;
  krad_link_t *krad_link;

  for (i = 0; i < KRAD_TRANSPONDER_MAX_SUBUNITS; i++) {
    krad_link = krad_transponder->krad_link[i];
    if (krad_link != NULL) {
      if (strcmp(sysname, krad_link->sysname) == 0) {
        return krad_link;
      }
    }
  }

  return NULL;
}

krad_tags_t *krad_transponder_get_tags_for_link (krad_transponder_t *krad_transponder, char *sysname) {

  krad_link_t *krad_link;
  
  krad_link = krad_transponder_get_link_from_sysname (krad_transponder, sysname);

  if (krad_link != NULL) {
    return krad_link_get_tags (krad_link);
  } else {
    return NULL;
  }
}

krad_transponder_t *krad_transponder_create (krad_radio_t *krad_radio) {

  krad_transponder_t *krad_transponder;
  
  krad_transponder = calloc (1, sizeof(krad_transponder_t));

  krad_transponder->address.path.unit = KR_TRANSPONDER;
  krad_transponder->address.path.subunit.mixer_subunit = KR_UNIT;

  krad_transponder->krad_radio = krad_radio;
  krad_transponder->krad_transmitter = krad_transmitter_create ();
  krad_transponder->xpdr = krad_xpdr_create (krad_transponder->krad_radio);

  return krad_transponder;
}

void krad_transponder_destroy (krad_transponder_t *krad_transponder) {

  int l;
  
  printk ("Krad Transponder: Destroy Started");

  for (l = 0; l < KRAD_TRANSPONDER_MAX_SUBUNITS; l++) {
    if (krad_transponder->krad_link[l] != NULL) {
      krad_link_destroy (krad_transponder->krad_link[l]);
      krad_transponder->krad_link[l] = NULL;
    }
  }

  krad_transmitter_destroy (krad_transponder->krad_transmitter);
  krad_xpdr_destroy (&krad_transponder->xpdr);

  free (krad_transponder);
  
  printk ("Krad Transponder: Destroy Completed");
}
