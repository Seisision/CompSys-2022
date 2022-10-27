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

#define FILE_BUFFER_SIZE 4096*8

int global_histogram[8] = { 0 };
pthread_mutex_t lock_histogram = PTHREAD_MUTEX_INITIALIZER;

// Each thread will run this function.  The thread argument is a
// pointer to a worker_info which has a needle and a pointer to a job queue.
void* worker(void *arg) {
  struct job_queue *jq = arg;
  char *filename;
  char *buffer = malloc(FILE_BUFFER_SIZE*sizeof(char));
  int pop_result = 0;
  
  while(pop_result >= 0) {
    int i = 0;
    int local_histogram[8] = { 0 };
    pop_result = job_queue_pop(jq, (void**)&filename);
    if(pop_result != -1) {
      // pop successfull we have a file to process
      FILE *file = fopen(filename, "r");
      size_t readBytes = 0;
      // read file in chunks of FILE_BUFFER_SIZE
      do {
        readBytes = fread(buffer, sizeof(char), FILE_BUFFER_SIZE, file);
        // process each byte of chunk
        for (size_t j = 0; j < readBytes; ++j) {
          // increment total byte counter
          i++;
          update_histogram(local_histogram, buffer[j]);
          // only merge and print histogram every 250k bytes
          if ((i % 250000) == 0) {
            // protect shared resources with mutex
            pthread_mutex_lock(&lock_histogram);
            merge_histogram(local_histogram, global_histogram);
            print_histogram(global_histogram); 
            pthread_mutex_unlock(&lock_histogram);
          }
        }
      } 
      while (readBytes > 0);

      // write remaining histogram data
      pthread_mutex_lock(&lock_histogram);
      merge_histogram(local_histogram, global_histogram);
      print_histogram(global_histogram); 
      pthread_mutex_unlock(&lock_histogram);

      fclose(file);
      free(filename);
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

  // Start worker threads
  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, &worker, &jq) != 0) {
      err(1, "pthread_create() failed");
    }
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
 
  move_lines(9);

  return 0;
}
