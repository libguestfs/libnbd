/* NBD client library in userspace
 * Copyright (C) 2013-2022 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

#ifdef HAVE_STDATOMIC_H
#include <stdatomic.h>
#else
/* Rely on ints being atomic enough on the platform. */
#define _Atomic /**/
#endif

#include <limits.h>
#include <ublksrv.h>
#include <ublksrv_aio.h>

#include <libnbd.h>

#include "ispowerof2.h"
#include "vector.h"

#include "nbdublk.h"

/* Thread model:
 *
 * There are two threads per NBD connection.  One thread
 * ('io_uring_thread') handles the io_uring traffic.  The other thread
 * ('nbd_work_thread') handles the NBD asynchronous commands for that
 * connection.
 *
 * The thread_info entry is shared between each pair of threads.
 */
struct thread_info {
  const struct ublksrv_dev *dev;
  size_t i;                     /* index into nbd.ptr[], also q_id */
  pthread_t io_uring_thread;
  pthread_t nbd_work_thread;

  struct ublksrv_aio_ctx *aio_ctx;
  struct ublksrv_aio_list compl;
};
DEFINE_VECTOR_TYPE (thread_infos, struct thread_info)
static thread_infos thread_info;

static pthread_barrier_t barrier;

static char jbuf[4096];
static pthread_mutex_t jbuf_lock = PTHREAD_MUTEX_INITIALIZER;

/* Command completion callback (called on the NBD thread). */
static int
command_completed (void *vpdata, int *error)
{
  struct ublksrv_aio *req = vpdata;
  int q_id = ublksrv_aio_qid (req->id);
  struct ublksrv_aio_list *compl = &thread_info.ptr[q_id].compl;

  if (verbose)
    fprintf (stderr,
             "%s: command_completed: tag=%d q_id=%u error=%d\n",
             "nbdublk", ublksrv_aio_tag (req->id),
	     ublksrv_aio_qid (req->id), *error);

  /* If the command failed, override the normal result. */
  if (*error != 0)
    req->res = *error;

  pthread_spin_lock (&compl->lock);
  aio_list_add (&compl->list, req);
  pthread_spin_unlock (&compl->lock);

  return 1;
}

static int
aio_submitter (struct ublksrv_aio_ctx *ctx, struct ublksrv_aio *req)
{
  const struct ublksrv_io_desc *iod = &req->io;
  const unsigned op = ublksrv_get_op (iod);
  const unsigned flags = ublksrv_get_flags (iod);
  const bool fua = flags & UBLK_IO_F_FUA;
  const bool alloc_zero = flags & UBLK_IO_F_NOUNMAP; /* else punch hole */
  const size_t q_id = ublksrv_aio_qid (req->id); /* also NBD handle number */
  struct nbd_handle *h = nbd.ptr[q_id];
  uint32_t nbd_flags = 0;
  int64_t r;
  nbd_completion_callback cb;

  if (verbose)
    fprintf (stderr, "%s: handle_io_async: tag = %d q_id = %zu\n",
             "nbdublk", ublksrv_aio_tag (req->id), q_id);

  req->res = iod->nr_sectors << 9;
  cb.callback = command_completed;
  cb.user_data = req;
  cb.free = NULL;

  switch (op) {
  case UBLK_IO_OP_READ:
    r = nbd_aio_pread (h, (void *) iod->addr, iod->nr_sectors << 9,
                       iod->start_sector << 9, cb, 0);
    if (r == -1) {
      fprintf (stderr, "%s: %s\n", "nbdublk", nbd_get_error ());
      return -EINVAL;
    }
    break;

  case UBLK_IO_OP_WRITE:
    if (fua && can_fua)
      nbd_flags |= LIBNBD_CMD_FLAG_FUA;

    r = nbd_aio_pwrite (h, (const void *) iod->addr, iod->nr_sectors << 9,
                        iod->start_sector << 9, cb, nbd_flags);
    if (r == -1) {
      fprintf (stderr, "%s: %s\n", "nbdublk", nbd_get_error ());
      return -EINVAL;
    }
    break;

  case UBLK_IO_OP_FLUSH:
    r = nbd_aio_flush (h, cb, 0);
    if (r == -1) {
      fprintf (stderr, "%s: %s\n", "nbdublk", nbd_get_error ());
      return -EINVAL;
    }
    break;

  case UBLK_IO_OP_DISCARD:
    if (fua && can_fua)
      nbd_flags |= LIBNBD_CMD_FLAG_FUA;

    r = nbd_aio_trim (h, iod->nr_sectors << 9, iod->start_sector << 9,
                      cb, nbd_flags);
    if (r == -1) {
      fprintf (stderr, "%s: %s\n", "nbdublk", nbd_get_error ());
      return -EINVAL;
    }
    break;

  case UBLK_IO_OP_WRITE_ZEROES:
    if (fua && can_fua)
      nbd_flags |= LIBNBD_CMD_FLAG_FUA;

    if (alloc_zero)
      nbd_flags |= LIBNBD_CMD_FLAG_NO_HOLE;

    r = nbd_aio_zero (h, iod->nr_sectors << 9, iod->start_sector << 9,
                      cb, nbd_flags);
    if (r == -1) {
      fprintf (stderr, "%s: %s\n", "nbdublk", nbd_get_error ());
      return -EINVAL;
    }
    break;

  default:
    fprintf (stderr, "%s: unknown operation %u\n", "nbdublk", op);
    return -ENOTSUP;
  }

  /* We would return 1 if the request was completed, but that doesn't
   * happen in any case above.
   */
  return 0;
}

