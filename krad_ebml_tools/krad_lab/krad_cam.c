#include "krad_sdl_opengl_display.h"
#include "krad_ebml.h"
#include "krad_dirac.h"
#include "krad_vpx.h"
#include "krad_vorbis.h"
#include "krad_flac.h"
#include "krad_gui.h"
#include "krad_audio.h"
#include "krad_v4l2.h"
#include "krad_ring.h"

#define APPVERSION "Krad Cam 0.2"

int do_shutdown;

typedef struct krad_cam_St krad_cam_t;

struct krad_cam_St {

	kradgui_t *krad_gui;
	krad_vpx_encoder_t *krad_vpx_encoder;
	krad_audio_t *krad_audio;
	krad_audio_api_t audio_api;
	krad_vorbis_t *krad_vorbis;
	krad_flac_t *krad_flac;
	kradebml_t *krad_ebml;
	krad_v4l2_t *krad_v4l2;
	krad_sdl_opengl_display_t *krad_opengl_display;
	
	char device[512];
	char alsa_device[512];
	char output[512];
	
	char host[512];
	int port;
	char mount[512];
	char password[512];
	
	int bitrate;
	
	int capture_width;
	int capture_height;
	int capture_fps;
	int composite_width;
	int composite_height;
	int composite_fps;
	int display_width;
	int display_height;
	int encoding_width;
	int encoding_height;
	int encoding_fps;

	int mjpeg_mode;

	int capture_buffer_frames;
	int encoding_buffer_frames;

	unsigned char *current_encoding_frame;
	unsigned char *current_frame;
	
	unsigned int captured_frames;
	
	int encoding;
	int capturing;
	
	int capture_audio;
	krad_ringbuffer_t *audio_input_ringbuffer[2];
	float *samples[2];
	int audio_encoder_ready;
	int audio_frames_captured;
	int audio_frames_encoded;
	
	krad_ringbuffer_t *encoded_audio_ringbuffer;
	krad_ringbuffer_t *encoded_video_ringbuffer;
	
	int video_frames_encoded;
	
	int composited_frame_byte_size;
	krad_ringbuffer_t *captured_frames_buffer;
	krad_ringbuffer_t *composited_frames_buffer;
	
	struct SwsContext *captured_frame_converter;
	struct SwsContext *encoding_frame_converter;
	struct SwsContext *display_frame_converter;
	
	int video_track;
	int audio_track;
	
	krad_audio_api_t krad_audio_api;
	
	pthread_t video_capture_thread;
	pthread_t video_encoding_thread;
	pthread_t audio_encoding_thread;
	pthread_t ebml_output_thread;



	int render_meters;
	int display;


	int new_capture_frame;
	int cam_started;

	float temp_peak;
	float kick;

	SDL_Event event;

	char bug[512];
	int bug_x;
	int bug_y;
	
	int debug;
	int *shutdown;
	int daemon;

};

void *video_capture_thread(void *arg) {

	krad_cam_t *krad_cam = (krad_cam_t *)arg;

	int dupe_frame = 0;
	int first = 1;
	struct timespec start_time;
	struct timespec current_time;
	struct timespec elapsed_time;
	void *captured_frame = NULL;
	unsigned char *captured_frame_rgb = malloc(krad_cam->composited_frame_byte_size); 
	
	//printf("\n\nvideo capture thread begins\n\n");

	krad_cam->krad_v4l2 = kradv4l2_create();

	kradv4l2_open(krad_cam->krad_v4l2, krad_cam->device, krad_cam->capture_width, krad_cam->capture_height, krad_cam->capture_fps);
	
	kradv4l2_start_capturing (krad_cam->krad_v4l2);

	while (krad_cam->capturing == 1) {

		captured_frame = kradv4l2_read_frame_wait_adv (krad_cam->krad_v4l2);
	
		
		if (first == 1) {
			first = 0;
			krad_cam->captured_frames = 0;
			krad_cam->capture_audio = 1;
			clock_gettime(CLOCK_MONOTONIC, &start_time);
		} else {
		
		
			if ((krad_cam->debug) && (krad_cam->captured_frames % 300 == 0)) {
		
				clock_gettime(CLOCK_MONOTONIC, &current_time);
				elapsed_time = timespec_diff(start_time, current_time);
				printf("Frames Captured: %u Expected: %ld - %u fps * %ld seconds\n", krad_cam->captured_frames, elapsed_time.tv_sec * krad_cam->capture_fps, krad_cam->capture_fps, elapsed_time.tv_sec);
	
			}
			
			krad_cam->captured_frames++;
	
		}
	
		krad_cam->mjpeg_mode = 1;
	
		if (krad_cam->mjpeg_mode == 0) {
	
			int rgb_stride_arr[3] = {4*krad_cam->composite_width, 0, 0};

			const uint8_t *yuv_arr[4];
			int yuv_stride_arr[4];
		
			yuv_arr[0] = captured_frame;

			yuv_stride_arr[0] = krad_cam->capture_width + (krad_cam->capture_width/2) * 2;
			yuv_stride_arr[1] = 0;
			yuv_stride_arr[2] = 0;
			yuv_stride_arr[3] = 0;

			unsigned char *dst[4];

			dst[0] = captured_frame_rgb;

			sws_scale(krad_cam->captured_frame_converter, yuv_arr, yuv_stride_arr, 0, krad_cam->capture_height, dst, rgb_stride_arr);
	
		} else {
		
			kradv4l2_jpeg_to_rgb (krad_cam->krad_v4l2, captured_frame_rgb, captured_frame, krad_cam->krad_v4l2->jpeg_size);
		
		}
	
		kradv4l2_frame_done (krad_cam->krad_v4l2);

	
		if (krad_ringbuffer_write_space(krad_cam->captured_frames_buffer) >= krad_cam->composited_frame_byte_size) {
	
			krad_ringbuffer_write(krad_cam->captured_frames_buffer, 
								 (char *)captured_frame_rgb, 
								 krad_cam->composited_frame_byte_size );
	
		}
		
		if (dupe_frame == 1) {

			//printf("\n\nduping a frame *********** bad should not happen\n\n");

			if (krad_ringbuffer_write_space(krad_cam->captured_frames_buffer) >= krad_cam->composited_frame_byte_size) {

				krad_ringbuffer_write(krad_cam->captured_frames_buffer, 
									 (char *)captured_frame_rgb, 
									 krad_cam->composited_frame_byte_size );

			}
			

			dupe_frame = 0;
		}

	
	}

	kradv4l2_stop_capturing (krad_cam->krad_v4l2);
	kradv4l2_close(krad_cam->krad_v4l2);
	kradv4l2_destroy(krad_cam->krad_v4l2);

	free(captured_frame_rgb);

	krad_cam->capture_audio = 2;
	krad_cam->encoding = 2;
	//printf("\n\nvideo capture thread ends\n\n");

	return NULL;
	
}

