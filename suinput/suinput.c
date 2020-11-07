/*
suinput - Simple C-API to the Linux uinput-system.
Copyright (C) 2009 Tuomas Räsänen <tuos@codegrove.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include "suinput.h"
#include "suinput-keys.h"

#define die(str, args...) { \
	perror(str); \
	exit(EXIT_FAILURE); \
}

char* UINPUT_FILEPATHS[] = {
    "/dev/uinput",
    "/dev/input/uinput",
    "/dev/misc/uinput",
    "/android/dev/uinput",
};
#define UINPUT_FILEPATHS_COUNT (sizeof(UINPUT_FILEPATHS) / sizeof(char*))

int suinput_write(int uinput_fd,
                  uint16_t type, uint16_t code, int32_t value)
{
    struct input_event event;
    memset(&event, 0, sizeof(event));
    gettimeofday(&event.time, 0); /* This should not be able to fail ever.. */
    event.type = type;
    event.code = code;
    event.value = value;
    if (write(uinput_fd, &event, sizeof(event)) != sizeof(event))
        return -1;
    return 0;
}

int suinput_write_syn(int uinput_fd,
                             uint16_t type, uint16_t code, int32_t value)
{
    if (suinput_write(uinput_fd, type, code, value))
        return -1;
    return suinput_write(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

void init_touch_device(int fd, uint16_t w, uint16_t h)
{

	// enable synchronization
	if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0)
		die("error: ioctl UI_SET_EVBIT EV_SYN");

	// enable 1 button
	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
		die("error: ioctl UI_SET_EVBIT EV_KEY");
	if (ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) < 0)
		die("error: ioctl UI_SET_KEYBIT");
	if (ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_FINGER) < 0)
		die("error: ioctl UI_SET_KEYBIT");
/*
	if (ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS) < 0)
		die("error: ioctl UI_SET_KEYBIT");
	if (ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS2) < 0)
		die("error: ioctl UI_SET_KEYBIT");
*/

	// enable 2 main axes + pressure (absolute positioning)
	if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0)
		die("error: ioctl UI_SET_EVBIT EV_ABS");
	if (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0)
		die("error: ioctl UI_SETEVBIT ABS_X");
	if (ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0)
		die("error: ioctl UI_SETEVBIT ABS_Y");
	if (ioctl(fd, UI_SET_ABSBIT, ABS_PRESSURE) < 0)
		die("error: ioctl UI_SETEVBIT ABS_PRESSURE");

        {
          struct uinput_abs_setup abs_setup;
          struct uinput_setup setup;

          memset(&abs_setup, 0, sizeof(abs_setup));
          abs_setup.code = ABS_X;
          abs_setup.absinfo.value = 0;
          abs_setup.absinfo.minimum = 0;
          abs_setup.absinfo.maximum = w;
          abs_setup.absinfo.fuzz = 0;
          abs_setup.absinfo.flat = 0;
          abs_setup.absinfo.resolution = 400;
          if (ioctl(fd, UI_ABS_SETUP, &abs_setup) < 0)
            die("error: UI_ABS_SETUP ABS_X");

          memset(&abs_setup, 0, sizeof(abs_setup));
          abs_setup.code = ABS_Y;
          abs_setup.absinfo.value = 0;
          abs_setup.absinfo.minimum = 0;
          abs_setup.absinfo.maximum = h;
          abs_setup.absinfo.fuzz = 0;
          abs_setup.absinfo.flat = 0;
          abs_setup.absinfo.resolution = 400;
          if (ioctl(fd, UI_ABS_SETUP, &abs_setup) < 0)
            die("error: UI_ABS_SETUP ABS_Y");

          memset(&abs_setup, 0, sizeof(abs_setup));
          abs_setup.code = ABS_PRESSURE;
          abs_setup.absinfo.value = 0;
          abs_setup.absinfo.minimum = 0;
          abs_setup.absinfo.maximum = 100;
          abs_setup.absinfo.fuzz = 0;
          abs_setup.absinfo.flat = 0;
          abs_setup.absinfo.resolution = 0;
          if (ioctl(fd, UI_ABS_SETUP, &abs_setup) < 0)
            die("error: UI_ABS_SETUP ABS_PRESSURE");

          memset(&setup, 0, sizeof(setup));
          snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "VNCINPUT");
          setup.id.bustype = BUS_VIRTUAL;
          setup.id.vendor  = 0x1;
          setup.id.product = 0x1;
          setup.id.version = 2;
          setup.ff_effects_max = 0;
          if (ioctl(fd, UI_DEV_SETUP, &setup) < 0)
            die("error: UI_DEV_SETUP");

          if (ioctl(fd, UI_DEV_CREATE) < 0)
            die("error: ioctl");
        }
}

