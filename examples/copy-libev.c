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
 *     COPY_LIBEV_DEBUG=1 ./copy-ev ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

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
#define EXTENTS_SIZE (128 * 1024 * 1024)
#define GIB (1024 * 1024 * 1024)

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
    bool can_zero;
    bool can_extents;
};

enum request_state {
    IDLE,       /* Not used yet. */
    EXTENTS,    /* Getting extents from source. */
    READ,       /* Read from source. */
    WRITE,      /* Write to destiation. */
    ZERO,       /* Write zeroes to destiation. */
    SLEEP       /* Waiting for extents completion. */
};

static const char *state_names[] = {
    "idle", "extents", "read", "write", "zero", "sleep"
};

struct request {
    ev_timer watcher;       /* For starting on next loop iteration. */
    int64_t offset;
    size_t length;
    bool zero;
    unsigned char *data;
    size_t index;
    ev_tstamp started;
    enum request_state state;
};

struct extent {
    uint32_t length;
    bool zero;
};

static struct ev_loop *loop;
static ev_prepare prepare;
static struct connection src;
static struct connection dst;
static struct request requests[MAX_REQUESTS];

/* List of extents received from source server. */
static struct extent *extents;
static size_t extents_len;
static size_t extents_pos;

/* Set when we start asynchronous block status request. */
static bool extents_in_progress;

static int64_t size;
static int64_t offset;
static int64_t written;
static bool debug;
static ev_tstamp started;
static ev_timer progress;

static inline void start_request_soon (struct request *r);
static void start_request_cb (struct ev_loop *loop, ev_timer *w, int revents);
static void start_request(struct request *r);
static void start_read(struct request *r);
static void start_write(struct request *r);
static void start_zero(struct request *r);
static int read_completed(void *user_data, int *error);
static int request_completed(void *user_data, int *error);

/* Return true iff data is all zero bytes.
 *
 * Based on Rusty Russell's memeqzero:
 * https://rusty.ozlabs.org/?p=560
 */
static bool
is_zero (const unsigned char *data, size_t len)
{
  const unsigned char *p = data;
  size_t i;

  for (i = 0; i < 16; i++) {
    if (len == 0)
        return true;
    if (*p)
      return false;
    p++;
    len--;
  }

  return memcmp (data, p, len) == 0;
}

static inline const char *
request_state (struct request *r)
{
    return state_names[r->state];
}

static inline int
get_fd(struct connection *c)
{
    return nbd_aio_get_fd (c->nbd);
}

static inline int
get_events(struct connection *c)
{
    unsigned dir = nbd_aio_get_direction (c->nbd);

    switch (dir) {
        case LIBNBD_AIO_DIRECTION_READ:
            return EV_READ;
        case LIBNBD_AIO_DIRECTION_WRITE:
            return EV_WRITE;
        case LIBNBD_AIO_DIRECTION_BOTH:
            return EV_READ | EV_WRITE;
        default:
            return 0;
    }
}

static int
extent_callback (void *user_data, const char *metacontext, uint64_t offset,
                 uint32_t *entries, size_t nr_entries, int *error)
{
    struct request *r = user_data;

    if (strcmp (metacontext, LIBNBD_CONTEXT_BASE_ALLOCATION) != 0) {
        DEBUG ("Unexpected meta context: %s", metacontext);
        return 1;
    }

    /* Libnbd returns uint32_t pair (length, flags) for each extent. */
    extents_len = nr_entries / 2;

    extents = malloc (extents_len * sizeof *extents);
    if (extents == NULL)
        FAIL ("Cannot allocated extents: %s", strerror (errno));

    /* Copy libnbd entries to extents array. */
    for (int i = 0, j = 0; i < extents_len; i++, j=i*2) {
        extents[i].length = entries[j];

        /* Libnbd exposes both ZERO and HOLE flags. We care only about
         * ZERO status, meaning we can copy this extent using efficinet
         * zero method.
         */
        extents[i].zero = (entries[j + 1] & LIBNBD_STATE_ZERO) != 0;
    }

    DEBUG ("r%zu: received %zu extents for %s",
           r->index, extents_len, metacontext);

    return 1;
}

static int
extents_completed (void *user_data, int *error)
{
    struct request *r = (struct request *)user_data;
    int i;

    DEBUG ("r%zu: extents completed time=%.6f",
           r->index, ev_now (loop) - r->started);

    extents_in_progress = false;

    if (extents == NULL) {
        DEBUG ("r%zu: received no extents, disabling extents", r->index);
        src.can_extents = false;
    }

    /* Start the request to process recvievd extents. This must be done on the
     * next loop iteration, to avoid deadlock if we need to start a read.
     */
    start_request_soon(r);

    /* Wake up requests waiting for extents completion */
    for (i = 0; i < MAX_REQUESTS; i++) {
        struct request *r = &requests[i];
        if (r->state == SLEEP)
            start_request_soon (r);
    }

    return 1;
}

