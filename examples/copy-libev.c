/* This example shows you how to make libnbd interoperate with the
 * libev event loop.  For more information about libvev see:
 *
 * http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod
 *
 * To build it you need the libev-devel pacakge.
 *
 * To run it:
 *
 *     nbdkit -r pattern size=1G -U /tmp/src.sock
 *     nbdkit memory size=1g -U /tmp/dst.sock
 *     ./copy-ev nbd+unix:///?socket=/tmp/src.sock nbd+unix:///?socket=/tmp/dst.sock
 *
 * To debug it:
 *
 *     LIBNBD_DEBUG=1 ./copy-ev ...
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libnbd.h>

#include <ev.h>

/* These values depend on the enviroment tested.
 *
 * For shared storage using direct I/O:
 *
 * MAX_REQUESTS 16
 * REQUEST_SIZE (1024 * 1024)
 *
 * For nbdkit memory plugin:
 *
 * MAX_REQUESTS 8
 * REQUEST_SIZE (128 * 1024)
 */
#define MAX_REQUESTS 16
#define REQUEST_SIZE (1024 * 1024)

#define MIN(a,b) (a) < (b) ? (a) : (b)

#define PROG "copy-libev"

#define DEBUG(fmt, ...)                                     \
  do {                                                      \
    if (debug)                                              \
      fprintf (stderr, PROG ": " fmt "\n", ## __VA_ARGS__); \
  } while (0)

#define FAIL(fmt, ...)                                      \
  do {                                                      \
    fprintf (stderr, PROG ": " fmt "\n", ## __VA_ARGS__);   \
    exit (EXIT_FAILURE);                                    \
  } while (0)

struct connection {
    ev_io watcher;
    struct nbd_handle *nbd;
};

struct request {
    int64_t offset;
    size_t length;
    unsigned char *data;
};

static struct ev_loop *loop;
static ev_prepare prepare;
static struct connection src;
static struct connection dst;
static struct request requests[MAX_REQUESTS];
static int64_t size;
static int64_t offset;
static int64_t written;
static bool debug;

static void start_read(struct request *r);
static int read_completed(void *user_data, int *error);
static int write_completed(void *user_data, int *error);

static inline int
get_fd(struct connection *c)
{
    return nbd_aio_get_fd (c->nbd);
}

static inline int
get_events(struct connection *c)
{
    int events = 0;
    unsigned dir = nbd_aio_get_direction (c->nbd);

    if (dir & LIBNBD_AIO_DIRECTION_WRITE)
        events |= EV_WRITE;

    if (dir & LIBNBD_AIO_DIRECTION_READ)
        events |= EV_READ;

    return events;
}

static void
start_read(struct request *r)
{
    int64_t cookie;

    assert (offset < size);

    r->length = MIN (REQUEST_SIZE, size - offset);
    r->offset = offset;

    DEBUG ("start read offset=%ld len=%ld", r->offset, r->length);

    cookie = nbd_aio_pread (
        src.nbd, r->data, r->length, r->offset,
        (nbd_completion_callback) { .callback=read_completed,
                                    .user_data=r },
        0);
    if (cookie == -1)
        FAIL ("Cannot start read: %s", nbd_get_error ());

    offset += r->length;
}

static int
read_completed (void *user_data, int *error)
{
    struct request *r = (struct request *)user_data;
    int64_t cookie;

    DEBUG ("read completed, starting write offset=%ld len=%ld",
           r->offset, r->length);

    cookie = nbd_aio_pwrite (
        dst.nbd, r->data, r->length, r->offset,
        (nbd_completion_callback) { .callback=write_completed,
                                    .user_data=r },
        0);
    if (cookie == -1)
        FAIL ("Cannot start write: %s", nbd_get_error ());

    return 1;
}

static int
write_completed (void *user_data, int *error)
{
    struct request *r = (struct request *)user_data;

    written += r->length;

    DEBUG ("write completed offset=%ld len=%ld", r->offset, r->length);

    if (written == size) {
        /* The last write completed. Stop all watchers and break out
         * from the event loop.
         */
        ev_io_stop (loop, &src.watcher);
        ev_io_stop (loop, &dst.watcher);
        ev_prepare_stop (loop, &prepare);
        ev_break (loop, EVBREAK_ALL);
    }

    /* If we have data to read, start a new read. */
    if (offset < size)
        start_read(r);

    return 1;
}

/* Notify libnbd about io events. */
static void
io_cb (struct ev_loop *loop, ev_io *w, int revents)
{
    struct connection *c = (struct connection *)w;

    if (revents & EV_WRITE)
        nbd_aio_notify_write (c->nbd);

    if (revents & EV_READ)
        nbd_aio_notify_read (c->nbd);
}

static inline void
update_watcher (struct connection *c)
{
    int events = get_events(c);

    if (events != c->watcher.events) {
        ev_io_stop (loop, &c->watcher);
        ev_io_set (&c->watcher, get_fd (c), events);
        ev_io_start (loop, &c->watcher);
    }
}

/* Update watchers events based on libnbd handle state. */
static void
prepare_cb (struct ev_loop *loop, ev_prepare *w, int revents)
{
    update_watcher (&src);
    update_watcher (&dst);
}

int
main (int argc, char *argv[])
{
    int i;

    loop = EV_DEFAULT;

    if (argc != 3)
        FAIL ("Usage: %s src-uri dst-uri", PROG);

    src.nbd = nbd_create ();
    if (src.nbd == NULL)
        FAIL ("Cannot create source: %s", nbd_get_error ());

    dst.nbd = nbd_create ();
    if (dst.nbd == NULL)
        FAIL ("Cannot create destination: %s", nbd_get_error ());

    debug = nbd_get_debug (src.nbd);

    /* Connecting is fast, so use the syncronous API. */

    if (nbd_connect_uri (src.nbd, argv[1]))
        FAIL ("Cannot connect to source: %s", nbd_get_error ());

    if (nbd_connect_uri (dst.nbd, argv[2]))
        FAIL ("Cannot connect to destination: %s", nbd_get_error ());

    size = nbd_get_size (src.nbd);

    if (size > nbd_get_size (dst.nbd))
        FAIL ("Destinatio is not large enough\n");

    /* Start the copy "loop".  When request completes, it starts the
     * next request, until entire image was copied. */

    for (i = 0; i < MAX_REQUESTS && offset < size; i++) {
        struct request *r = &requests[i];

        r->data = malloc (REQUEST_SIZE);
        if (r->data == NULL)
            FAIL ("Cannot allocate buffer: %s", strerror (errno));

        start_read(r);
    }

    /* Start watching events on src and dst handles. */

    ev_io_init (&src.watcher, io_cb, get_fd (&src), get_events (&src));
    ev_io_start (loop, &src.watcher);

    ev_io_init (&dst.watcher, io_cb, get_fd (&dst), get_events (&dst));
    ev_io_start (loop, &dst.watcher);

    /* Register a prepare watcher for updating src and dst events once
     * before the event loop waits for new events.
     */

    ev_prepare_init (&prepare, prepare_cb);
    ev_prepare_start (loop, &prepare);

    /* Run the event loop. The call will return when entire image was
     * copied.
     */

    ev_run (loop, 0);

    /* Copy completed - flush data to storage. */

    DEBUG ("flush");
    if (nbd_flush (dst.nbd, 0))
        FAIL ("Cannot flush: %s", nbd_get_error ());

    /* We don't care about errors here since data was flushed. */

    nbd_shutdown (dst.nbd, 0);
    nbd_close (dst.nbd);

    nbd_shutdown (src.nbd, 0);
    nbd_close (src.nbd);

    /* We can free requests data here, but it is not really needed. */

    return 0;
}