void *video_encoding_thread(void *arg) {

	krad_cam_t *krad_cam = (krad_cam_t *)arg;
	
	//printf("\n\nvideo encoding thread begins\n\n");
	
	void *vpx_packet;
	int keyframe;
	int packet_size;
	char keyframe_char[1];

	int first = 1;

	krad_cam->krad_vpx_encoder = krad_vpx_encoder_create(krad_cam->encoding_width, krad_cam->encoding_height, krad_cam->bitrate);


	krad_cam->krad_vpx_encoder->quality = 1000 * ((krad_cam->encoding_fps / 4) * 3);
	//printf("\n\n encoding quality set to %ld\n\n", krad_cam->krad_vpx_encoder->quality);

	while ((krad_cam->encoding == 1) || (krad_ringbuffer_read_space(krad_cam->composited_frames_buffer) >= krad_cam->composited_frame_byte_size)) {

		if (krad_ringbuffer_read_space(krad_cam->composited_frames_buffer) >= krad_cam->composited_frame_byte_size) {
	
			krad_ringbuffer_read(krad_cam->composited_frames_buffer, 
								 (char *)krad_cam->current_encoding_frame, 
								 krad_cam->composited_frame_byte_size );
								 
						
						
			if (krad_ringbuffer_read_space(krad_cam->composited_frames_buffer) >= (krad_cam->composited_frame_byte_size * (krad_cam->encoding_buffer_frames / 2)) ) {
			
				krad_cam->krad_vpx_encoder->quality = (1000 * ((krad_cam->encoding_fps / 4) * 3)) / 2LU;
				
				//printf("\n\n encoding quality set to %ld\n\n", krad_cam->krad_vpx_encoder->quality);
			
			}
						
			if (krad_ringbuffer_read_space(krad_cam->composited_frames_buffer) >= (krad_cam->composited_frame_byte_size * (krad_cam->encoding_buffer_frames / 4)) ) {
			
				krad_cam->krad_vpx_encoder->quality = (1000 * ((krad_cam->encoding_fps / 4) * 3)) / 4LU;
			
				//printf("\n\n encoding quality set to %ld\n\n", krad_cam->krad_vpx_encoder->quality);
			
			}						 
								 
			int rgb_stride_arr[3] = {4*krad_cam->composite_width, 0, 0};
			const uint8_t *rgb_arr[4];
	
			rgb_arr[0] = krad_cam->current_encoding_frame;

			sws_scale(krad_cam->encoding_frame_converter, rgb_arr, rgb_stride_arr, 0, krad_cam->composite_height, 
					  krad_cam->krad_vpx_encoder->image->planes, krad_cam->krad_vpx_encoder->image->stride);
										 
			packet_size = krad_vpx_encoder_write(krad_cam->krad_vpx_encoder, (unsigned char **)&vpx_packet, &keyframe);
								 
			//printf("packet size was %d\n", packet_size);
	
			
	
			if (packet_size) {

				keyframe_char[0] = keyframe;

				krad_ringbuffer_write(krad_cam->encoded_video_ringbuffer, (char *)&packet_size, 4);
				krad_ringbuffer_write(krad_cam->encoded_video_ringbuffer, keyframe_char, 1);
				krad_ringbuffer_write(krad_cam->encoded_video_ringbuffer, (char *)vpx_packet, packet_size);

				if (first == 1) {				
					first = 0;
				}
				
				krad_cam->video_frames_encoded++;
				
			}
	
		} else {
		
			if (krad_cam->krad_vpx_encoder->quality != (1000 * ((krad_cam->encoding_fps / 4) * 3))) {
				krad_cam->krad_vpx_encoder->quality = 1000 * ((krad_cam->encoding_fps / 4) * 3);
				//printf("\n\n encoding quality set to %ld\n\n", krad_cam->krad_vpx_encoder->quality);
			}

			
			usleep(4000);
		
		}
		
	}
	
	while (packet_size) {
		krad_vpx_encoder_finish(krad_cam->krad_vpx_encoder);
		packet_size = krad_vpx_encoder_write(krad_cam->krad_vpx_encoder, (unsigned char **)&vpx_packet, &keyframe);
	}

	krad_vpx_encoder_destroy(krad_cam->krad_vpx_encoder);
	
	krad_cam->encoding = 3;
	
	//printf("\n\nvideo encoding thread ends\n\n");
	
	return NULL;
	
}


