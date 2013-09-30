#include <cairo.h>
#include "kr_client.h"
#include <ccv.h>

typedef struct kr_snapshot {
  uint32_t width;
  uint32_t height;
  uint8_t *rgba;
	kr_videoport_t *videoport_in;
	kr_client_t *client_in;
  ccv_comp_t dbox;
  ccv_rect_t bbox;
  ccv_tld_t *tld;
  int got_frame;
} kr_snapshot;

static int destroy = 0;

void signal_recv (int sig) {
  destroy = 1;
}

void render_smiley (cairo_t *cr, int x_in, int y_in, int width, int height) {
  double pi = 3.14f;
  double w = 1.5 * (double) width;
  double h = 1.5 * (double) height;

  double x = x_in -  0.18*w;
  double y = y_in -  0.18*h;


  cairo_save(cr);
  cairo_set_line_width(cr, 6);

  cairo_set_source_rgb(cr, 0.2, 0.4, 0.8 );

  //eyes
  cairo_move_to(cr, x + w/3.f, y + h/3.f );
  cairo_rel_line_to(cr, 0.f, h/6.f );
  cairo_move_to(cr, x + 2*(w/3.f), y + h/3.f );
  cairo_rel_line_to(cr, 0.f, h/6.f );
  cairo_stroke(cr);

  cairo_set_source_rgb(cr, 1.f, 1.f, 0.f );

  double rad = (w > h) ? h : w;
  //face
  cairo_arc(cr, x + w/2.f, y + h/2.f, (rad/2.f) - 20.f,0.f,2.f * pi );
  cairo_stroke(cr);
  //smile
  cairo_arc(cr, x + w/2.f, y + h/2.f, (rad/3.f) -10.f, pi/3.f, 2.f * (pi/3.f));
  cairo_stroke(cr);
  cairo_restore(cr);

}

int videoport_process_out (void *buffer, void *user) {

  cairo_surface_t *surface;
	cairo_t *cr;

  kr_snapshot *snapshot;
  snapshot = (kr_snapshot *)user;
  //while (snapshot->got_frame != 1) { usleep(500); }

    memcpy(buffer, snapshot->rgba, snapshot->width
     * snapshot->height * 4);

     // snapshot->got_frame = 0;
    //if (snapshot->dbox.rect != 0) {
      surface = cairo_image_surface_create_for_data(buffer,
       CAIRO_FORMAT_ARGB32, snapshot->width, snapshot->height,
       snapshot->width * 4);
     	cr = cairo_create (surface);
      render_smiley(cr, snapshot->dbox.rect.x, snapshot->dbox.rect.y,
       snapshot->dbox.rect.width, snapshot->dbox.rect.height);

      cairo_surface_flush (surface);
      cairo_surface_destroy(surface);
      cairo_destroy (cr);
   // }
  //}

	return 0;
}

int videoport_process_in (void *buffer, void *user) {

  kr_snapshot *snapshot;
  static int first_call = 1;
  static ccv_dense_matrix_t *x = 0;
  static ccv_dense_matrix_t *y = 0;
  snapshot = (kr_snapshot *)user;

  //if (snapshot->got_frame == 0) {
    memcpy(snapshot->rgba, buffer, snapshot->width *
     snapshot->height * 4);

    if (first_call == 1) {
      first_call = 0;
      ccv_read(snapshot->rgba, &x, CCV_IO_ARGB_RAW
       | CCV_IO_GRAY, snapshot->height, snapshot->width, snapshot->width * 4);
      snapshot->tld = ccv_tld_new(x, snapshot->bbox, ccv_tld_default_params);
    } else {
      snapshot->got_frame = 1;
      ccv_read(snapshot->rgba, &y, CCV_IO_ARGB_RAW
       | CCV_IO_GRAY, snapshot->height, snapshot->width, snapshot->width * 4);
		  ccv_tld_info_t info;
      snapshot->dbox = ccv_tld_track_object(snapshot->tld, x, y, &info);
      x = y;
      y = 0;
    //}
  }

	return 0;
}


int main (int argc, char *argv[]) {

  int ret;
  int i;
	uint32_t width;
	uint32_t height;
	kr_client_t *client_in, *client_out;
	kr_videoport_t *videoport_in;
	kr_videoport_t *videoport_out;
  kr_snapshot *snapshot;
  ret = 0;

	if (argc != 7) {
    fprintf (stderr, "args: station_in station_out x, y, w, h.\n");
    return 1;
	}

	client_in = kr_client_create ("krad in videoport client");
	client_out = kr_client_create ("krad out videoport client");

	if (client_out == NULL) {
		fprintf (stderr, "Could not create output KR client.\n");
		return 1;
	}

	if (client_in == NULL) {
		fprintf (stderr, "Could not create input KR client.\n");
		return 1;
	}

  kr_connect(client_in, argv[1]);
  kr_connect(client_out, argv[2]);

  if (!kr_connected (client_in)) {
		fprintf (stderr, "Could not connect to %s krad radio daemon.\n", argv[1]);
	  kr_client_destroy (&client_in);
	  return 1;
  }
	  if (!kr_connected (client_out)) {
		fprintf (stderr, "Could not connect to %s krad radio daemon.\n", argv[2]);
	  kr_client_destroy (&client_out);
	  return 1;
  }

  if (kr_compositor_get_info_wait (client_in, &width, &height, NULL, NULL) != 1) {
    fprintf (stderr, "Could not get compositor info!\n");
	  kr_client_destroy (&client_in);
	  return 1;
  }

	videoport_in = kr_videoport_create (client_in, OUTPUT);
	videoport_out = kr_videoport_create (client_out, INPUT);

	if (videoport_in == NULL) {
		fprintf (stderr, "Could not make input videoport.\n");
	  kr_client_destroy (&client_in);
	  return 1;
	} else {
		printf ("Input Working!\n");
	}

	if (videoport_out == NULL) {
		fprintf (stderr, "Could not make output videoport.\n");
	  kr_client_destroy (&client_out);
	  return 1;
	} else {
		printf ("Output Working!\n");
	}

  snapshot = calloc(1, sizeof(kr_snapshot));
  snapshot->width = width;
  snapshot->height = height;
  snapshot->bbox = ccv_rect(atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));

  snapshot->rgba = malloc(snapshot->width * snapshot->height * 4);

	kr_videoport_set_callback (videoport_in, videoport_process_in, snapshot);
	kr_videoport_set_callback (videoport_out, videoport_process_out, snapshot);

  signal (SIGINT, signal_recv);
  signal (SIGTERM, signal_recv);

	kr_videoport_activate (videoport_in);
	kr_videoport_activate (videoport_out);
  ccv_enable_default_cache();

  while(1) {
    if (destroy == 1) {
		  printf ("Got signal!\n");
	    break;
	  }
    if (kr_videoport_error (videoport_in) || kr_videoport_error(videoport_out)) {
      printf ("Error: %s\n", "videoport Error");
      ret = 1;
      break;
    }
	}

	kr_videoport_deactivate (videoport_in);
	kr_videoport_deactivate (videoport_out);

	kr_videoport_destroy (videoport_in);
	kr_videoport_destroy (videoport_out);

	kr_client_destroy (&client_in);
	kr_client_destroy (&client_out);

	return ret;
}