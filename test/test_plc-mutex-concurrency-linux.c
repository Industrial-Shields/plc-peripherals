/*
 * Copyright (c) 2026 Industrial Shields. All rights reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Real-thread stress test for the Linux mutex backend
 * (src/plc-mutex-linux.c).
 */

#include "unity.h"

#include "plc-peripherals-platform.h"
#include "plc-mutex.h"

// No header of its own maps to plc-mutex-linux.c
#if PLC_ENVIRONMENT == PLC_LINUX
TEST_SOURCE_FILE("plc-mutex-linux.c")
#endif

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <malloc.h>

#define NUM_MUTEXES 4
#define NUM_THREADS 8
#define ITERATIONS_PER_THREAD 500
#define LOCK_TIMEOUT_MS 1000

static plc_mutex_t* mutexes[NUM_MUTEXES];

// Per-mutex count of threads currently holding it.
static atomic_int concurrent_holders[NUM_MUTEXES];
static atomic_int max_concurrent_holders_seen[NUM_MUTEXES];

// Set by any thread that gets an unexpected error from lock/unlock. Unity
// assertions aren't safe to call from worker threads, so failures are
// recorded here and asserted on on the main thread only, after every
// thread has been joined.
static atomic_int unexpected_lock_failures;
static atomic_int unexpected_unlock_failures;

void setUp(void)
{
	for (int i = 0; i < NUM_MUTEXES; i++) {
		mutexes[i] = plc_mutex_create();
		TEST_ASSERT_NOT_NULL(mutexes[i]);
		atomic_store(&concurrent_holders[i], 0);
		atomic_store(&max_concurrent_holders_seen[i], 0);
	}

	atomic_store(&unexpected_lock_failures, 0);
	atomic_store(&unexpected_unlock_failures, 0);
}

void tearDown(void)
{
	for (int i = 0; i < NUM_MUTEXES; i++) {
		plc_mutex_destroy(mutexes[i]);
	}
}

static void* hammer_thread(void* arg)
{
	uintptr_t thread_id = (uintptr_t)arg;

	for (int i = 0; i < ITERATIONS_PER_THREAD; i++) {
		int idx = (int)((thread_id + (uintptr_t)i) % NUM_MUTEXES);

		if (plc_mutex_acquire(mutexes[idx], LOCK_TIMEOUT_MS) != 0) {
			atomic_fetch_add(&unexpected_lock_failures, 1);
			continue;
		}

		int holders = atomic_fetch_add(&concurrent_holders[idx], 1) + 1;
		int prev_max = atomic_load(&max_concurrent_holders_seen[idx]);
		while (holders > prev_max &&
		       !atomic_compare_exchange_weak(
			       &max_concurrent_holders_seen[idx],
			       &prev_max,
			       holders)) {
			// Retry: another thread updated the max concurrently
		}

		atomic_fetch_sub(&concurrent_holders[idx], 1);

		if (plc_mutex_release(mutexes[idx]) != 0) {
			atomic_fetch_add(&unexpected_unlock_failures, 1);
		}
	}

	return NULL;
}

void test_concurrent_acquire_release_never_lets_two_threads_hold_one_mutex(void)
{
	pthread_t threads[NUM_THREADS];

	for (uintptr_t i = 0; i < NUM_THREADS; i++) {
		TEST_ASSERT_EQUAL_INT(
			0,
			pthread_create(
				&threads[i], NULL, hammer_thread, (void*)i));
	}

	for (int i = 0; i < NUM_THREADS; i++) {
		TEST_ASSERT_EQUAL_INT(0, pthread_join(threads[i], NULL));
	}

	TEST_ASSERT_EQUAL_INT(0, atomic_load(&unexpected_lock_failures));
	TEST_ASSERT_EQUAL_INT(0, atomic_load(&unexpected_unlock_failures));

	for (int i = 0; i < NUM_MUTEXES; i++) {
		TEST_ASSERT_LESS_OR_EQUAL_INT(
			1, atomic_load(&max_concurrent_holders_seen[i]));
	}
}