void krad_cam_audio_callback(int frames, void *userdata) {

	krad_cam_t *krad_cam = (krad_cam_t *)userdata;
	
	kradaudio_read (krad_cam->krad_audio, 0, (char *)krad_cam->samples[0], frames * 4 );
	kradaudio_read (krad_cam->krad_audio, 1, (char *)krad_cam->samples[1], frames * 4 );

	if (krad_cam->capture_audio == 1) {
		krad_cam->audio_frames_captured += frames;
		krad_ringbuffer_write(krad_cam->audio_input_ringbuffer[0], (char *)krad_cam->samples[0], frames * 4);
		krad_ringbuffer_write(krad_cam->audio_input_ringbuffer[1], (char *)krad_cam->samples[1], frames * 4);
	}
	
	if (krad_cam->capture_audio == 2) {
		krad_cam->capture_audio = 3;
	}
	
	
}

void *audio_encoding_thread(void *arg) {

	krad_cam_t *krad_cam = (krad_cam_t *)arg;
	
	//printf("\n\naudio encoding thread begins\n\n");
	
	char *audioname;
	
	if (krad_cam->krad_audio_api == ALSA) { 
		audioname = krad_cam->alsa_device;
	} else {
		audioname = "Krad Cam";
	}
	
	krad_cam->krad_audio = kradaudio_create(audioname, KINPUT, krad_cam->krad_audio_api);
	
	krad_cam->krad_vorbis = krad_vorbis_encoder_create(2, krad_cam->krad_audio->sample_rate, 0.7);
	//krad_cam->krad_flac = krad_flac_encoder_create(2, krad_cam->kradaudio->sample_rate, 16);
	
	int framecnt = 1024;
	int frames;
	float *audio = calloc(1, 1000000);
	unsigned char *buffer = calloc(1, 1000000);
	ogg_packet *op;
	int altframecount = 0;

	krad_cam->audio_input_ringbuffer[0] = krad_ringbuffer_create (2000000);
	krad_cam->audio_input_ringbuffer[1] = krad_ringbuffer_create (2000000);

	krad_cam->samples[0] = malloc(4 * 8192);
	krad_cam->samples[1] = malloc(4 * 8192);

	kradaudio_set_process_callback(krad_cam->krad_audio, krad_cam_audio_callback, krad_cam);

	if (krad_cam->audio_api == JACK) {
		krad_jack_t *jack = (krad_jack_t *)krad_cam->krad_audio->api;
		kradjack_connect_port(jack->jack_client, "firewire_pcm:001486af2e61ac6b_Unknown0_in", "Krad Cam:InputLeft");
		kradjack_connect_port(jack->jack_client, "firewire_pcm:001486af2e61ac6b_Unknown0_in", "Krad Cam:InputRight");
	}
	
	krad_cam->audio_encoder_ready = 1;
	
	while (krad_cam->encoding) {

		while ((krad_ringbuffer_read_space(krad_cam->audio_input_ringbuffer[1]) >= framecnt * 4) || (op != NULL)) {
			
			op = krad_vorbis_encode(krad_cam->krad_vorbis, framecnt, krad_cam->audio_input_ringbuffer[0], krad_cam->audio_input_ringbuffer[1]);

			if (op != NULL) {
			
				frames = op->granulepos - krad_cam->audio_frames_encoded;
				
				altframecount += frames;
				
				krad_ringbuffer_write(krad_cam->encoded_audio_ringbuffer, (char *)&op->bytes, 4);
				krad_ringbuffer_write(krad_cam->encoded_audio_ringbuffer, (char *)&frames, 4);
				krad_ringbuffer_write(krad_cam->encoded_audio_ringbuffer, (char *)op->packet, op->bytes);

				
				krad_cam->audio_frames_encoded = op->granulepos;
			
				//printf("frames encoded: %d frames captured: %d alt %d\r", krad_cam->audio_frames_encoded, krad_cam->audio_frames_captured, altframecount);
				//fflush(stdout);
			
			}
		}
	
		while (krad_ringbuffer_read_space(krad_cam->audio_input_ringbuffer[1]) < framecnt * 4) {
	
			usleep(10000);
	
			if (krad_cam->encoding == 3) {
				break;
			}
	
		}
		
		if ((krad_cam->encoding == 3) && (krad_ringbuffer_read_space(krad_cam->audio_input_ringbuffer[1]) < framecnt * 4)) {
				break;
		}
		
	}
	
	krad_cam->encoding = 4;
	
	while (krad_cam->capture_audio != 3) {
		usleep(5000);
	}
	
	free(krad_cam->samples[0]);
	free(krad_cam->samples[1]);

	krad_ringbuffer_free ( krad_cam->audio_input_ringbuffer[0] );
	krad_ringbuffer_free ( krad_cam->audio_input_ringbuffer[1] );
	
	free(audio);
	free(buffer);
	
	//printf("\n\naudio encoding thread ends\n\n");
	
	return NULL;
	
}

