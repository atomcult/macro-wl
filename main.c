#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libevdev/libevdev.h>
#include <libinput.h>

#define NELEM(x) sizeof(x) / sizeof(*x)

typedef struct command {
  size_t length;
  char **elements;
} command;

static const char *USAGE =
  "USAGE: macro-wl [OPTIONS] <device>\n";

command COMMAND_MAP[KEY_MAX+1] = {0};

void usage()
{
  printf("%s", USAGE);
}

static int
open_restricted(const char *path, int flags, void *user_data)
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

static void
close_restricted(int fd, void *user_data)
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

  printf("%s (%d, %d)\n",
    keyname,
    key,
    libevdev_event_code_from_code_name(keyname));
}

void execute_command(struct libinput_event *event)
{
  struct libinput_event_keyboard *key_event =
    libinput_event_get_keyboard_event(event);
  enum libinput_key_state state_pressed;
  uint32_t key;

  state_pressed = libinput_event_keyboard_get_key_state(key_event);
  if (state_pressed)
    return;

  key = libinput_event_keyboard_get_key(key_event);

  command cmd = COMMAND_MAP[key];
  for (int index = 0; index < cmd.length; index++) {
    printf("%s ", cmd.elements[index]);
  }
  printf("\n");

  pid_t pid = fork();
  if (pid == -1) {
    // FIXME
    // Something went wrong
  } else if (pid == 0) {
    execv(cmd.elements[0], cmd.elements);
  }
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
        execute_command(event);
        break;
      case LIBINPUT_EVENT_NONE:
      default:
        break;
    }
  }
}

int main() {
  struct libinput *context;
  struct libinput_device *dev;
  struct pollfd fds;
  bool grab = true;

  static const char *CMD[] = {"sh", "-c", "echo 'Hello, world!' >output.txt", NULL};

  char **cmd = malloc(sizeof(CMD));
  for (int index = 0; index < 4; index++)
    cmd[index] = CMD[index];

  command new_command = {
    .length   = 4,
    .elements = cmd, 
  };
  COMMAND_MAP[KEY_ENTER] = new_command;

  context = libinput_path_create_context(&interface, &grab);
  dev = libinput_path_add_device(
    context,
    "/dev/input/by-id/usb-Dell_Dell_USB_Entry_Keyboard-event-kbd");

  fds.fd = libinput_get_fd(context);
  fds.events = POLLIN;
  fds.revents = 0;

  if (poll(&fds, 1, -1) > -1) {
    do {
      handle_events(context);
    } while (poll(&fds, 1, -1) > -1);
  }
  handle_events(context);

  for (int index = 0; index < KEY_MAX+1; index++) {
    char **elements = COMMAND_MAP[index].elements;
    if (elements)
      free(elements);
  }

  return EXIT_SUCCESS;
}