#define SHARED_ITERATIONS 200000

typedef struct {
	plc_mutex_t mutex;
	long counter;
} shared_region_t;

static void hammer_region(shared_region_t* region, int* failures)
{
	for (int i = 0; i < SHARED_ITERATIONS; i++) {
		if (plc_mutex_acquire(&region->mutex, LOCK_TIMEOUT_MS) != 0) {
			(*failures)++;
			continue;
		}

		// Not atomic
		region->counter++;

		if (plc_mutex_release(&region->mutex) != 0) {
			(*failures)++;
		}
	}
}

void test_a_shared_mutex_excludes_an_unrelated_process(void)
{
	char name[64];
	snprintf(name, sizeof(name), "/plc-mutex-test-%d", (int)getpid());

	int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);
	TEST_ASSERT_EQUAL_INT(0, ftruncate(fd, sizeof(shared_region_t)));

	shared_region_t* region = mmap(NULL,
				       sizeof(shared_region_t),
				       PROT_READ | PROT_WRITE,
				       MAP_SHARED,
				       fd,
				       0);
	TEST_ASSERT_NOT_EQUAL(MAP_FAILED, region);
	TEST_ASSERT_EQUAL_INT(0, shm_unlink(name));

	region->counter = 0;
	TEST_ASSERT_EQUAL_INT(0,
			      plc_mutex_static_create(&region->mutex,
						      PLC_MUTEX_SCOPE_SHARED));

	/*
	 * Empty stdout first: the child would inherit whatever Unity has
	 * buffered and emit it a second time, so the runner would see one
	 * result more than there are tests.
	 */
	fflush(NULL);

	pid_t pid = fork();
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, pid);

	if (pid == 0) {
		munmap(region, sizeof(shared_region_t));
		shared_region_t* child = mmap(NULL,
					      sizeof(shared_region_t),
					      PROT_READ | PROT_WRITE,
					      MAP_SHARED,
					      fd,
					      0);
		if (child == MAP_FAILED) {
			_exit(1);
		}

		int failures = 0;
		hammer_region(child, &failures);
		munmap(child, sizeof(shared_region_t));

		_exit(failures == 0 ? 0 : 1);
	}

	int parent_failures = 0;
	hammer_region(region, &parent_failures);

	int status = 0;
	TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
	TEST_ASSERT_TRUE(WIFEXITED(status));
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0, WEXITSTATUS(status), "Child hit lock/unlock errors");
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0, parent_failures, "Parent hit lock/unlock errors");

	TEST_ASSERT_EQUAL_INT_MESSAGE(
		2 * SHARED_ITERATIONS,
		region->counter,
		"Updates were lost, so the mutex did not exclude the peer");

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&region->mutex));
	munmap(region, sizeof(shared_region_t));
	close(fd);
}

void test_a_created_mutex_does_not_cross_a_fork(void)
{
	int ready[2];

	plc_mutex_t* private_mutex = plc_mutex_create();
	TEST_ASSERT_NOT_NULL(private_mutex);
	TEST_ASSERT_EQUAL_INT(0, pipe(ready));

	fflush(NULL);
	pid_t pid = fork();
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, pid);

	if (pid == 0) {
		close(ready[0]);
		if (plc_mutex_acquire(private_mutex, LOCK_TIMEOUT_MS) != 0) {
			_exit(1);
		}

		char token = 'x';
		ssize_t written = write(ready[1], &token, 1);
		usleep(300 * 1000);
		plc_mutex_release(private_mutex);
		_exit(written == 1 ? 0 : 1);
	}

	close(ready[1]);
	char token = 0;
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		1, read(ready[0], &token, 1), "Child never took the lock");

	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0,
		plc_mutex_acquire(private_mutex, 100),
		"plc_mutex_create must be process-private, so the child has its own");
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(private_mutex));

	int status = 0;
	TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
	close(ready[0]);
	TEST_ASSERT_TRUE(WIFEXITED(status));
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0, WEXITSTATUS(status), "Child hit lock/unlock errors");
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_destroy(private_mutex));
}