void *ebml_output_thread(void *arg) {

	krad_cam_t *krad_cam = (krad_cam_t *)arg;


	unsigned char *packet;
	int packet_size;
	int keyframe;
	int frames;
	char keyframe_char[1];
	
	int video_frames_muxed;
	int audio_frames_muxed;
	int audio_frames_per_video_frame;

	//printf("\n\nebml muxing thread begins\n\n");

	audio_frames_muxed = 0;
	video_frames_muxed = 0;

	while (krad_cam->krad_audio == NULL) {
		usleep(5000);
	}

	audio_frames_per_video_frame = krad_cam->krad_audio->sample_rate / krad_cam->capture_fps;

	packet = malloc(2000000);


	krad_cam->krad_ebml = kradebml_create();
	
	if (krad_cam->host[0] != '\0') {
		kradebml_open_output_stream(krad_cam->krad_ebml, krad_cam->host, krad_cam->port, krad_cam->mount, krad_cam->password);
	} else {
		printf("Outputing to file: %s\n", krad_cam->output);
		kradebml_open_output_file(krad_cam->krad_ebml, krad_cam->output);
	}

	kradebml_header(krad_cam->krad_ebml, "webm", APPVERSION);
	
	krad_cam->video_track = kradebml_add_video_track(krad_cam->krad_ebml, "V_VP8", krad_cam->encoding_fps,
										 			 krad_cam->encoding_width, krad_cam->encoding_height);
	
	
	while ( krad_cam->audio_encoder_ready != 1) {
	
		usleep(10000);
	
	}
	
	krad_cam->audio_track = kradebml_add_audio_track(krad_cam->krad_ebml, "A_VORBIS", krad_cam->krad_audio->sample_rate, 2, krad_cam->krad_vorbis->header, 
													 krad_cam->krad_vorbis->headerpos);
	
	kradebml_write(krad_cam->krad_ebml);
	
	//printf("\n\nebml muxing thread waiting..\n\n");
	
	while ( krad_cam->encoding ) {

		if ((krad_ringbuffer_read_space(krad_cam->encoded_video_ringbuffer) >= 4) && (krad_cam->encoding < 3)) {

			krad_ringbuffer_read(krad_cam->encoded_video_ringbuffer, (char *)&packet_size, 4);
		
			while ((krad_ringbuffer_read_space(krad_cam->encoded_video_ringbuffer) < packet_size + 1) && (krad_cam->encoding < 3)) {
				usleep(10000);
			}
			
			if ((krad_ringbuffer_read_space(krad_cam->encoded_video_ringbuffer) < packet_size + 1) && (krad_cam->encoding > 2)) {
				continue;
			}
			
			krad_ringbuffer_read(krad_cam->encoded_video_ringbuffer, keyframe_char, 1);
			krad_ringbuffer_read(krad_cam->encoded_video_ringbuffer, (char *)packet, packet_size);

			keyframe = keyframe_char[0];
	

			kradebml_add_video(krad_cam->krad_ebml, krad_cam->video_track, packet, packet_size, keyframe);

			video_frames_muxed++;
		
		}
		
		if ((krad_ringbuffer_read_space(krad_cam->encoded_audio_ringbuffer) >= 4) && 
			((video_frames_muxed * audio_frames_per_video_frame) > audio_frames_muxed)) {

			krad_ringbuffer_read(krad_cam->encoded_audio_ringbuffer, (char *)&packet_size, 4);
		
			while ((krad_ringbuffer_read_space(krad_cam->encoded_audio_ringbuffer) < packet_size + 4) && (krad_cam->encoding != 4)) {
				usleep(10000);
			}
			
			if ((krad_ringbuffer_read_space(krad_cam->encoded_audio_ringbuffer) < packet_size + 4) && (krad_cam->encoding == 4)) {
				break;
			}
			
			krad_ringbuffer_read(krad_cam->encoded_audio_ringbuffer, (char *)&frames, 4);
			krad_ringbuffer_read(krad_cam->encoded_audio_ringbuffer, (char *)packet, packet_size);
	

			kradebml_add_audio(krad_cam->krad_ebml, krad_cam->audio_track, packet, packet_size, frames);

			audio_frames_muxed += frames;
		
		} else {
			if (krad_cam->encoding == 4) {
				break;
			}
		}
		
		
		if (((krad_ringbuffer_read_space(krad_cam->encoded_audio_ringbuffer) < 4) || 
			((video_frames_muxed * audio_frames_per_video_frame) > audio_frames_muxed)) && 
		   (krad_ringbuffer_read_space(krad_cam->encoded_video_ringbuffer) < 4)) {
		
			if (krad_cam->encoding == 4) {
				break;
			}
		
			usleep(10000);
			
		}		
	
	}

	kradebml_destroy(krad_cam->krad_ebml);
	
	free(packet);
	
	//printf("\n\nebml muxing thread ends\n\n");
	
	return NULL;
	
}

