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
  "  --help, -h  Print this help\n"
  "  --grab, -g  Take exclusive control of the device\n";

typedef struct config {
  char *prog_path;
  char *device_path;
  bool grab;
} config;

static int open_restricted(const char *path, int flags, void *user_data)
{
  bool *grab = user_data;
  int fd = open(path, flags);

  if (fd < 0)
    fprintf(stderr, "Failed to open %s (%s)\n",
      path, strerror(errno));
  else if (grab && *grab && ioctl(fd, EVIOCGRAB, (void*)true) == -1)
    fprintf(stderr, "Grab requested, but failed for %s (%s)",
      path, strerror(errno));

  return fd < 0 ? -errno : fd;
}

static void close_restricted(int fd, void *user_data)
{
  close(fd);
}

static const struct libinput_interface interface = {
  .open_restricted  = open_restricted,
  .close_restricted = close_restricted,
};

void print_key(struct libinput_event *event)
{
  enum libinput_key_state state_pressed;
  uint32_t key;
  const char *keyname;

  struct libinput_event_keyboard *key_event =
    libinput_event_get_keyboard_event(event);

  state_pressed = libinput_event_keyboard_get_key_state(key_event);
  if (state_pressed)
    return;

  key = libinput_event_keyboard_get_key(key_event);

  keyname = libevdev_event_code_get_name(EV_KEY, key);

  printf("%s\n", keyname ? keyname : "?");
}

void handle_events(struct libinput *context)
{
  struct libinput_event *event;

  // Handle each new event
  libinput_dispatch(context);
  while ((event = libinput_get_event(context))) {
    enum libinput_event_type type = libinput_event_get_type(event);
    switch (type) {
    case LIBINPUT_EVENT_KEYBOARD_KEY:
      print_key(event);
      break;
    case LIBINPUT_EVENT_NONE:
    default:
      break;
    }
  }
}

void parse_config(config *cfg, int argc, char **argv) {
  int opt;
  int option_index = 0;

  static const char *short_options = "gh";
  static const struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"grab", no_argument, 0, 'g'},
    {0,      no_argument, 0,  0 },
  };

  // Set defaults
  cfg->prog_path   = argv[0];
  cfg->device_path = NULL;
  cfg->grab        = false;

  // Handle options
  for (;;) {
    opt = getopt_long(argc, argv, short_options, long_options, &option_index);

    if (opt == -1)
      break;

    switch (opt) {
    case 'g':
      cfg->grab = true;
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
        fprintf(stderr, "%s: too many positional arguments!\n\n", cfg->prog_path);
        fprintf(stderr, USAGE, cfg->prog_path);
        exit(EXIT_FAILURE);
      }
      cfg->device_path = argv[optind++];
    }
  }
  
  return;
}

void validate_config(config *cfg) {
  struct stat device;

  if (cfg->device_path == NULL) {
    fprintf(stderr, "%s: device path not provided\n\n", cfg->prog_path);
    fprintf(stderr, USAGE, cfg->prog_path);
    exit(EXIT_FAILURE);
  }

  // Make sure that the device path exists
  if (stat(cfg->device_path, &device)) {
    fprintf(stderr, "%s: device doesn't exist: %s\n", cfg->prog_path, cfg->device_path);
    exit(EXIT_FAILURE);
  }

  // Make sure that the device is a character device
  if (S_ISCHR(device.st_mode) == 0) {
    fprintf(stderr, "%s: device isn't a valid character device: %s\n",
            cfg->prog_path, cfg->device_path);
    exit(EXIT_FAILURE);
  }

  return;
}

void poll_device(config *cfg) {
  struct libinput *context;
  struct libinput_device *dev;
  struct pollfd fds;
  
  context = libinput_path_create_context(&interface, &cfg->grab);
  dev = libinput_path_add_device(context, cfg->device_path);

  fds.fd = libinput_get_fd(context);
  fds.events = POLLIN;
  fds.revents = 0;

  while (poll(&fds, 1, -1) > -1)
    handle_events(context);
}

int main(int argc, char **argv) {
  config cfg;

  parse_config(&cfg, argc, argv);
  validate_config(&cfg);
  poll_device(&cfg);

  return EXIT_SUCCESS;
}