static bool
start_extents (struct request *r)
{
    size_t count = MIN (EXTENTS_SIZE, size - offset);
    int64_t cookie;

    DEBUG ("r%zu: start extents offset=%" PRIi64 " count=%zu",
           r->index, offset, count);

    cookie = nbd_aio_block_status (
        src.nbd, count, offset,
        (nbd_extent_callback) { .callback=extent_callback,
                                .user_data=r },
        (nbd_completion_callback) { .callback=extents_completed,
                                    .user_data=r },
        0);
    if (cookie == -1) {
        DEBUG ("Cannot get extents: %s", nbd_get_error ());
        src.can_extents = false;
        return false;
    }

    r->state = EXTENTS;
    extents_in_progress = true;

    return true;
}

/* Return next extent to process. */
static void
next_extent (struct request *r)
{
    uint32_t limit;
    uint32_t length = 0;
    bool is_zero;

    assert (extents);

    is_zero = extents[extents_pos].zero;

    /* Zero can be much faster, so try to zero entire extent. */
    if (is_zero && dst.can_zero)
        limit = MIN (EXTENTS_SIZE, size - offset);
    else
        limit = MIN (REQUEST_SIZE, size - offset);

    while (length < limit) {
        DEBUG ("e%zu: offset=%" PRIi64 " len=%" PRIu32 " zero=%d",
               extents_pos, offset, extents[extents_pos].length, is_zero);

        /* If this extent is too large, steal some data from it to
         * complete the request.
         */
        if (length + extents[extents_pos].length > limit) {
            uint32_t stolen = limit - length;

            extents[extents_pos].length -= stolen;
            length += stolen;
            break;
        }

        /* Consume the entire extent and start looking at the next one. */
        length += extents[extents_pos].length;
        extents[extents_pos].length = 0;

        if (extents_pos + 1 == extents_len)
            break;

        extents_pos++;

        /* If next extent is different, we are done. */
        if (extents[extents_pos].zero != is_zero)
            break;
    }

    assert (length > 0 && length <= limit);

    r->offset = offset;
    r->length = length;
    r->zero = is_zero;

    DEBUG ("r%zu: extent offset=%" PRIi64 " len=%zu zero=%d",
           r->index, r->offset, r->length, r->zero);

    offset += length;

    if (extents_pos + 1 == extents_len && extents[extents_pos].length == 0) {
        /* Processed all extents, clear extents. */
        DEBUG ("r%zu: consumed all extents offset=%" PRIi64, r->index, offset);
        free (extents);
        extents = NULL;
        extents_pos = 0;
        extents_len = 0;
    }
}

static inline void
start_request_soon (struct request *r)
{
    r->state = IDLE;
    ev_timer_init (&r->watcher, start_request_cb, 0, 0);
    ev_timer_start (loop, &r->watcher);
}

static void
start_request_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
    struct request *r = (struct request *)w;
    start_request (r);
}

/* Start async copy or zero request. */
static void
start_request(struct request *r)
{
    /* Cancel the request if we are done. */
    if (offset == size)
        return;

    r->started = ev_now (loop);

    /* If needed, get more extents from server. */
    if (src.can_extents && extents == NULL) {
        if (extents_in_progress) {
            r->state = SLEEP;
            return;
        }

        if (start_extents (r))
            return;
    }

    if (src.can_extents) {
        /* Handle the next extent. */
        next_extent (r);
        if (r->zero) {
            if (dst.can_zero) {
                start_zero (r);
            } else {
                memset (r->data, 0, r->length);
                start_write (r);
            }
        } else {
            start_read (r);
        }
    } else {
        /* Extents not available. */
        r->length = MIN (REQUEST_SIZE, size - offset);
        r->offset = offset;
        start_read (r);
        offset += r->length;
    }
}

static void
start_read(struct request *r)
{
    int64_t cookie;

    r->state = READ;

    DEBUG ("r%zu: start read offset=%" PRIi64 " len=%zu",
           r->index, r->offset, r->length);

    cookie = nbd_aio_pread (
        src.nbd, r->data, r->length, r->offset,
        (nbd_completion_callback) { .callback=read_completed,
                                    .user_data=r },
        0);
    if (cookie == -1)
        FAIL ("Cannot start read: %s", nbd_get_error ());
}

static int
read_completed (void *user_data, int *error)
{
    struct request *r = (struct request *)user_data;

    DEBUG ("r%zu: read completed offset=%" PRIi64 " len=%zu",
           r->index, r->offset, r->length);

    if (dst.can_zero && is_zero (r->data, r->length))
        start_zero (r);
    else
        start_write (r);

    return 1;
}