void krad_cam_handle_input(krad_cam_t *krad_cam) {

	while ( SDL_PollEvent( &krad_cam->event ) ){
		switch( krad_cam->event.type ){
			/* Look for a keypress */
			case SDL_KEYDOWN:
				/* Check the SDLKey values and move change the coords */
				switch( krad_cam->event.key.keysym.sym ){
					case SDLK_LEFT:
						break;
					case SDLK_RIGHT:
						break;
					case SDLK_UP:
						break;
					case SDLK_z:
						kradgui_remove_bug (krad_cam->krad_gui);
						break;
					/*			        
					case SDLK_m:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/DinoHead-r2_small.png", 30, 30);
						break;
					case SDLK_x:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/fish_xiph_org.png", 30, 30);
						break;
					case SDLK_h:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/html_5logo.png", 30, 30);
						break;
					case SDLK_w:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/WebM-logo1.png", 30, 30);
						break;
					case SDLK_e:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/airmoz3.png", 30, 30);
						break;
					case SDLK_r:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/airmoz4.png", 30, 30);
						break;					        
					case SDLK_s:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/airmoz5.png", 30, 30);
						break;
					case SDLK_a:
						kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/airmoz6.png", 30, 30);
						break;
					*/
					case SDLK_b:
						if (krad_cam->bug[0] != '\0') {
							kradgui_set_bug (krad_cam->krad_gui, krad_cam->bug, krad_cam->bug_x, krad_cam->bug_y);
						}
						break;
					case SDLK_l:
						if (krad_cam->krad_gui->live == 1) {
							krad_cam->krad_gui->live = 0;
						} else {
							krad_cam->krad_gui->live = 1;
						}
						break;
					case SDLK_0:
						krad_cam->krad_gui->bug_alpha += 0.1f;
						if (krad_cam->krad_gui->bug_alpha > 1.0f) {
							krad_cam->krad_gui->bug_alpha = 1.0f;
						}
						break;
					case SDLK_9:
						krad_cam->krad_gui->bug_alpha -= 0.1f;
						if (krad_cam->krad_gui->bug_alpha < 0.1f) {
							krad_cam->krad_gui->bug_alpha = 0.1f;
						}
						break;
					case SDLK_q:
						*krad_cam->shutdown = 1;
						break;
					case SDLK_f:
						if (krad_cam->krad_gui->render_ftest == 1) {
							krad_cam->krad_gui->render_ftest = 0;
						} else {
							krad_cam->krad_gui->render_ftest = 1;
						}
						break;
					case SDLK_v:
						if (krad_cam->render_meters == 1) {
							krad_cam->render_meters = 0;
						} else {
							krad_cam->render_meters = 1;
						}
						break;
					default:
						break;
				}
				break;

			case SDL_KEYUP:
				switch( krad_cam->event.key.keysym.sym ) {
					case SDLK_LEFT:
						break;
					case SDLK_RIGHT:
						break;
					case SDLK_UP:
						break;
					case SDLK_DOWN:
						break;
					default:
						break;
				}
				break;

			case SDL_MOUSEMOTION:
				//printf("mouse!\n");
				krad_cam->krad_gui->cursor_x = krad_cam->event.motion.x;
				krad_cam->krad_gui->cursor_y = krad_cam->event.motion.y;
				break;
	
			case SDL_QUIT:
				*krad_cam->shutdown = 1;	 			
				break;

			default:
				break;
		}
	}

}

void krad_cam_display(krad_cam_t *krad_cam) {

	if ((krad_cam->composite_width == krad_cam->display_width) && (krad_cam->composite_height == krad_cam->display_height)) {

		memcpy( krad_cam->krad_opengl_display->rgb_frame_data, krad_cam->krad_gui->data, krad_cam->krad_gui->bytes );

	} else {


		int rgb_stride_arr[3] = {4*krad_cam->display_width, 0, 0};

		const uint8_t *rgb_arr[4];
		int rgb1_stride_arr[4];

		rgb_arr[0] = krad_cam->krad_gui->data;

		rgb1_stride_arr[0] = krad_cam->composite_width * 4;
		rgb1_stride_arr[1] = 0;
		rgb1_stride_arr[2] = 0;
		rgb1_stride_arr[3] = 0;

		unsigned char *dst[4];

		dst[0] = krad_cam->krad_opengl_display->rgb_frame_data;

		sws_scale(krad_cam->display_frame_converter, rgb_arr, rgb1_stride_arr, 0, krad_cam->display_height, dst, rgb_stride_arr);


	}

	krad_sdl_opengl_draw_screen( krad_cam->krad_opengl_display );

	//if (read_composited) {
	//	krad_sdl_opengl_read_screen( krad_cam->krad_opengl_display, krad_cam->krad_opengl_display->rgb_frame_data);
	//}

}