void init_keyboard_device(int fd)
{
    int i;
	
	int n = sizeof(keyboard_keys) / sizeof(keyboard_keys[0]); 


	// enable synchronization
	if (ioctl(fd, UI_SET_EVBIT, EV_SYN) < 0)
		die("error: ioctl UI_SET_EVBIT EV_SYN");


	// enable 1 button
	if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
		die("error: ioctl UI_SET_EVBIT EV_KEY");

	/// Key and button repetition events
	if (ioctl(fd, UI_SET_EVBIT, EV_REP) == -1)
		die("error: ioctl UI_SET_EVBIT EV_REP");
/*
	if (ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) < 0)
		die("error: ioctl UI_SET_KEYBIT");
	if (ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_FINGER) < 0)
		die("error: ioctl UI_SET_KEYBIT");

	if (ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS) < 0)
		die("error: ioctl UI_SET_KEYBIT");
	if (ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS2) < 0)
		die("error: ioctl UI_SET_KEYBIT");
*/
    /* Configure device to handle keys, see suinput-keys.h. */
    for (i = 0; i < n; i++) {
        if (ioctl(fd, UI_SET_KEYBIT, keyboard_keys[i]) < 0)
            die("error: ioctl UI_SET_KEYBIT");
    }

/*
	// enable 2 main axes + pressure (absolute positioning)
	if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0)
		die("error: ioctl UI_SET_EVBIT EV_ABS");
	if (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0)
		die("error: ioctl UI_SETEVBIT ABS_X");
	if (ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0)
		die("error: ioctl UI_SETEVBIT ABS_Y");
	if (ioctl(fd, UI_SET_ABSBIT, ABS_PRESSURE) < 0)
		die("error: ioctl UI_SETEVBIT ABS_PRESSURE");
*/
        {
//          struct uinput_abs_setup abs_setup;
          struct uinput_setup setup;
/*
          memset(&abs_setup, 0, sizeof(abs_setup));
          abs_setup.code = ABS_X;
          abs_setup.absinfo.value = 0;
          abs_setup.absinfo.minimum = 0;
          abs_setup.absinfo.maximum = 1920;
          abs_setup.absinfo.fuzz = 0;
          abs_setup.absinfo.flat = 0;
          abs_setup.absinfo.resolution = 400;
          if (ioctl(fd, UI_ABS_SETUP, &abs_setup) < 0)
            die("error: UI_ABS_SETUP ABS_X");

          memset(&abs_setup, 0, sizeof(abs_setup));
          abs_setup.code = ABS_Y;
          abs_setup.absinfo.value = 0;
          abs_setup.absinfo.minimum = 0;
          abs_setup.absinfo.maximum = 1080;
          abs_setup.absinfo.fuzz = 0;
          abs_setup.absinfo.flat = 0;
          abs_setup.absinfo.resolution = 400;
          if (ioctl(fd, UI_ABS_SETUP, &abs_setup) < 0)
            die("error: UI_ABS_SETUP ABS_Y");

          memset(&abs_setup, 0, sizeof(abs_setup));
          abs_setup.code = ABS_PRESSURE;
          abs_setup.absinfo.value = 0;
          abs_setup.absinfo.minimum = 0;
          abs_setup.absinfo.maximum = 100;
          abs_setup.absinfo.fuzz = 0;
          abs_setup.absinfo.flat = 0;
          abs_setup.absinfo.resolution = 0;
          if (ioctl(fd, UI_ABS_SETUP, &abs_setup) < 0)
            die("error: UI_ABS_SETUP ABS_PRESSURE");
*/
          memset(&setup, 0, sizeof(setup));
          snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "VNCKEYBOARD");
          setup.id.bustype = BUS_VIRTUAL;
          setup.id.vendor  = 0x1;
          setup.id.product = 0x2;
          setup.id.version = 2;
          setup.ff_effects_max = 0;
          if (ioctl(fd, UI_DEV_SETUP, &setup) < 0)
            die("error: UI_DEV_SETUP");

          if (ioctl(fd, UI_DEV_CREATE) < 0)
            die("error: ioctl");
        }
}

int suinput_open(int iskeyboard, uint16_t w, uint16_t h)
{
    int original_errno = 0;
    int uinput_fd = -1;
    int i;

    for (i = 0; i < UINPUT_FILEPATHS_COUNT; ++i) {
        uinput_fd = open(UINPUT_FILEPATHS[i], O_WRONLY | O_NONBLOCK);
        if (uinput_fd != -1)
            break;
    }

    if (uinput_fd == -1)
        return -1;

	if (iskeyboard > 0){
		init_keyboard_device(uinput_fd);
	} else {
		init_touch_device(uinput_fd, w, h);
	}

    /*
  The reason for generating a small delay is that creating succesfully
  an uinput device does not guarantee that the device is ready to process
  input events. It's probably due the asynchronous nature of the udev.
  However, my experiments show that the device is not ready to process input
  events even after a device creation event is received from udev.
  */

    //sleep(2);

    return uinput_fd;

    err:

    /*
    At this point, errno is set for some reason. However, cleanup-actions
    can also fail and reset errno, therefore we store the original one
    and reset it before returning.
  */
    original_errno = errno;

    /* Cleanup. */
    close(uinput_fd); /* Might fail, but we don't care anymore at this point. */

    errno = original_errno;
    return -1;
}


int suinput_close(int uinput_fd)
{
    /*
    Sleep before destroying the device because there still can be some
    unprocessed events. This is not the right way, but I am still
    looking for better ways. The question is: how to know whether there
    are any unprocessed uinput events?
   */
    sleep(2);

    if (ioctl(uinput_fd, UI_DEV_DESTROY) == -1) {
        close(uinput_fd);
        return -1;
    }

    if (close(uinput_fd) == -1)
        return -1;

    return 0;
}

int suinput_move_pointer(int uinput_fd, int32_t x, int32_t y)
{
    if (suinput_write(uinput_fd, EV_REL, REL_X, x))
        return -1;
    return suinput_write_syn(uinput_fd, EV_REL, REL_Y, y);
}

int suinput_set_pointer(int uinput_fd, int32_t x, int32_t y)
{
    if (suinput_write(uinput_fd, EV_ABS, ABS_X, x))
        return -1;
    return suinput_write_syn(uinput_fd, EV_ABS, ABS_Y, y);
}

int suinput_press(int uinput_fd, uint16_t code)
{
    return suinput_write(uinput_fd, EV_KEY, code, 1);
}

int suinput_release(int uinput_fd, uint16_t code)
{
    return suinput_write(uinput_fd, EV_KEY, code, 0);
}

int suinput_click(int uinput_fd, uint16_t code)
{
    if (suinput_press(uinput_fd, code))
        return -1;
    return suinput_release(uinput_fd, code);
}
