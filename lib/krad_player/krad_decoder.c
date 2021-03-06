#include "krad_decoder.h"
#include "kr_client.h"

#include "krad_player_common.h"

typedef struct kr_decoder_msg_St kr_decoder_msg_t;

static void kr_decoder_start (void *actual);
static int kr_decoder_process (void *msgin, void *actual);
static void kr_decoder_destroy_actual (void *actual);

struct kr_decoder_msg_St {
  kr_decoder_cmd_t cmd;
  union {
    float real;
    int64_t integer;
  } param;
};

typedef union {
  krad_vhs_t *kvhs;
	krad_vpx_decoder_t *vpx;
  krad_theora_decoder_t *theora;
  krad_flac_t *flac;
  krad_opus_t *opus;
  krad_vorbis_t *vorbis;
} kr_decoder_codec_state;

struct kr_decoder_St {
  kr_decoder_state_t state;
  kr_machine_t *machine;  
  kr_decoder_codec_state dec;
  krad_codec_t codec;
};

void kr_decoder_destroy_instance_decoder (kr_decoder_t *decoder) {

  switch (decoder->codec) {
    case OPUS:
      krad_opus_decoder_destroy (decoder->dec.opus);  
      break;
    case FLAC:
      krad_flac_decoder_destroy (decoder->dec.flac);
      break;
    case VORBIS:
      krad_vorbis_decoder_destroy (&decoder->dec.vorbis);
      break;
    case KVHS:
      krad_vhs_decoder_destroy (decoder->dec.kvhs);
      break;
    case VP8:
      krad_vpx_decoder_destroy (&decoder->dec.vpx);
      break;
    case THEORA:
      krad_theora_decoder_destroy (decoder->dec.theora);
      break;
    default:
      break;
  }
  decoder->codec = NOCODEC;
}

void kr_decoder_create_instance_decoder (kr_decoder_t *decoder,
                                         krad_codec_header_t *header) {
  switch (header->codec) {
    case OPUS:
      decoder->dec.opus = krad_opus_decoder_create (header);
      break;
    case FLAC:
      decoder->dec.flac = krad_flac_decoder_create (header);
      break;
    case VORBIS:
      decoder->dec.vorbis = krad_vorbis_decoder_create (header);
      break;
    case KVHS:
      decoder->dec.kvhs = krad_vhs_create_decoder ();
      break;
    case VP8:
      decoder->dec.vpx = krad_vpx_decoder_create ();
      break;
    case THEORA:
      decoder->dec.theora = krad_theora_decoder_create (header);
      break;
    default:
      return;
  }
  decoder->codec = header->codec;
}

static int kr_decoder_check (kr_decoder_t *decoder,
                             kr_codeme_t *codeme) {

  if (codeme->codec != decoder->codec) {
    if (decoder->codec != NOCODEC) {
      kr_decoder_destroy_instance_decoder (decoder);
    }
    if (codeme->codec != NOCODEC) {
      kr_decoder_create_instance_decoder (decoder, codeme->hdr);
    } else {
      return -1;
    }
  }

  return 0;
}

int kr_decoder_decode_direct (kr_decoder_t *decoder,
                              kr_medium_t *medium,
                              kr_codeme_t *codeme) {
  int ret;
  
  ret = kr_decoder_check (decoder, codeme);
  if (ret < 0) {
    return ret;
  }
  
  switch (codeme->codec) {
    case OPUS:
      //kr_opus_decode (decoder->dec.opus, medium, codeme);
      //krad_opus_decoder_write (decoder->dec.opus, kr_slice->data, kr_slice->size);
      //bytes = krad_opus_decoder_read (krad_link->krad_opus, c + 1, (char *)krad_link->au_audio, 120 * 4);
      break;
    case FLAC:
      //kr_flac_decode (decoder->dec.flac, medium, codeme);
      //len = krad_flac_decode (decoder->dec.flac, kr_slice->data, kr_slice->size, krad_link->au_samples);
      //krad_resample_ring_write (krad_link->krad_resample_ring[c], (unsigned char *)krad_link->au_samples[c], len * 4);
      break;
    case VORBIS:
      kr_vorbis_decode (decoder->dec.vorbis, medium, codeme);
      break;
    case KVHS:
      //kr_vhs_decode (decoder->dec.kvhs, medium, codeme);
      break;
    case VP8:
      kr_vpx_decode (decoder->dec.vpx, medium, codeme);
      break;
    case THEORA:
      //kr_theora_decode (decoder->dec.theora, medium, codeme);
      break;
    default:
    return -1;
  }

  return 0;
}

/* Private Functions */

static int kr_decoder_process (void *msgin, void *actual) {

  kr_decoder_t *decoder;
  kr_decoder_msg_t *msg;

  msg = (kr_decoder_msg_t *)msgin;
  decoder = (kr_decoder_t *)actual;

  printf ("kr_decoder_process cmd %p\n", decoder);

  //printf ("kr_decoder_process cmd %d\n", msg->cmd);

  switch (msg->cmd) {
    case DODECODE:
      printf ("Got DECODE command!\n");
      break;
    case DECODERDESTROY:
      printf ("Got DECODERDESTROY command!\n");
      return 0;
  }
  
  //printf ("kr_decoder_process done\n");  
  
  return 1;
}

static void kr_decoder_destroy_actual (void *actual) {

  kr_decoder_t *decoder;

  decoder = (kr_decoder_t *)actual;

  printf ("kr_decoder_destroy_actual cmd %p\n", decoder);
}

static void kr_decoder_start (void *actual) {

  kr_decoder_t *decoder;

  decoder = (kr_decoder_t *)actual;

  decoder->state = DEIDLE;
  printf ("kr_decoder_start()!\n");
}

/* Public Functions */

void kr_decoder_destroy (kr_decoder_t **decoder) {
  kr_decoder_msg_t msg;
  if ((decoder != NULL) && (*decoder != NULL)) {
    printf ("kr_decoder_destroy()!\n");
    msg.cmd = DECODERDESTROY;
    krad_machine_msg ((*decoder)->machine, &msg);
    krad_machine_destroy (&(*decoder)->machine);
    free (*decoder);
    *decoder = NULL;
  }
}

kr_decoder_t *kr_decoder_create () {
  
  kr_decoder_t *decoder;
  kr_machine_params_t machine_params;

  decoder = calloc (1, sizeof(kr_decoder_t));

  decoder->codec = NOCODEC;

  machine_params.actual = decoder;
  machine_params.msg_sz = sizeof (kr_decoder_msg_t);
  machine_params.start = kr_decoder_start;
  machine_params.process = kr_decoder_process;
  machine_params.destroy = kr_decoder_destroy_actual;

  decoder->machine = krad_machine_create (&machine_params);
  
  return decoder;
};

kr_decoder_state_t kr_decoder_state_get (kr_decoder_t *decoder) {
  return decoder->state;
}

void kr_decoder_destroy_direct (kr_decoder_t **decoder) {
  if ((decoder != NULL) && (*decoder != NULL)) {
    kr_decoder_destroy_instance_decoder (*decoder);
    free (*decoder);
    *decoder = NULL;
  }
}

kr_decoder_t *kr_decoder_create_direct () {
  kr_decoder_t *decoder;
  decoder = calloc (1, sizeof(kr_decoder_t));
  decoder->codec = NOCODEC;
  return decoder;
};