void krad_cam_composite(krad_cam_t *krad_cam) {

	int c;

	if (krad_ringbuffer_read_space(krad_cam->captured_frames_buffer) >= krad_cam->composited_frame_byte_size) {
	
		krad_ringbuffer_read(krad_cam->captured_frames_buffer, 
							 (char *)krad_cam->current_frame, 
							 krad_cam->composited_frame_byte_size );

		if (krad_cam->cam_started == 0) {
			krad_cam->cam_started = 1;
			//krad_cam->krad_gui->render_ftest = 1;
			kradgui_go_live(krad_cam->krad_gui);
		}
		
		krad_cam->new_capture_frame = 1;
	} else {
		krad_cam->new_capture_frame = 0;
	}		
	
	memcpy( krad_cam->krad_gui->data, krad_cam->current_frame, krad_cam->krad_gui->bytes );
	
	kradgui_render( krad_cam->krad_gui );

	if (krad_cam->new_capture_frame == 1) {
		if (krad_cam->krad_audio != NULL) {
			for (c = 0; c < 2; c++) {
				krad_cam->temp_peak = read_peak(krad_cam->krad_audio, KINPUT, c);
				if (krad_cam->temp_peak >= krad_cam->krad_gui->output_peak[c]) {
					if (krad_cam->temp_peak > 2.7f) {
						krad_cam->krad_gui->output_peak[c] = krad_cam->temp_peak;
						krad_cam->kick = ((krad_cam->krad_gui->output_peak[c] - krad_cam->krad_gui->output_current[c]) / 300.0);
					}
				} else {
					if (krad_cam->krad_gui->output_peak[c] == krad_cam->krad_gui->output_current[c]) {
						krad_cam->krad_gui->output_peak[c] -= (0.9 * (60/krad_cam->capture_fps));
						if (krad_cam->krad_gui->output_peak[c] < 0.0f) {
							krad_cam->krad_gui->output_peak[c] = 0.0f;
						}
						krad_cam->krad_gui->output_current[c] = krad_cam->krad_gui->output_peak[c];
					}
				}
			
				if (krad_cam->krad_gui->output_peak[c] > krad_cam->krad_gui->output_current[c]) {
					krad_cam->krad_gui->output_current[c] = (krad_cam->krad_gui->output_current[c] + 1.4) * (1.3 + krad_cam->kick); ;
				}
		
				if (krad_cam->krad_gui->output_peak[c] < krad_cam->krad_gui->output_current[c]) {
					krad_cam->krad_gui->output_current[c] = krad_cam->krad_gui->output_peak[c];
				}
			
			
			}
		}
	}
	
	if (krad_cam->render_meters) {
		kradgui_render_meter (krad_cam->krad_gui, 110, krad_cam->composite_height - 30, 96, krad_cam->krad_gui->output_current[0]);
		kradgui_render_meter (krad_cam->krad_gui, krad_cam->composite_width - 110, krad_cam->composite_height - 30, 96, krad_cam->krad_gui->output_current[1]);
	}
	
	
	if ((krad_cam->cam_started == 1) && (krad_cam->new_capture_frame == 1) && (krad_ringbuffer_write_space(krad_cam->composited_frames_buffer) >= krad_cam->composited_frame_byte_size)) {

			krad_ringbuffer_write(krad_cam->composited_frames_buffer, 
								  (char *)krad_cam->krad_gui->data, 
								  krad_cam->composited_frame_byte_size );

	} else {
	
		if ((krad_cam->cam_started == 1) && (krad_cam->new_capture_frame == 1)) {
			printf("encoding to slow! overflow!\n");
		}
	
	}
		
		
}


void krad_cam_run(krad_cam_t *krad_cam) {


	while (!*krad_cam->shutdown) {

		if (!krad_cam->display) {
			while (krad_ringbuffer_read_space(krad_cam->captured_frames_buffer) < krad_cam->composited_frame_byte_size) {
				usleep(5000);
			}
		}

		krad_cam_composite(krad_cam);

		if (krad_cam->display) {
			krad_cam_display(krad_cam);
			krad_cam_handle_input(krad_cam);
		}
	}

}

void daemonize() {

		char *daemon_name = "krad_cam";

		/* Our process ID and Session ID */
		pid_t pid, sid;

        printf("Daemonizing..\n");
 
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
 
        /* Change the file mode mask */
        umask(0);
 
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
 
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }
 
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        //close(STDOUT_FILENO);
        //close(STDERR_FILENO);
        
        
		char err_log_file[98] = "";
		char log_file[98] = "";
		char *homedir = getenv ("HOME");
		
		char timestamp[64];
		
		time_t ltime;
    	ltime=time(NULL);
		sprintf(timestamp, "%s",asctime( localtime(&ltime) ) );
		
		int i;
		
		for (i = 0; i< strlen(timestamp); i++) {
			if (timestamp[i] == ' ') {
				timestamp[i] = '_';
			}
			if (timestamp[i] == ':') {
				timestamp[i] = '.';
			}
			if (timestamp[i] == '\n') {
				timestamp[i] = '\0';
			}
		
		}
		
		sprintf(err_log_file, "%s/%s_%s_err.log", homedir, daemon_name, timestamp);
		sprintf(log_file, "%s/%s_%s.log", homedir, daemon_name, timestamp);
		
		FILE *fp;
		
		fp = freopen( err_log_file, "w", stderr );
		if (fp == NULL) {
			printf("ruh oh error in freopen stderr\n");
		}
		fp = freopen( log_file, "w", stdout );
		if (fp == NULL) {
			printf("ruh oh error in freopen stdout\n");
		}
        
}

void krad_cam_shutdown() {

	if (do_shutdown == 1) {
		exit(1);
	}

	do_shutdown = 1;

}

