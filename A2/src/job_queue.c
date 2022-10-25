#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "job_queue.h"

int job_queue_init(struct job_queue *job_queue, int capacity) {
	if (job_queue == NULL) {
		// return error
		return -1;
	}

	job_queue->capacity = capacity;
	job_queue->top = 0;
	job_queue->jobs = malloc(sizeof(void*) * capacity);
	job_queue->dead = 0;

	assert(pthread_mutex_init(&(job_queue->lock_has_space), NULL) == 0);
	assert(pthread_mutex_init(&(job_queue->lock_has_job), NULL) == 0);
	assert(pthread_cond_init(&(job_queue->cond_has_space), NULL) == 0);
	assert(pthread_cond_init(&(job_queue->cond_has_job), NULL) == 0);
	return 0;
}

int job_queue_destroy(struct job_queue *job_queue) {
	job_queue->dead = 1;

	// block till queue is empty
	while (job_queue->top != 0) {
		pthread_mutex_lock(&(job_queue->lock_has_space));
		pthread_cond_wait(&(job_queue->cond_has_space), (&job_queue->lock_has_space));
		pthread_mutex_unlock(&(job_queue->lock_has_space));
	}
	// wake up pending pop jobs
	pthread_cond_broadcast(&(job_queue->cond_has_job));

	// clean up
	free(job_queue->jobs);
	pthread_mutex_destroy(&(job_queue->lock_has_space));
	pthread_mutex_destroy(&(job_queue->lock_has_job));
	pthread_cond_destroy(&(job_queue->cond_has_space));
	pthread_cond_destroy(&(job_queue->cond_has_job));
}

int job_queue_push(struct job_queue *job_queue, void *data) {
	if (job_queue->dead == 1) {
		return -1;
	}

	if (job_queue->top == job_queue->capacity) {
		// lock and wait until space is available
		pthread_mutex_lock(&(job_queue->lock_has_space));
		pthread_cond_wait(&(job_queue->cond_has_space), &(job_queue->lock_has_space));
	}

	// push job to queue
	job_queue->jobs[job_queue->top] = data;
	job_queue->top += 1;

	// unlock and signal job is available
	pthread_mutex_unlock(&(job_queue->lock_has_space));
	pthread_cond_signal(&(job_queue->cond_has_job));
	return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data) {
	if (job_queue->top == 0) {
		if (job_queue->dead == 1) {
			return -1;
		}
		// lock and wait until job is available
		pthread_mutex_lock(&(job_queue->lock_has_job));
		pthread_cond_wait(&(job_queue->cond_has_job), &(job_queue->lock_has_job));
	}

	// pop job from queue
	*data = (job_queue->jobs[job_queue->top-1]);
	job_queue->top -= 1;

	// unlock and signal space is available
	pthread_mutex_unlock(&(job_queue->lock_has_job));
	pthread_cond_signal(&(job_queue->cond_has_space));	
	return 0;
}
