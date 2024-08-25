#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/input.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include <libevdev.h>
#include <libinput.h>

static const char *USAGE =
  "USAGE: %s [OPTIONS] <device-path>\n"
  "\n"
  "OPTIONS:\n"
  "  -h, --help        Print this help\n"
  "  -g, --grab        Take exclusive control of the device\n"
  "  -n, --no-grab     Take exclusive control of the device\n"
  "  -p, --on-press    Print keys when the key is pressed (default)\n"
  "  -r, --on-release  Print keys when the key is released\n";

typedef struct {
  char *prog_path;
  char *device_path;
  bool grab;
  bool on_press;
} config;

static int open_restricted(const char *path, int flags, void *data) {
  bool *grab = data;

  int fd = open(path, flags);
  if (fd < 0)
    fprintf(stderr, "Failed to open %s (%s)\n",
      path, strerror(errno));
  else if (grab && *grab && ioctl(fd, EVIOCGRAB, (void*)true) == -1)
    fprintf(stderr, "Grab requested, but failed for %s (%s)",
      path, strerror(errno));

  return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *data) {
  close(fd);
}

static const struct libinput_interface interface = {
  .open_restricted  = open_restricted,
  .close_restricted = close_restricted,
};

static void print_key(config *cfg, struct libinput_event *event) {
  struct libinput_event_keyboard *key_event;
  key_event = libinput_event_get_keyboard_event(event);

  enum libinput_key_state key_state;
  key_state = libinput_event_keyboard_get_key_state(key_event);

  if (key_state ^ cfg->on_press)
    return;

  uint32_t key = libinput_event_keyboard_get_key(key_event);

  const char *key_name = libevdev_event_code_get_name(EV_KEY, key);

  printf("%s\n", key_name ? key_name : "?");
}

static void handle_events(config *cfg, struct libinput *context) {
  struct libinput_event *event;

  libinput_dispatch(context);

  // Handle each new event
  while ((event = libinput_get_event(context))) {
    enum libinput_event_type type = libinput_event_get_type(event);
    switch (type) {
    case LIBINPUT_EVENT_KEYBOARD_KEY:
      print_key(cfg, event);
      break;
    default:
      break;
    }
  }
}

static void parse_config(config *cfg, int argc, char **argv) {
  static const char *short_options = "ghpr";
  static const struct option long_options[] = {
    {"grab",       no_argument, 0, 'g'},
    {"help",       no_argument, 0, 'h'},
    {"no-grab",    no_argument, 0, 'n'},
    {"on-press",   no_argument, 0, 'p'},
    {"on-release", no_argument, 0, 'r'},
    {0,            no_argument, 0,  0 },
  };

  // Set defaults
  cfg->prog_path   = argv[0];
  cfg->device_path = NULL;
  cfg->grab        = false;
  cfg->on_press    = true;

  // Handle options
  for (;;) {
    int option_index;

    int opt = getopt_long(argc, argv,
                          short_options, long_options, &option_index);
    if (opt == -1)
      break;

    switch (opt) {
    case 'g':
      cfg->grab = true;
      break;
    case 'n':
      cfg->grab = false;
      break;
    case 'p':
      cfg->on_press = true;
      break;
    case 'r':
      cfg->on_press = false;
      break;
    case '?':
      fprintf(stderr, "\n");
    default:
      fprintf(stderr, USAGE, cfg->prog_path);
      exit(EXIT_FAILURE);
    }
  }

  // Handle positional arguments
  if (optind < argc) {
    while (optind < argc) {
      if (cfg->device_path) {
        fprintf(stderr, "%s: too many positional arguments!\n\n",
                cfg->prog_path);
        fprintf(stderr, USAGE, cfg->prog_path);
        exit(EXIT_FAILURE);
      }
      cfg->device_path = argv[optind++];
    }
  }
}

static void validate_config(config *cfg) {
  // Make sure that device path was provided
  if (cfg->device_path == NULL) {
    fprintf(stderr, "%s: device path not provided\n\n", cfg->prog_path);
    fprintf(stderr, USAGE, cfg->prog_path);
    exit(EXIT_FAILURE);
  }

  // Make sure that the device path exists
  struct stat device;
  if (stat(cfg->device_path, &device)) {
    fprintf(stderr, "%s: device doesn't exist: %s\n",
            cfg->prog_path, cfg->device_path);
    exit(EXIT_FAILURE);
  }

  // Make sure that the device is a character device
  if (S_ISCHR(device.st_mode) == 0) {
    fprintf(stderr, "%s: device isn't a valid character device: %s\n",
            cfg->prog_path, cfg->device_path);
    exit(EXIT_FAILURE);
  }
}

static void poll_device(config *cfg) {
  struct libinput *context;
  context = libinput_path_create_context(&interface, &cfg->grab);

  struct libinput_device *dev;
  dev = libinput_path_add_device(context, cfg->device_path);

  struct pollfd fds;
  fds.fd = libinput_get_fd(context);
  fds.events = POLLIN;
  fds.revents = 0;

  while (poll(&fds, 1, -1) > -1)
    handle_events(cfg, context);
}

int main(int argc, char **argv) {
  config cfg;

  parse_config(&cfg, argc, argv);
  validate_config(&cfg);
  poll_device(&cfg);

  return EXIT_SUCCESS;
}
