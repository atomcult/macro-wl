#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libevdev.h>
#include <libinput.h>

static int open_restricted(const char *path, int flags, void *user_data)
{
  bool *grab = user_data;
  int fd = open(path, flags);

  if (fd < 0)
    fprintf(stderr, "Failed to open %s (%s)\n",
      path, strerror(errno));
  else if (grab && *grab && ioctl(fd, EVIOCGRAB, (void*)1) == -1)
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
  struct libinput_event_keyboard *key_event =
    libinput_event_get_keyboard_event(event);
  enum libinput_key_state state_pressed;
  uint32_t key;
  const char *keyname;

  state_pressed = libinput_event_keyboard_get_key_state(key_event);
  if (state_pressed)
    return;

  key = libinput_event_keyboard_get_key(key_event);

  keyname = libevdev_event_code_get_name(EV_KEY, key);
  keyname = keyname ? keyname : "???";

  printf("%s\n", keyname);
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

int main(int argc, char **argv) {
  struct libinput *context;
  struct libinput_device *dev;
  struct pollfd fds;
  bool grab = true;

  if (argc != 2) {
    printf(
      "USAGE: %s <device>\n",
      argv[0]
    );
    return EXIT_FAILURE;
  }

  context = libinput_path_create_context(&interface, &grab);
  dev = libinput_path_add_device(context, argv[1]);

  fds.fd = libinput_get_fd(context);
  fds.events = POLLIN;
  fds.revents = 0;

  if (poll(&fds, 1, -1) > -1) {
    do {
      handle_events(context);
    } while (poll(&fds, 1, -1) > -1);
  }
  handle_events(context);

  return EXIT_SUCCESS;
}