void krad_cam_destroy(krad_cam_t *krad_cam) {

	krad_cam->capturing = 0;
	pthread_join(krad_cam->video_capture_thread, NULL);
	pthread_join(krad_cam->video_encoding_thread, NULL);
	pthread_join(krad_cam->audio_encoding_thread, NULL);
	pthread_join(krad_cam->ebml_output_thread, NULL);

	if (krad_cam->display) {
		krad_sdl_opengl_display_destroy(krad_cam->krad_opengl_display);
	}

	kradgui_destroy(krad_cam->krad_gui);

	sws_freeContext ( krad_cam->captured_frame_converter );
	sws_freeContext ( krad_cam->encoding_frame_converter );
	sws_freeContext ( krad_cam->display_frame_converter );

	// must be before vorbis
	kradaudio_destroy (krad_cam->krad_audio);
	krad_vorbis_encoder_destroy (krad_cam->krad_vorbis);

	krad_ringbuffer_free ( krad_cam->captured_frames_buffer );

	krad_ringbuffer_free ( krad_cam->encoded_audio_ringbuffer );
	krad_ringbuffer_free ( krad_cam->encoded_video_ringbuffer );

	free(krad_cam->current_encoding_frame);
	free(krad_cam->current_frame);

	free(krad_cam);
}


krad_cam_t *krad_cam_create(char *device, char *output, char *host, int port, char *mount, char *password, int bitrate, krad_audio_api_t audio_api, 
							int capture_width, int capture_height, int capture_fps, int composite_width, int composite_height, 
							int composite_fps, int display_width, int display_height, int encoding_width, int encoding_height, 
							int encoding_fps, int *shutdown, int daemon, char *bug, int bug_x, int bug_y, char *alsa_device) {

	krad_cam_t *krad_cam;
	
	krad_cam = calloc(1, sizeof(krad_cam_t));

	krad_cam->capture_width = capture_width;
	krad_cam->capture_height = capture_height;
	krad_cam->capture_fps = capture_fps;
	krad_cam->composite_width = composite_width;
	krad_cam->composite_height = composite_height;
	krad_cam->composite_fps = composite_fps;
	krad_cam->display_width = display_width;
	krad_cam->display_height = display_height;
	krad_cam->encoding_width = encoding_width;
	krad_cam->encoding_height = encoding_height;
	krad_cam->encoding_fps = encoding_fps;
	
	krad_cam->krad_audio_api = audio_api;
	
	krad_cam->capture_buffer_frames = 5;
	krad_cam->encoding_buffer_frames = 15;
	
	if (krad_cam->display_width != 0) {
		krad_cam->display = 1;		
	} else {
		krad_cam->display = 0;
	}
	
	krad_cam->shutdown = shutdown;
	
	signal(SIGINT, krad_cam_shutdown);
	signal(SIGTERM, krad_cam_shutdown);

	krad_cam->daemon = daemon;

	if (krad_cam->daemon) {
		daemonize();
	}
	
	krad_cam->bitrate = bitrate;
	krad_cam->port = port;
	
	memcpy(krad_cam->device, device, sizeof(krad_cam->device));
	memcpy(krad_cam->alsa_device, alsa_device, sizeof(krad_cam->alsa_device));
	memcpy(krad_cam->output, output, sizeof(krad_cam->output)); 
	memcpy(krad_cam->host, host, sizeof(krad_cam->host));
	memcpy(krad_cam->mount, mount, sizeof(krad_cam->mount));
	memcpy(krad_cam->password, password, sizeof(krad_cam->password)); 

	if (krad_cam->display) {

		krad_cam->krad_opengl_display = krad_sdl_opengl_display_create(APPVERSION, krad_cam->display_width, krad_cam->display_height, 
															 		   krad_cam->composite_width, krad_cam->composite_height);
	}
	
	krad_cam->composited_frame_byte_size = krad_cam->composite_width * krad_cam->composite_height * 4;
	krad_cam->current_frame = calloc(1, krad_cam->composited_frame_byte_size);
	krad_cam->current_encoding_frame = calloc(1, krad_cam->composited_frame_byte_size);

	krad_cam->captured_frame_converter = sws_getContext ( krad_cam->capture_width, krad_cam->capture_height, PIX_FMT_YUYV422, 
														  krad_cam->composite_width, krad_cam->composite_height, PIX_FMT_RGB32, 
														  SWS_BICUBIC, NULL, NULL, NULL);
														  
	krad_cam->encoding_frame_converter = sws_getContext ( krad_cam->composite_width, krad_cam->composite_height, PIX_FMT_RGB32, 
															krad_cam->encoding_width, krad_cam->encoding_height, PIX_FMT_YUV420P, 
															SWS_BICUBIC, NULL, NULL, NULL);
			
	if (krad_cam->display) {
															
		krad_cam->display_frame_converter = sws_getContext ( krad_cam->composite_width, krad_cam->composite_height, PIX_FMT_RGB32, 
															 krad_cam->display_width, krad_cam->display_height, PIX_FMT_RGB32, 
															 SWS_BICUBIC, NULL, NULL, NULL);
	}
		
	krad_cam->captured_frames_buffer = krad_ringbuffer_create (krad_cam->composited_frame_byte_size * krad_cam->capture_buffer_frames);
	krad_cam->composited_frames_buffer = krad_ringbuffer_create (krad_cam->composited_frame_byte_size * krad_cam->encoding_buffer_frames);

	krad_cam->encoded_audio_ringbuffer = krad_ringbuffer_create (2000000);
	krad_cam->encoded_video_ringbuffer = krad_ringbuffer_create (2000000);


	krad_cam->krad_gui = kradgui_create_with_internal_surface(krad_cam->composite_width, krad_cam->composite_height);

	if (bug[0] != '\0') {
		memcpy(krad_cam->bug, bug, sizeof(krad_cam->bug));
		krad_cam->bug_x = bug_x;
		krad_cam->bug_y = bug_y;
		kradgui_set_bug (krad_cam->krad_gui, krad_cam->bug, krad_cam->bug_x, krad_cam->bug_y);
	}

	//krad_cam->krad_gui->update_drawtime = 1;
	//krad_cam->krad_gui->print_drawtime = 1;
	
	//krad_cam->krad_gui->render_ftest = 1;
	//krad_cam->krad_gui->render_tearbar = 1;
	
//	kradgui_set_bug (krad_cam->krad_gui, "/home/oneman/Pictures/DinoHead-r2.png", 30, 30);
	
	krad_cam->krad_gui->clear = 0;
	//kradgui_test_screen(krad_cam->krad_gui, "Krad Cam Test");

	//printf("mem use approx %d MB\n", (krad_cam->composited_frame_byte_size * (krad_cam->capture_buffer_frames + krad_cam->encoding_buffer_frames + 4)) / 1000 / 1000);

	krad_cam->encoding = 1;
	krad_cam->capturing = 1;

	pthread_create(&krad_cam->video_capture_thread, NULL, video_capture_thread, (void *)krad_cam);
	pthread_create(&krad_cam->video_encoding_thread, NULL, video_encoding_thread, (void *)krad_cam);
	pthread_create(&krad_cam->audio_encoding_thread, NULL, audio_encoding_thread, (void *)krad_cam);
	pthread_create(&krad_cam->ebml_output_thread, NULL, ebml_output_thread, (void *)krad_cam);	

	return krad_cam;

}