static void
start_write(struct request *r)
{
    int64_t cookie;

    r->state = WRITE;

    DEBUG ("r%zu: start write offset=%" PRIi64 " len=%zu",
           r->index, r->offset, r->length);

    cookie = nbd_aio_pwrite (
        dst.nbd, r->data, r->length, r->offset,
        (nbd_completion_callback) { .callback=request_completed,
                                    .user_data=r },
        0);
    if (cookie == -1)
        FAIL ("Cannot start write: %s", nbd_get_error ());
}

static void
start_zero(struct request *r)
{
    int64_t cookie;

    r->state = ZERO;

    DEBUG ("r%zu: start zero offset=%" PRIi64 " len=%zu",
           r->index, r->offset, r->length);

    cookie = nbd_aio_zero (
        dst.nbd, r->length, r->offset,
        (nbd_completion_callback) { .callback=request_completed,
                                    .user_data=r },
        0);
    if (cookie == -1)
        FAIL ("Cannot start zero: %s", nbd_get_error ());
}

/* Called when async copy or zero request completed. */
static int
request_completed (void *user_data, int *error)
{
    struct request *r = (struct request *)user_data;

    written += r->length;

    DEBUG ("r%zu: %s completed offset=%" PRIi64 " len=%zu, time=%.6f",
           r->index, request_state (r), r->offset, r->length,
           ev_now (loop) - r->started);

    if (written == size) {
        /* The last write completed. Stop all watchers and break out
         * from the event loop.
         */
        ev_io_stop (loop, &src.watcher);
        ev_io_stop (loop, &dst.watcher);
        ev_prepare_stop (loop, &prepare);
        ev_break (loop, EVBREAK_ALL);
    }

    /* If we have more work, start a new request on the next loop
     * iteration, to avoid deadlock if we need to start a zero or write.
     */
    if (offset < size)
        start_request_soon(r);

    return 1;
}

/* Notify libnbd about io events. */
static void
io_cb (struct ev_loop *loop, ev_io *w, int revents)
{
    struct connection *c = (struct connection *)w;

    /* Based on lib/poll.c, we need to prefer read over write, and avoid
     * invoking both notify_read() and notify_write(), since notify_read() may
     * change the state of the handle.
     */

    if (revents & EV_READ)
        nbd_aio_notify_read (c->nbd);
    else if (revents & EV_WRITE)
        nbd_aio_notify_write (c->nbd);
}

static void
progress_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
    ev_tstamp duration = ev_now (loop) - started;

    printf ("[ %6.2f%% ] %.2f GiB, %.2f seconds, %.2f GiB/s      %c",
            (double) written / size * 100,
            (double) size / GIB,
            duration,
            (double) written / GIB / duration,
            revents ? '\r' : '\n');

    fflush (stdout);
}

static void
start_progress ()
{
    started = ev_now (loop);
    ev_timer_init (&progress, progress_cb, 0, 0.1);
    ev_timer_start (loop, &progress);
}

static void
finish_progress ()
{
    ev_now_update (loop);
    progress_cb (loop, &progress, 0);
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

    start_progress ();

    src.nbd = nbd_create ();
    if (src.nbd == NULL)
        FAIL ("Cannot create source: %s", nbd_get_error ());

    dst.nbd = nbd_create ();
    if (dst.nbd == NULL)
        FAIL ("Cannot create destination: %s", nbd_get_error ());

    debug = getenv ("COPY_LIBEV_DEBUG") != NULL;

    /* Configure soruce to report extents. */

    if (nbd_add_meta_context (src.nbd, LIBNBD_CONTEXT_BASE_ALLOCATION))
        FAIL ("Cannot add base:allocation: %s", nbd_get_error ());

    /* Connecting is fast, so use the syncronous API. */

    if (nbd_connect_uri (src.nbd, argv[1]))
        FAIL ("Cannot connect to source: %s", nbd_get_error ());

    src.can_extents = nbd_can_meta_context (
        src.nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) > 0;

    if (nbd_connect_uri (dst.nbd, argv[2]))
        FAIL ("Cannot connect to destination: %s", nbd_get_error ());

    size = nbd_get_size (src.nbd);

    if (size > nbd_get_size (dst.nbd))
        FAIL ("Destinatio is not large enough\n");

    /* Check destination server capabilities. */

    dst.can_zero = nbd_can_zero (dst.nbd) > 0;

    /* Start the copy "loop".  When request completes, it starts the
     * next request, until entire image was copied. */

    for (i = 0; i < MAX_REQUESTS; i++) {
        struct request *r = &requests[i];
        r->index = i;
        r->data = malloc (REQUEST_SIZE);
        if (r->data == NULL)
            FAIL ("Cannot allocate buffer: %s", strerror (errno));

        start_request(r);
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

    finish_progress ();

    return 0;
}