void test_a_private_mutex_does_not_cross_a_fork(void)
{
	int ready[2];

	plc_mutex_t* private_mutex = malloc(sizeof(plc_mutex_t));
	TEST_ASSERT_NOT_NULL(private_mutex);
	TEST_ASSERT_EQUAL_INT(0,
			      plc_mutex_static_create(private_mutex,
						      PLC_MUTEX_SCOPE_PRIVATE));
	TEST_ASSERT_EQUAL_INT(0, pipe(ready));

	fflush(NULL);
	pid_t pid = fork();
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, pid);

	if (pid == 0) {
		close(ready[0]);
		if (plc_mutex_acquire(private_mutex, LOCK_TIMEOUT_MS) != 0) {
			_exit(1);
		}

		char token = 'x';
		ssize_t written = write(ready[1], &token, 1);
		usleep(300 * 1000);
		plc_mutex_release(private_mutex);
		_exit(written == 1 ? 0 : 1);
	}

	close(ready[1]);
	char token = 0;
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		1, read(ready[0], &token, 1), "Child never took the lock");

	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0,
		plc_mutex_acquire(private_mutex, 100),
		"A private mutex must not span processes, so the child has its own");
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(private_mutex));

	int status = 0;
	TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
	close(ready[0]);
	TEST_ASSERT_TRUE(WIFEXITED(status));
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0, WEXITSTATUS(status), "Child hit lock/unlock errors");

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(private_mutex));
	free(private_mutex);
}

void test_acquire_reports_eownerdead_when_a_peer_dies_holding_the_lock(void)
{
	char name[64];
	snprintf(name, sizeof(name), "/plc-mutex-robust-%d", (int)getpid());

	int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);
	TEST_ASSERT_EQUAL_INT(0, ftruncate(fd, sizeof(shared_region_t)));

	shared_region_t* region = mmap(NULL,
				       sizeof(shared_region_t),
				       PROT_READ | PROT_WRITE,
				       MAP_SHARED,
				       fd,
				       0);
	TEST_ASSERT_NOT_EQUAL(MAP_FAILED, region);
	TEST_ASSERT_EQUAL_INT(0, shm_unlink(name));

	region->counter = 0;
	TEST_ASSERT_EQUAL_INT(0,
			      plc_mutex_static_create(&region->mutex,
						      PLC_MUTEX_SCOPE_SHARED));

	fflush(NULL);
	pid_t pid = fork();
	TEST_ASSERT_GREATER_OR_EQUAL_INT(0, pid);

	if (pid == 0) {
		if (plc_mutex_acquire(&region->mutex, LOCK_TIMEOUT_MS) != 0) {
			_exit(1);
		}
		region->counter = 1; // Tell the parent the lock is held
		_exit(0); // Die still holding it
	}

	int status = 0;
	TEST_ASSERT_EQUAL_INT(pid, waitpid(pid, &status, 0));
	TEST_ASSERT_TRUE(WIFEXITED(status));
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		0, WEXITSTATUS(status), "Child never took the lock");
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		1, region->counter, "Child never reached the critical section");

	errno = 0;
	int ret = plc_mutex_acquire(&region->mutex, LOCK_TIMEOUT_MS);
	TEST_ASSERT_EQUAL_INT(-1, ret);
	TEST_ASSERT_EQUAL_INT_MESSAGE(
		EOWNERDEAD,
		errno,
		"A shared mutex must be robust and report EOWNERDEAD");

	// EOWNERDEAD means the lock WAS taken, so it still has to be released.
	TEST_ASSERT_EQUAL_INT(0, plc_mutex_release(&region->mutex));

	TEST_ASSERT_EQUAL_INT(0, plc_mutex_static_destroy(&region->mutex));
	munmap(region, sizeof(shared_region_t));
	close(fd);
}
