#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <poll.h>
#include <sys/syscall.h>
#include <wayland-client.h>

#define exit_if_fail(c) { \
	if (!(c)) { \
		fprintf (stderr, "%s(%d) '%s' failed.\n", __FUNCTION__, __LINE__, #c); \
		exit (-1); \
	} \
}

static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
	wl_callback_destroy (callback);
}

static const struct wl_callback_listener sync_listener = {
	sync_callback
};

void*
thread_sub (void *data)
{
	struct wl_display *display = (struct wl_display*)data;
	struct wl_event_queue *queue;
	int ret;

	/* make sure that display is created */
	while (!display)
		usleep (10000);

	/* This queue is fake to test sub-thread. It will do nothing */
	queue = wl_display_create_queue  (display);
	exit_if_fail (queue != NULL);

	/* sub-thread */
	while (1) {
		fprintf (stderr,
			 "*** tid(%ld) wl_display_dispatch_queue starts\n",
			 syscall(SYS_gettid));
		ret = wl_display_dispatch_queue(display, queue);
		fprintf (stderr,
			 "*** tid(%ld) wl_display_dispatch_queue returns %d\n",
			 syscall(SYS_gettid), ret);
		if (ret < 0)
			fprintf(stderr, "failed: wl_display_dispatch_queue\n");
	}
}

static void
thread_start (struct wl_display *display)
{
	pthread_t thread;

	if(pthread_create (&thread, NULL, thread_sub, display) < 0) {
		fprintf (stdout, "failed: staring thread\n");
		exit (-1);
	}

	pthread_detach (thread);
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	int ret;

	/* create a display object */
	display = wl_display_connect (NULL);
	exit_if_fail (display != NULL);

	/* start sub-thread with the same display object */
	thread_start (display);

	/* main-thread */
	while (1) {
		struct wl_callback *callback = wl_display_sync (display);
		exit_if_fail (callback != NULL);

		usleep(200);

		wl_callback_add_listener(callback, &sync_listener, NULL);

		fprintf (stderr, "+++ tid(%ld) wl_display_dispatch starts\n",
			 syscall(SYS_gettid));
		ret = wl_display_dispatch(display);
		fprintf (stderr,
			 "+++ tid(%ld) wl_display_dispatch returns %d\n",
			 syscall(SYS_gettid), ret);
		if (ret < 0)
			fprintf (stderr, "failed: wl_display_dispatch\n");
	}

	return 0;
}
