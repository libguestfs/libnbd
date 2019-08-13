/* This example shows you how to make libnbd interoperate with the
 * glib main loop.  For more information about glib main loop see:
 *
 * https://developer.gnome.org/glib/stable/glib-The-Main-Event-Loop.html
 *
 * To run it, simply do:
 *
 *   ./examples/glib-main-loop
 *
 * For debugging, do:
 *
 *   LIBNBD_DEBUG=1 ./examples/glib-main-loop
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

#include <libnbd.h>

#include <glib.h>

struct NBDSource;
typedef void (*connecting_callback_t) (struct NBDSource *);
typedef void (*connected_callback_t) (struct NBDSource *);

/* This is the derived GSource type. */
struct NBDSource {
  /* The base type.  This MUST be the first element in this struct. */
  GSource source;

  /* The underlying libnbd handle. */
  struct nbd_handle *nbd;
  bool debug;                   /* true if handle has debug set */

  /* The poll file descriptor.  We store the value here as well as in
   * the GSource so we can see if it changes.
   */
  int fd;
  gpointer tag;

  /* You can optionally register callbacks to be called when the
   * handle changes state:
   *
   * connected_callback is called once when the handle moves from
   * connecting to connected (ready) state.
   */
  connected_callback_t connected_callback;
  bool called_connected_callback;

  /* Arbitrary pointer for use by caller. */
  gpointer user_data;
};

