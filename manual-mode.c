#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <math.h>
#include <sys/time.h>

#include "pslr.h"
#include "pslr_model.h"
#include "pktriggercord-servermode.h"

#ifdef WIN32
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC | O_BINARY
#else
#define FILE_ACCESS O_WRONLY | O_CREAT | O_TRUNC
#endif

bool debug = false;
pslr_handle_t camhandle;

int open_file(char* output_file, int frameNo, user_file_format_t ufft) {
  printf("open_file called\n");
  int ofd;
  char fileName[256];

  if (!output_file) {
      printf("if (!output_file)\n");
      ofd = 1;
  } else {
      printf("if (output_file)\n");
      char *dot = strrchr(output_file, '.');
      int prefix_length;

      if (dot && !strcmp(dot+1, ufft.extension)) {
          prefix_length = dot - output_file;
      } else {
          prefix_length = strlen(output_file);
      }

      printf("snprintf(filename, 256, ...)\n");
      snprintf(fileName, 256, "%.*s-%04d.%s", prefix_length, output_file, frameNo, ufft.extension);

      printf("ofd = open(fileName, FILE_ACCESS, 0664)\n");
      ofd = open(fileName, FILE_ACCESS, 0664);
      if (ofd == -1) {
          fprintf(stderr, "Could not open %s\n", output_file);
          return -1;
      }
  }
  return ofd;
}

int save_buffer(pslr_handle_t camhandle, int bufno, int fd, pslr_status *status, user_file_format filefmt, int jpeg_stars) {
  pslr_buffer_type imagetype;
  uint8_t buf[65536];
  uint32_t length;
  uint32_t current;

  if (filefmt == USER_FILE_FORMAT_PEF) {
    imagetype = PSLR_BUF_PEF;
  } else if (filefmt == USER_FILE_FORMAT_DNG) {
    imagetype = PSLR_BUF_DNG;
  } else {
    imagetype = pslr_get_jpeg_buffer_type( camhandle, jpeg_stars );
  }

  DPRINT("get buffer %d type %d res %d\n", bufno, imagetype, status->jpeg_resolution);

  if (pslr_buffer_open(camhandle, bufno, imagetype, status->jpeg_resolution) != PSLR_OK) {
    return 1;
  }

  length = pslr_buffer_get_size(camhandle);
  DPRINT("Buffer length: %d\n", length);
  current = 0;

  while (true) {
    uint32_t bytes;
    bytes = pslr_buffer_read(camhandle, buf, sizeof (buf));
    if (bytes == 0) {
      break;
    }
    ssize_t r = write(fd, buf, bytes);
    if (r == 0) {
      DPRINT("write(buf): Nothing has been written to buf.\n");
    } else if (r == -1) {
      perror("write(buf)");
    } else if (r < bytes) {
      DPRINT("write(buf): only write %zu bytes, should be %d bytes.\n", r, bytes);
    }
    current += bytes;
  }
  pslr_buffer_close(camhandle);
  return 0;
}

void disconnect_camera(void) {
  printf("Disconnecting camera ...\n");
  camera_close(camhandle);
}

int main(int argc, char **argv) {
  char buf[2100];
  char *device = NULL;
  int fd;
  char *model = NULL;
  char output_file[100];
  int quality = -1;
  int timeout = 0;
  int image_index = 0;
  pslr_status status;
  user_file_format uff = USER_FILE_FORMAT_JPEG;
  user_file_format_t ufft = *get_file_format_t(uff);
  long at_exit_conf;
  int disconnect_at_exit;

  at_exit_conf = sysconf(_SC_ATEXIT_MAX);
  printf("ATEXIT_MAX = %ld\n", at_exit_conf);

  disconnect_at_exit = atexit(disconnect_camera);
  if (disconnect_at_exit != 0) {
    // Do not connect if we can't disconnect at exit
    fprintf(stderr, "Cannot set exit function\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    sprintf(output_file, "image_%d", image_index);

    if ( !(camhandle = camera_connect( model, device, timeout, buf)) ) {
      printf("%s", buf);
      exit(-1);
    }
    printf("Camera name %s\n", pslr_camera_name(camhandle));
    pslr_get_status(camhandle, &status);
    printf("pslr_get_status called - nothing died yet\n");
    printf("user_file_format uff and ufft defined\n");
    fd = open_file(output_file, 0, ufft);
    while (save_buffer(camhandle, 0, fd, &status, uff, quality)) {
      printf("Waiting to save buffer ...\n");
      usleep(1000000);
    }
    printf("Deleting buffer.\n");
    pslr_delete_buffer(camhandle, 0);
    if (fd != 1) {
      close(fd);
    }
    image_index++;
  }
}
