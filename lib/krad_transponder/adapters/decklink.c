int decklink_audio_cb(void *arg, void *buffer, int frames) {
  kr_adapter_path *path;
  kr_adapter_path_av_cb_arg cb_arg;
  path = (kr_adapter_path *)arg;
  cb_arg.path = path;
  cb_arg.user = cb_arg.path->user;
  //cb_arg.path->av_cb(&cb_arg);
  return 0;
}

int decklink_image_cb(void *user, kr_image *image) {
  kr_adapter_path *path;
  kr_adapter_path_av_cb_arg cb_arg;
  path = (kr_adapter_path *)user;
  cb_arg.path = path;
  cb_arg.user = cb_arg.path->user;
  cb_arg.image = *image;
  cb_arg.path->av_cb(&cb_arg);
  return 0;
}

void decklink_adapter_path_destroy(kr_adapter_path *path) {
  kr_decklink_stop(path->adapter->handle.decklink);
}

void decklink_adapter_path_create(kr_adapter_path *path) {
  kr_decklink *dl;
  kr_decklink_path_info info;
  dl = path->adapter->handle.decklink;
  memset(&info, 0, sizeof(kr_decklink_path_info));
  info.width = 1920;
  info.height = 1080;
  info.num = 60000;
  info.den = 1001;
  strcpy(info.video_connector, "hdmi");
  strcpy(info.audio_connector, "hdmi");
  kr_decklink_set_video_mode(dl, info.width, info.height, info.num, info.den);
  kr_decklink_set_video_input(dl, info.video_connector);
  kr_decklink_set_audio_input(dl, info.audio_connector);
  dl->user = path;
  dl->image_cb = decklink_image_cb;
  dl->audio_cb = decklink_audio_cb;
  kr_decklink_start(dl);
}

void decklink_adapter_destroy(kr_adapter *adapter) {
  kr_decklink_destroy(adapter->handle.decklink);
  adapter->handle.decklink = NULL;
}

void decklink_adapter_create(kr_adapter *adapter) {
  /*  kr_decklink_setup setup;
  memset(&setup, 0, sizeof(kr_decklink_setup));
  setup.dev = adapter->info.api_info.decklink.dev;
  setup.priority = adapter->info.api_info.decklink.priority;*/
  adapter->handle.decklink = kr_decklink_create("0");
}
