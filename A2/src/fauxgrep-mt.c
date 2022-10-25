 // Setting _DEFAULT_SOURCE is necessary to activate visibility of
// certain header file contents on GNU/Linux systems.
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

// err.h contains various nonstandard BSD extensions, but they are
// very handy.
#include <err.h>

#include <pthread.h>

#include "job_queue.h"

pthread_mutex_t lock_writing_stdout = PTHREAD_MUTEX_INITIALIZER;


struct worker_info {
    struct job_queue *jq;
    char *needle;
};

// Each thread will run this function.  The thread argument is a
// pointer to a worker_info which has a needle and a pointer to a job queue.
void* worker(void *arg) {
  struct worker_info *wi = arg;
  struct job_queue *jq = wi->jq;
  char *filename;
  int pop_result = 0;
  char buffer[65792];
  
  while(pop_result >= 0) {
    pop_result = job_queue_pop(jq, &filename);
    int line_num = 1;
    // pop successfull
    if(pop_result != -1) {
       FILE *file = fopen(filename, "r");
       // read file line for line with fget
       while(1) {
           char *line = fgets(buffer, 65792, file);
           // break at file end
           if (line == 0) {
               break;
           } 
           // needle found
           if(strstr(line, wi->needle) != NULL) {
                // exclusive access when printing to avoid interveawing of prints to stdout
                pthread_mutex_lock(&lock_writing_stdout);
                printf("%s:%d: %s", filename, line_num, line);
                pthread_mutex_unlock(&lock_writing_stdout);
           }
           line_num++;
       }
       fclose(file);
    }
  }
  return NULL;
}

int main(int argc, char * const *argv) {
  if (argc < 2) {
    err(1, "usage: [-n INT] STRING paths...");
    exit(1);
  }

  int num_threads = 1;
  char const *needle = argv[1];
  char * const *paths = &argv[2];


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

    needle = argv[3];
    paths = &argv[4];

  } else {
    needle = argv[1];
    paths = &argv[2];
  }

  // set up job queue
  struct job_queue jq;
  job_queue_init(&jq, 64);

  // Setup worker arguments (we can only pass one void pointer)
  struct worker_info wi;
  wi.jq = &jq;
  wi.needle = needle;

  // Start worker threads
  pthread_t *threads = calloc(num_threads, sizeof(pthread_t));
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&threads[i], NULL, &worker, &wi) != 0) {
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

  // cleanup. Destroy the queue and mutex.
  job_queue_destroy(&jq);

  // Wait for all threads to finish.  This is important, at some may
  // still be working on their job.
  for (int i = 0; i < num_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      err(1, "pthread_join() failed");
    }
    printf("join");
  }
  
  pthread_mutex_destroy(&lock_writing_stdout);


  return 0;
}