/* Print debug statements when debugging is set for the handle. */
#define DEBUG(source, fs, ...)                                          \
  do {                                                                  \
    if ((source)->debug)                                                \
      fprintf (stderr, "glib: debug: " fs "\n", ## __VA_ARGS__);        \
  } while (0)

/* These are the GSource functions for libnbd handles. */
static inline int
events_from_nbd (struct nbd_handle *nbd)
{
  unsigned dir = nbd_aio_get_direction (nbd);
  int r = 0;

  if ((dir & LIBNBD_AIO_DIRECTION_READ) != 0)
    r |= G_IO_IN;
  if ((dir & LIBNBD_AIO_DIRECTION_WRITE) != 0)
    r |= G_IO_OUT;
  return r;
}

static gboolean
prepare (GSource *sp, gint *timeout_)
{
  struct NBDSource *source = (struct NBDSource *) sp;
  int new_fd;
  int events;

  /* The poll file descriptor can change or become invalid at any
   * time.
   */
  new_fd = nbd_aio_get_fd (source->nbd);
  if (source->fd != new_fd) {
    if (source->tag != NULL) {
      g_source_remove_unix_fd ((GSource *) source, source->tag);
      source->fd = -1;
      source->tag = NULL;
    }
    if (new_fd >= 0) {
      source->fd = new_fd;
      source->tag = g_source_add_unix_fd ((GSource *) source, new_fd, 0);
    }
  }

  if (!source->tag)
    return FALSE;

  events = events_from_nbd (source->nbd);
  g_source_modify_unix_fd ((GSource *) source, source->tag, events);
  *timeout_ = -1;

  DEBUG (source, "prepare: events = 0x%x%s%s",
         events,
         events & G_IO_IN ? " G_IO_IN" : "",
         events & G_IO_OUT ? " G_IO_OUT" : "");

  if (source->connected_callback &&
      !source->called_connected_callback &&
      nbd_aio_is_ready (source->nbd)) {
    DEBUG (source, "calling connected_callback");
    source->connected_callback (source);
    source->called_connected_callback = true;
  }

  return FALSE;
}

static gboolean
check (GSource *sp)
{
  struct NBDSource *source = (struct NBDSource *) sp;
  unsigned dir;
  int revents;

  if (!source->tag)
    return FALSE;

  revents = g_source_query_unix_fd ((GSource *) source, source->tag);
  dir = nbd_aio_get_direction (source->nbd);

  DEBUG (source, "check: direction = 0x%x%s%s, revents = 0x%x%s%s",
         dir,
         dir & LIBNBD_AIO_DIRECTION_READ ? " READ" : "",
         dir & LIBNBD_AIO_DIRECTION_WRITE ? " WRITE" : "",
         revents,
         revents & G_IO_IN ? " G_IO_IN" : "",
         revents & G_IO_OUT ? " G_IO_OUT" : "");

  if ((revents & G_IO_IN) != 0 && (dir & LIBNBD_AIO_DIRECTION_READ) != 0)
    return TRUE;
  if ((revents & G_IO_OUT) != 0 && (dir & LIBNBD_AIO_DIRECTION_WRITE) != 0)
    return TRUE;

  return FALSE;
}

static gboolean
dispatch (GSource *sp,
          GSourceFunc callback,
          gpointer user_data)
{
  struct NBDSource *source = (struct NBDSource *) sp;
  int revents;
  int r;

  revents = g_source_query_unix_fd ((GSource *) source, source->tag);

  DEBUG (source, "dispatch: revents = 0x%x%s%s",
         revents,
         revents & G_IO_IN ? " G_IO_IN" : "",
         revents & G_IO_OUT ? " G_IO_OUT" : "");

  r = 0;
  if ((revents & G_IO_IN) != 0)
    r = nbd_aio_notify_read (source->nbd);
  else if ((revents & G_IO_OUT) != 0)
    r = nbd_aio_notify_write (source->nbd);

  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;
}

static void
finalize (GSource *sp)
{
  struct NBDSource *source = (struct NBDSource *) sp;

  DEBUG (source, "finalize");

  assert (nbd_aio_in_flight (source->nbd) == 0);
  assert (nbd_aio_peek_command_completed (source->nbd) == -1);
  nbd_close (source->nbd);
}

GSourceFuncs nbd_source_funcs = {
  .prepare = prepare,
  .check = check,
  .dispatch = dispatch,
  .finalize = finalize,
};

/* Create a libnbd GSource from a libnbd handle.
 *
 * Note that the return value is also a ‘GSource *’, you just have to
 * cast the return value if you need a GSource pointer.
 */
static struct NBDSource *
create_libnbd_gsource (struct nbd_handle *nbd)
{
  struct NBDSource *source;

  source =
    (struct NBDSource *) g_source_new (&nbd_source_funcs, sizeof *source);
  source->nbd = nbd;
  source->debug = nbd_get_debug (nbd);
  source->fd = -1;

  return source;
}

/*----------------------------------------------------------------------*/

/* The rest of this file is an example showing how to use the GSource
 * defined above to control two nbdkit subprocesses, copying from one
 * to the other in parallel.
 */

/* Source and destination nbdkit instances. */
static struct NBDSource *gssrc, *gsdest;

#define SIZE (1024*1024*1024)

static const char *src_args[] = {
  "nbdkit", "-s", "--exit-with-parent", "-r", "pattern", "size=1G", NULL
};

static const char *dest_args[] = {
  "nbdkit", "-s", "--exit-with-parent", "memory", "size=1G", NULL
};

/* The list of buffers waiting to be written.  Note that the source
 * server can answer requests out of order so these buffers may not be
 * sorted by offset.
 */
#define MAX_BUFFERS 16
#define BUFFER_SIZE 65536

enum buffer_state {
  BUFFER_UNUSED = 0,
  BUFFER_READING,
  BUFFER_READ_COMPLETED,
  BUFFER_WRITING,
};

struct buffer {
  uint64_t offset;
  enum buffer_state state;
  char *data;
};

static struct buffer buffers[MAX_BUFFERS];
static size_t nr_buffers;

static bool finished, reader_paused;

static GMainLoop *loop;

static void connected (struct NBDSource *source);
static gboolean read_data (gpointer user_data);
static int finished_read (void *vp, int *error);
static gboolean write_data (gpointer user_data);
static int finished_write (void *vp, int *error);

int
main (int argc, char *argv[])
{
  struct nbd_handle *src, *dest;
  GMainContext *loopctx = NULL;

  /* Create the main loop. */
  loop = g_main_loop_new (loopctx, FALSE);

  /* Create the two NBD handles and nbdkit instances. */
  src = nbd_create ();
  if (!src) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  dest = nbd_create ();
  if (!dest) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Create the GSource main loop sources from each handle. */
  gssrc = create_libnbd_gsource (src);
  gsdest = create_libnbd_gsource (dest);
  loopctx = g_main_loop_get_context (loop);
  g_source_attach ((GSource *) gssrc, loopctx);
  g_source_attach ((GSource *) gsdest, loopctx);

  /* Make sure we get called back when each handle connects. */
  gssrc->connected_callback = connected;
  gsdest->connected_callback = connected;

  /* Asynchronously start each handle connecting. */
  if (nbd_aio_connect_command (src, (char **) src_args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_connect_command (dest, (char **) dest_args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Run the main loop until quit. */
  g_main_loop_run (loop);
  exit (EXIT_SUCCESS);
}

/* This is called back when either handle becomes connected.  By
 * counting the number of times this happens (there are two handles)
 * we can tell when both handles have finished connecting.
 */
static void
connected (struct NBDSource *source)
{
  static int count = 0;

  count++;
  if (count == 2) {
    DEBUG (source, "both handles are connected");

    /* Now that both handles are connected, we can begin copying.
     * Register an idle handler that will repeatedly read from the
     * source.
     */
    g_idle_add (read_data, NULL);
  }
}

/* This idle callback reads data from the source nbdkit until the ring
 * is full.
 */
static gboolean
read_data (gpointer user_data)
{
  static uint64_t posn = 0;
  size_t i;

  if (gssrc == NULL)
    return FALSE;

  /* Finished reading from the source nbdkit? */
  if (posn >= SIZE) {
    DEBUG (gssrc, "read_data: finished reading from source");
    finished = true;
    return FALSE;
  }

  /* Find a free buffer. */
  for (i = 0; i < MAX_BUFFERS; ++i)
    if (buffers[i].state == BUFFER_UNUSED)
      goto found;

  /* If too many read requests are in flight, return FALSE so this
   * idle callback is unregistered.  It will be registered by the
   * write callback when nr_buffers decreases.
   */
  assert (nr_buffers == MAX_BUFFERS);
  DEBUG (gssrc, "read_data: buffer full, pausing reads from source");
  reader_paused = true;
  return FALSE;

 found:
  /* Begin reading into the new buffer. */
  assert (buffers[i].data == NULL);
  buffers[i].data = g_new (char, BUFFER_SIZE);
  buffers[i].state = BUFFER_READING;
  buffers[i].offset = posn;
  nr_buffers++;
  posn += BUFFER_SIZE;

  if (nbd_aio_pread (gssrc->nbd, buffers[i].data,
                     BUFFER_SIZE, buffers[i].offset,
                     (nbd_completion_callback) { .callback = finished_read, .user_data = &buffers[i] },
                     0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  return TRUE;
}

/* This callback is called from libnbd when any read command finishes. */
static int
finished_read (void *vp, int *error)
{
  struct buffer *buffer = vp;

  if (gssrc == NULL)
    return 0;

  DEBUG (gssrc, "finished_read: read completed");

  assert (buffer->state == BUFFER_READING);
  buffer->state = BUFFER_READ_COMPLETED;

  /* Create a writer idle handler. */
  g_idle_add (write_data, buffer);

  return 1;
}

/* This idle callback schedules a write. */
static gboolean
write_data (gpointer user_data)
{
  struct buffer *buffer = user_data;

  if (gsdest == NULL)
    return FALSE;

  assert (buffer->state == BUFFER_READ_COMPLETED);
  buffer->state = BUFFER_WRITING;
  if (nbd_aio_pwrite (gsdest->nbd, buffer->data,
                      BUFFER_SIZE, buffer->offset,
                      (nbd_completion_callback) { .callback = finished_write, .user_data = buffer },
                      0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* We always unregister this idle handler because the read side
   * creates a new idle handler for every buffer that has to be
   * written.
   */
  return FALSE;
}

/* This callback is called from libnbd when any write command finishes. */
static int
finished_write (void *vp, int *error)
{
  struct buffer *buffer = vp;

  if (gsdest == NULL)
    return 0;

  DEBUG (gsdest, "finished_write: write completed");

  assert (buffer->state == BUFFER_WRITING);
  g_free (buffer->data);
  buffer->data = NULL;
  buffer->state = BUFFER_UNUSED;
  nr_buffers--;

  /* If the number of buffers was MAX_BUFFERS and has now gone down to
   * MAX_BUFFERS-1 then we need to restart the read handler.
   */
  if (nr_buffers == MAX_BUFFERS-1 && reader_paused) {
    DEBUG (gsdest, "finished_write: restarting reader");
    g_idle_add (read_data, NULL);
    reader_paused = false;
  }

  /* If the reader has finished and there are no more buffers then we
   * have done.
   */
  if (finished && nr_buffers == 0) {
    DEBUG (gsdest, "finished_write: all finished");
    g_source_remove (g_source_get_id ((GSource *) gssrc));
    g_source_unref ((GSource *) gssrc);
    gssrc = NULL;
    g_source_remove (g_source_get_id ((GSource *) gsdest));
    g_source_unref ((GSource *) gsdest);
    gsdest = NULL;
    g_main_loop_quit (loop);
  }

  return 1;
}