static void *
nbd_work_thread (void *vpinfo)
{
  struct thread_info *ti = vpinfo;
  struct nbd_handle *h = nbd.ptr[ti->i];
  struct ublksrv_aio_ctx *aio_ctx = thread_info.ptr[ti->i].aio_ctx;
  struct ublksrv_aio_list *c = &thread_info.ptr[ti->i].compl;

  /* Signal to the main thread that we have initialized. */
  pthread_barrier_wait (&barrier);

  while (!ublksrv_aio_ctx_dead (aio_ctx)) {
    struct aio_list compl;

    aio_list_init (&compl);
    ublksrv_aio_submit_worker (aio_ctx, aio_submitter, &compl);

    pthread_spin_lock (&c->lock);
    aio_list_splice (&c->list, &compl);
    pthread_spin_unlock (&c->lock);

    ublksrv_aio_complete_worker (aio_ctx, &compl);

    if (nbd_poll2 (h, ublksrv_aio_get_efd (aio_ctx), -1) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  return NULL;
}

static void *
io_uring_thread (void *vpinfo)
{
  struct thread_info *thread_info = vpinfo;
  const struct ublksrv_dev *dev = thread_info->dev;
  const struct ublksrv_ctrl_dev *cdev = ublksrv_get_ctrl_dev (dev);
  const struct ublksrv_ctrl_dev_info *dinfo = ublksrv_ctrl_get_dev_info (cdev);
  const unsigned dev_id = dinfo->dev_id;
  const size_t q_id = thread_info->i;
  const struct ublksrv_queue *q;
  int r;
  int tid = gettid ();

  pthread_mutex_lock (&jbuf_lock);
  ublksrv_json_write_queue_info (cdev, jbuf, sizeof jbuf, q_id, tid);
  pthread_mutex_unlock (&jbuf_lock);

  q = ublksrv_queue_init (dev, q_id, NULL);
  if (!q) {
    perror ("ublksrv_queue_init");
    return NULL;
  }

  if (verbose)
    fprintf (stderr, "%s: ublk tid %d dev %d queue %d started\n",
             "nbdublk", tid, dev_id, q->q_id);

  for (;;) {
    r = ublksrv_process_io (q);
    if (r < 0) {
      if (r != -ENODEV) { /* ENODEV is expected when the device is deleted */
        errno = -r;
        perror ("ublksrv_process_io");
      }
      break;
    }
  }

  if (verbose)
    fprintf (stderr, "%s: ublk tid %d dev %d queue %d exited\n",
             "nbdublk", tid, dev_id, q->q_id);

  ublksrv_queue_deinit (q);
  return NULL;
}

static int
set_parameters (struct ublksrv_ctrl_dev *ctrl_dev,
                const struct ublksrv_dev *dev)
{
  const struct ublksrv_ctrl_dev_info *dinfo = ublksrv_ctrl_get_dev_info (ctrl_dev);
  const unsigned attrs =
    (readonly ? UBLK_ATTR_READ_ONLY : 0) |
    (rotational ? UBLK_ATTR_ROTATIONAL : 0) |
    (can_fua ? UBLK_ATTR_FUA : 0);
  struct ublk_params p = {
    .types = UBLK_PARAM_TYPE_BASIC,
    .basic = {
      .attrs                = attrs,
      .logical_bs_shift     = 9,
      .physical_bs_shift    = 9,
      .io_opt_shift         = log_2_bits (pref_block_size),
      .io_min_shift         = log_2_bits (min_block_size),
      .max_sectors          = dinfo->max_io_buf_bytes >> 9,
      .dev_sectors          = dev->tgt.dev_size >> 9,
    },
    .discard = {
      .max_discard_sectors  = UINT_MAX >> 9,
      .max_discard_segments = 1,
    },
  };
  int r;

  pthread_mutex_lock (&jbuf_lock);
  ublksrv_json_write_params (&p, jbuf, sizeof jbuf);
  pthread_mutex_unlock (&jbuf_lock);

  r = ublksrv_ctrl_set_params (ctrl_dev, &p);
  if (r < 0) {
    errno = -r;
    perror ("ublksrv_ctrl_set_params");
    return -1;
  }

  return 0;
}

int
start_daemon (struct ublksrv_ctrl_dev *ctrl_dev)
{
  const struct ublksrv_ctrl_dev_info *dinfo = ublksrv_ctrl_get_dev_info (ctrl_dev);
  const struct ublksrv_dev *dev;
  size_t i;
  int r;

  assert (dinfo->nr_hw_queues == connections);
  assert (nbd.len == connections);

  if (verbose)
    fprintf (stderr, "%s: starting daemon\n", "nbdublk");

  /* This barrier is used to ensure all NBD work threads have started
   * up before we proceed to start the device.
   */
  r = pthread_barrier_init (&barrier, NULL, nbd.len + 1);
  if (r != 0) {
    errno = r;
    perror ("nbdublk: pthread_barrier_init");
    return -1;
  }

  /* Reserve space for the thread_info. */
  if (thread_infos_reserve (&thread_info, nbd.len) == -1) {
    perror ("realloc");
    return -1;
  }

  r = ublksrv_ctrl_get_affinity (ctrl_dev);
  if (r < 0) {
    errno = r;
    perror ("ublksrv_ctrl_get_affinity");
    return -1;
  }

  dev = ublksrv_dev_init (ctrl_dev);
  if (!dev) {
    /* Annoyingly libublksrv logs some not very useful information to
     * syslog when this fails.
     */
    fprintf (stderr, "%s: ublksrv_dev_init failed: "
             "there may be more information in syslog\n",
             "nbdublk");
    return -1;
  }

  /* Create the threads. */
  for (i = 0; i < nbd.len; ++i) {
    /* Note this cannot fail because of previous reserve. */
    thread_infos_append (&thread_info,
                         (struct thread_info)
                         { .dev = dev, .i = i,});

    thread_info.ptr[i].aio_ctx = ublksrv_aio_ctx_init (dev, 0);
    if (!thread_info.ptr[i].aio_ctx) {
      perror ("ublksrv_aio_ctx_init");
      return -1;
    }
    ublksrv_aio_init_list (&thread_info.ptr[i].compl);

    r = pthread_create (&thread_info.ptr[i].io_uring_thread, NULL,
                        io_uring_thread, &thread_info.ptr[i]);
    if (r != 0)
      goto bad_pthread;
    r = pthread_create (&thread_info.ptr[i].nbd_work_thread, NULL,
                        nbd_work_thread, &thread_info.ptr[i]);
    if (r != 0) {
    bad_pthread:
      errno = r;
      perror ("nbdublk: pthread");
      ublksrv_dev_deinit (dev);
      return -1;
    }
  }

  /* Wait on the barrier to ensure all NBD work threads are up. */
  pthread_barrier_wait (&barrier);
  pthread_barrier_destroy (&barrier);

  if (set_parameters (ctrl_dev, dev) == -1) {
    ublksrv_dev_deinit (dev);
    return -1;
  }

  /* Start the device. */
  r = ublksrv_ctrl_start_dev (ctrl_dev, getpid ());
  if (r < 0) {
    errno = -r;
    perror ("ublksrv_ctrl_start_dev");
    ublksrv_dev_deinit (dev);
    return -1;
  }

  ublksrv_ctrl_get_info (ctrl_dev);
  ublksrv_ctrl_dump (ctrl_dev, jbuf);

  /* Wait for io_uring threads to exit. */
  for (i = 0; i < nbd.len; ++i)
    pthread_join (thread_info.ptr[i].io_uring_thread, NULL);

  for (i = 0; i < nbd.len; ++i) {
    ublksrv_aio_ctx_shutdown (thread_info.ptr[i].aio_ctx);
    pthread_join (thread_info.ptr[i].nbd_work_thread, NULL);
    ublksrv_aio_ctx_deinit (thread_info.ptr[i].aio_ctx);
  }

  ublksrv_dev_deinit (dev);
  //thread_infos_reset (&thread_info);
  return 0;
}

static int
init_tgt (struct ublksrv_dev *dev, int type, int argc, char *argv[])
{
  const struct ublksrv_ctrl_dev *cdev = ublksrv_get_ctrl_dev (dev);
  const struct ublksrv_ctrl_dev_info *info = ublksrv_ctrl_get_dev_info (cdev);
  struct ublksrv_tgt_info *tgt = &dev->tgt;
  struct ublksrv_tgt_base_json tgt_json = {
    .type = type,
    .name = "nbd",
  };

  if (verbose)
    fprintf (stderr, "%s: init_tgt: type = %d\n", "nbdublk", type);

  if (type != UBLKSRV_TGT_TYPE_NBD)
    return -1;

  tgt_json.dev_size = tgt->dev_size = size;
  tgt->tgt_ring_depth = info->queue_depth;
  tgt->nr_fds = 0;

  ublksrv_json_write_dev_info (ublksrv_get_ctrl_dev (dev), jbuf, sizeof jbuf);
  ublksrv_json_write_target_base_info (jbuf, sizeof jbuf, &tgt_json);

  return 0;
}

static void
handle_event (const struct ublksrv_queue *q)
{
  struct ublksrv_aio_ctx *aio_ctx = thread_info.ptr[q->q_id].aio_ctx;

  if (verbose)
    fprintf (stderr, "%s: handle_event: q_id = %d\n",
             "nbdublk", q->q_id);

  ublksrv_aio_handle_event (aio_ctx, q);
}

static int
handle_io_async (const struct ublksrv_queue *q, const struct ublk_io_data *io)
{
  struct ublksrv_aio_ctx *aio_ctx = thread_info.ptr[q->q_id].aio_ctx;
  const struct ublksrv_io_desc *iod = io->iod;
  struct ublksrv_aio *req = ublksrv_aio_alloc_req (aio_ctx, 0);

  req->io = *iod;
  req->id = ublksrv_aio_pid_tag (q->q_id, io->tag);
  if (verbose)
    fprintf (stderr, "%s: qid %d tag %d\n", "nbdublk", q->q_id, io->tag);
  ublksrv_aio_submit_req (aio_ctx, q, req);

  return 0;
}

struct ublksrv_tgt_type tgt_type = {
  .type = UBLKSRV_TGT_TYPE_NBD,
  .name = "nbd",
  .init_tgt = init_tgt,
  .handle_io_async = handle_io_async,
  .handle_event = handle_event,
};
