#include <stdio.h>
#include <unistd.h>

#include <krad_mkv_demux.h>

static char logfile[256];

void show_log () {

  int ret;
  char *args[3];

  args[0] = "cat";
  args[1] = logfile;
  args[2] = NULL;

  ret = execv ("/bin/cat", args);
  if (ret == -1) {
    printf ("Error running cat...\n");
  }
}

void krad_debug_init () {

  krad_system_init ();
   
  sprintf (logfile, "%s/kr_mkvinfo_%"PRIu64".log",
           getenv ("HOME"), krad_unixtime ());

  krad_system_log_on (logfile);

  printf ("Logging to: %s\n", logfile);
}

void krad_debug_shutdown () {
  krad_system_log_off ();
  show_log ();
}

int main (int argc, char *argv[]) {

  int32_t ret;
  kr_mkv_t *mkv;

  krad_debug_init ();
 
  mkv = kr_mkv_open_file (argv[1]);
 
  if (mkv == NULL) {
    printf ("Error opening %s\n", argv[1]);
  }

  if (mkv != NULL) {
    ret = kr_mkv_destroy (&mkv);
    if (ret < 0) {
      printf ("Error closing %s\n", argv[1]);
    }
  }
  
  krad_debug_shutdown ();

  return 0;
}