int main (int argc, char *argv[]) {

	krad_cam_t *krad_cam;
	
	int capture_width, capture_height, capture_fps;
	int display_width, display_height;
	int composite_width, composite_height, composite_fps;
	int encoding_width, encoding_height, encoding_fps;
	char device[512];
	char alsa_device[512];
	krad_audio_api_t krad_audio_api;
	char output[512];
	char host[512];
	int port;
	char mount[512];
	char password[512];	
	int c;

	char bug[512];
	int bug_x;
	int bug_y;
	int bitrate;

	int display;
	int daemon;

	capture_width = 640;
	capture_height = 480;
	capture_fps = 15;
	bug_x = 30;
	bug_y = 30;
	bitrate = 1250;
	daemon = 0;
	display = 1;
	
	krad_audio_api = ALSA;	
	strncpy(device, DEFAULT_V4L2_DEVICE, sizeof(device));
	strncpy(alsa_device, DEFAULT_ALSA_CAPTURE_DEVICE, sizeof(alsa_device));
	sprintf(output, "%s/Videos/krad_cam_%zu.webm", getenv ("HOME"), time(NULL));
	
	memset(bug, '\0', sizeof(bug));
	memset(host, '\0', sizeof(host));
	memset(mount, '\0', sizeof(mount));
	memset(password, '\0', sizeof(password));
	port = 0;


	while ((c = getopt (argc, argv, "ajpf:w:h:o:b:x:y:d:m:v:l:n:g:zcq:")) != -1) {
		switch (c) {
			case 'd':
				strncpy(device, optarg, sizeof(device));
				break;
			case 'a':
				krad_audio_api = ALSA;
				break;
			case 'j':
				krad_audio_api = JACK;
				break;
			case 'p':
				krad_audio_api = PULSE;
				break;
			case 'f':
				capture_fps = atoi(optarg);
				break;
			case 'w':
				capture_width = atoi(optarg);
				break;
			case 'h':
				capture_height = atoi(optarg);
				break;
			case 'x':
				bug_x = atoi(optarg);
				break;
			case 'y':
				bug_y = atoi(optarg);
				break;
			case 'm':
				strncpy(mount, optarg, sizeof(mount));
				break;
			case 'v':
				strncpy(password, optarg, sizeof(password));
				break;
			case 'l':
				strncpy(host, optarg, sizeof(password));
				break;
			case 'g':
				bitrate = atoi(optarg);
				break;
			case 'n':
				port = atoi(optarg);
				break;
			case 'z':
				display = 0;
				break;
			case 'o':
				strncpy(output, optarg, sizeof(output));
				break;
			case 'b':
				strncpy(bug, optarg, sizeof(bug));
				break;
			case 'c':
				daemon = 1;
				break;
			case 'q':
				strncpy(alsa_device, optarg, sizeof(alsa_device));
				break;
			case '?':
				printf("option argument fail\n");
				exit(1);
			default:			
				break;
		}
	}
		
	composite_fps = capture_fps;
	encoding_fps = capture_fps;

	composite_width = capture_width;
	composite_height = capture_height;

	encoding_width = capture_width;
	encoding_height = capture_height;

	if (display) {
		display_width = capture_width;
		display_height = capture_height;
	} else {
		display_width = 0;
		display_height = 0;
	}


	
	krad_cam = krad_cam_create( device, output, host, port, mount, password, bitrate, krad_audio_api, capture_width, capture_height, capture_fps, 
								composite_width, composite_height, composite_fps, display_width, display_height, encoding_width, 
								encoding_height, encoding_fps, &do_shutdown, daemon, bug, bug_x, bug_y, alsa_device );

	krad_cam_run(krad_cam);

	krad_cam_destroy( krad_cam );


	return 0;

}

