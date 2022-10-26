// Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#include "job_queue.h"

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include "histogram.h"

struct worker_info {
    struct job_queue *jq;
    struct job_queue *dq;
};

void push_histogram(int *local_histogram, struct job_queue *dq) {
    int *histogram_copy = malloc(8 * sizeof(int));
    memcpy(histogram_copy, local_histogram, 8 * sizeof(int));
    job_queue_push(dq, histogram_copy);
    // reset local histogram
    memset(local_histogram, 0, 8 * sizeof(int));
}

// Each thread will run this function.  The thread argument is a
// pointer to a worker_info which has a needle and a pointer to a job queue.
void* worker(void *arg) {
  struct worker_info *wi = arg;
  struct job_queue *jq = wi->jq;
  struct job_queue *dq = wi->dq;
  char *filename;
  char buffer;
  int pop_result = 0;

  
  while(pop_result >= 0) {
    int i = 0;
    int local_histogram[8] = { 0 };
    pop_result = job_queue_pop(jq, (void**)&filename);
    // pop successfull
    if(pop_result != -1) {
       FILE *file = fopen(filename, "r");
       while (fread(&buffer, sizeof(buffer), 1, file) == 1) {
           i++;
           update_histogram(local_histogram, buffer);
           if ((i % 100000) == 0) {
               push_histogram(local_histogram, dq);
           }
       }
       fclose(file);
       push_histogram(local_histogram, dq);
    }
  }
  return NULL;
}
  
void* display(void *arg) {
    struct job_queue *dq = arg;
    int pop_result = 0;
    int *local_histogram;
    int global_histogram[8] = { 0 };

    while (pop_result >= 0) {
        pop_result = job_queue_pop(dq, (void**)&local_histogram);
        if (pop_result != -1) {
            merge_histogram(local_histogram, global_histogram);
            print_histogram(global_histogram); 
            free(local_histogram);
        }
    }
    return NULL;
}

int main(int argc, char * const *argv) {
  if (argc < 2) {
    err(1, "usage: paths...");
    exit(1);
  }

  int num_threads = 1;
  char * const *paths = &argv[1];

  if (argc > 3 && strcmp(argv[1], "-n") == 0) {
    // Since atoi() simply returns zero on syntax errors, we cannot
    // distinguish between the user entering a zero, or some
    // non-numeric garbage.  In fact, we cannot even tell whether the
    // given option is suffixed by garbage, i.e. '123foo' returns
    // '123'.  A more robust solution would use strtol(), but its
    // interface is more complicated, so here we are.
    num_threads = atoi(argv[2]);

    if (num_threads < 1) {
      err(1, "invalid thread count: %s", argv[2]);
    }

    paths = &argv[3];
  } else {
    paths = &argv[1];
  }

  // set up job queue for workers
  struct job_queue jq;
  job_queue_init(&jq, 128);

  // set up job queue for merge and display
  struct job_queue dq;
  job_queue_init(&dq, 2048);

  // set up worker info
  struct worker_info wi;
  wi.jq = &jq;
  wi.dq = &dq;

  // Start worker threads
  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, &worker, &wi) != 0) {
      err(1, "pthread_create() failed");
    }
  }

  // start display thread
  pthread_t display_thread = 0;
  if (pthread_create(&display_thread, NULL, &display, &dq) != 0) {
    err(1, "pthread_create() failed");
  }

  // FTS_LOGICAL = follow symbolic links
  // FTS_NOCHDIR = do not change the working directory of the process
  //
  // (These are not particularly important distinctions for our simple
  // uses.)
  int fts_options = FTS_LOGICAL | FTS_NOCHDIR;

  FTS *ftsp;
  if ((ftsp = fts_open(paths, fts_options, NULL)) == NULL) {
    err(1, "fts_open() failed");
    return -1;
  }

  FTSENT *p;
  while ((p = fts_read(ftsp)) != NULL) {
    switch (p->fts_info) {
    case FTS_D:
      break;
    case FTS_F:
      // put filename into job queue
      job_queue_push(&jq, strdup(p->fts_path)); 
      break;
    default:
      break;
    }
  }

  fts_close(ftsp);

  // cleanup. Destroy the queue and mutex and shutdown worker threads.
  job_queue_destroy(&jq);

  // Wait for all threads to finish.  This is important, at some may
  // still be working on their job.
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
  }
  
  job_queue_destroy(&dq);

  if (pthread_join(display_thread, NULL) != 0) {
    err(1, "pthread_join() failed");
  }
 
  move_lines(9);

  return 0;
}
