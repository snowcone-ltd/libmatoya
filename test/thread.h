// Copyright (c) Ronald Huveneers <ronald@huuf.info>
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define test_thread_count 100

struct test_threadpool_data {
	MTY_Cond *cond;
	MTY_Mutex *mutex;
	MTY_ThreadPool *pool;
	MTY_Atomic32 atomic_32;
	MTY_Atomic32 atomic_32_detach;
};

static void test_threadpools_thread(void *opaque)
{
	struct test_threadpool_data *data = (struct test_threadpool_data *)opaque;
	MTY_MutexLock(data->mutex);
	MTY_CondWait(data->cond, data->mutex, 50000);
	MTY_MutexUnlock(data->mutex);
	MTY_Atomic32Add(&data->atomic_32, 1);
}

static void test_threadpools_detach(void *opaque)
{
	struct test_threadpool_data *data = (struct test_threadpool_data *)opaque;
	MTY_Atomic32Add(&data->atomic_32_detach, 1);
}

static bool test_threadpools()
{
	struct test_threadpool_data data = {0};
	data.cond = MTY_CondCreate();
	data.mutex = MTY_MutexCreate();
	data.pool = MTY_ThreadPoolCreate(test_thread_count);

	test_cmp("MTY_ThreadPoolCreate", data.pool != NULL);

	for (int32_t i = 0; i < test_thread_count; i++)
		MTY_ThreadPoolDispatch(data.pool, test_threadpools_thread, &data);

	MTY_Sleep(500);
	MTY_MutexLock(data.mutex);
	MTY_CondSignalAll(data.cond);
	MTY_MutexUnlock(data.mutex);
	MTY_Sleep(150);

	test_cmp("MTY_Atomic32Get", MTY_Atomic32Get(&data.atomic_32) == test_thread_count);

	MTY_ThreadPoolDestroy(&data.pool, test_threadpools_detach);
	test_cmp("MTY_ThreadPoolDestroy", data.pool == NULL);

	MTY_CondDestroy(&data.cond);
	MTY_MutexDestroy(&data.mutex);

	test_cmp("MTY_Atomic32Get", MTY_Atomic32Get(&data.atomic_32_detach) == test_thread_count);

	return true;
}

struct test_rw_lock_data {
	int32_t counter;
	MTY_Cond *cond;
	MTY_Mutex *mutex;
	MTY_Atomic32 atomic_32;
	MTY_RWLock *rw_lock;
};

static void *test_thread_rw_lock_write(void *opaque)
{
	struct test_rw_lock_data *data = (struct test_rw_lock_data *)opaque;
	MTY_MutexLock(data->mutex);
	MTY_CondWait(data->cond, data->mutex, 50000);
	MTY_MutexUnlock(data->mutex);
	MTY_RWLockWriter(data->rw_lock);
	int32_t i = data->counter;
	MTY_Sleep(5);
	data->counter = i + 1;
	MTY_RWLockUnlock(data->rw_lock);

	return NULL;
}

static void *test_thread_rw_lock_read(void *opaque)
{
	struct test_rw_lock_data *data = (struct test_rw_lock_data *)opaque;
	MTY_MutexLock(data->mutex);
	MTY_CondWait(data->cond, data->mutex, 50000);
	MTY_MutexUnlock(data->mutex);
	MTY_RWLockWriter(data->rw_lock);
	int32_t i = data->counter;
	MTY_Sleep(5);
	data->counter = i + 1;
	MTY_RWLockUnlock(data->rw_lock);

	return NULL;
}

static bool test_rw_locks()
{
	struct test_rw_lock_data data = {0};
	data.cond = MTY_CondCreate();
	data.mutex = MTY_MutexCreate();
	data.rw_lock = MTY_RWLockCreate();
	MTY_Atomic32Set(&data.atomic_32, 0);
	test_cmp("MTY_RWLockCreate", data.rw_lock != NULL);

	MTY_RWLockWriter(data.rw_lock);
	MTY_Thread **t_test = calloc(test_thread_count, sizeof(MTY_Thread *));
	for (int32_t i = 0; i < test_thread_count; i++) {
		t_test[i] = MTY_ThreadCreate(test_thread_rw_lock_write, &data);
	}

	test_cmp("MTY_RWLock write counter", data.counter == 0);
	MTY_Sleep(100);
	MTY_MutexLock(data.mutex);
	MTY_CondSignalAll(data.cond);
	MTY_MutexUnlock(data.mutex);
	MTY_Sleep(10);
	test_cmp("MTY_RWLock write counter", data.counter == 0);
	MTY_RWLockUnlock(data.rw_lock);

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_ThreadDestroy(&t_test[i]);
	}

	test_cmp("MTY_RWLock write counter", data.counter == test_thread_count);

	data.counter = 0;
	MTY_RWLockWriter(data.rw_lock);
	for (int32_t i = 0; i < test_thread_count; i++) {
		t_test[i] = MTY_ThreadCreate(test_thread_rw_lock_read, &data);
	}

	test_cmp_("MTY_RWLock read counter", data.counter == 0, data.counter, ": %d");
	MTY_Sleep(100);
	MTY_MutexLock(data.mutex);
	MTY_CondSignalAll(data.cond);
	MTY_MutexUnlock(data.mutex);
	MTY_Sleep(10);
	test_cmp_("MTY_RWLock read counter", data.counter == 0, data.counter, ": %d");
	MTY_RWLockUnlock(data.rw_lock);
	MTY_Sleep(100);

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_ThreadDestroy(&t_test[i]);
	}

	test_cmp("MTY_RWLock counter", data.counter == test_thread_count);
	test_cmp("MTY_RWTryLockReader", MTY_RWTryLockReader(data.rw_lock));
	//Double locking should succeed
	test_cmp("MTY_RWTryLockReader", MTY_RWTryLockReader(data.rw_lock));
	MTY_RWLockUnlock(data.rw_lock);
	MTY_RWLockUnlock(data.rw_lock);
	test_cmp("MTY_RWTryLockReader", MTY_RWTryLockReader(data.rw_lock));
	MTY_RWLockUnlock(data.rw_lock);

	MTY_Sleep(10);

	MTY_RWLockDestroy(&data.rw_lock);
	test_cmp("MTY_RWLockDestroy", data.rw_lock == NULL);

	MTY_CondDestroy(&data.cond);
	MTY_MutexDestroy(&data.mutex);

	return true;
}

struct test_waitable_data {
	MTY_Waitable *wait;
	MTY_Atomic32 atomic_32;
	MTY_Atomic64 atomic_64;
};

static void *test_thread_waitable_64(void *opaque)
{
	struct test_waitable_data *data = (struct test_waitable_data *)opaque;
	MTY_WaitableWait(data->wait, 50000);
	MTY_Atomic64Add(&data->atomic_64, 1);

	return NULL;
}

static void *test_thread_waitable_32(void *opaque)
{
	struct test_waitable_data *data = (struct test_waitable_data *)opaque;
	MTY_WaitableWait(data->wait, 50000);
	MTY_Atomic32Add(&data->atomic_32, 1);

	return NULL;
}

static bool test_waitables()
{
	struct test_waitable_data data = {0};

	data.wait = MTY_WaitableCreate();
	test_cmp("MTY_WaitableCreate", data.wait != NULL);
	MTY_Atomic32Set(&data.atomic_32, 0);

	MTY_Thread **t_test = calloc(test_thread_count, sizeof(MTY_Thread *));
	for (int32_t i = 0; i < test_thread_count; i++) {
		t_test[i] = MTY_ThreadCreate(test_thread_waitable_32, &data);
	}

	MTY_Sleep(500);

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_WaitableSignal(data.wait);
		MTY_Sleep(5);
		if (MTY_Atomic32Get(&data.atomic_32) != i + 1)
			test_cmp("MTY_Atomic32Get", MTY_Atomic32Get(&data.atomic_32) == i + 1);
	}

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_ThreadDestroy(&t_test[i]);
	}

	test_cmp("MTY_Atomic32Get", MTY_Atomic32Get(&data.atomic_32) == test_thread_count);

	MTY_Atomic64Set(&data.atomic_64, 0);

	for (int32_t i = 0; i < test_thread_count; i++) {
		t_test[i] = MTY_ThreadCreate(test_thread_waitable_64, &data);
	}

	MTY_Sleep(500);

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_WaitableSignal(data.wait);
		MTY_Sleep(5);
		if (MTY_Atomic64Get(&data.atomic_64) != i + 1)
			test_cmp("MTY_Atomic64Get", MTY_Atomic64Get(&data.atomic_64) == (i + 1));
	}

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_ThreadDestroy(&t_test[i]);
	}

	MTY_WaitableDestroy(&data.wait);
	test_cmp("MTY_WaitableDestroy", data.wait == NULL);
	free(t_test);

	return true;
}

struct test_cond_data {
	int32_t counter;
	MTY_Mutex *mutex;
	MTY_Cond *cond;
};

static void *test_thread_cond_1(void *opaque)
{
	struct test_cond_data *data = (struct test_cond_data *)opaque;

	MTY_MutexLock(data->mutex);
	MTY_CondWait(data->cond, data->mutex, 5000);
	int32_t c = data->counter;
	MTY_Sleep(5);
	data->counter = c + 1;
	MTY_MutexUnlock(data->mutex);

	return NULL;
}

static void *test_thread_cond_2(void *opaque)
{
	struct test_cond_data *data = (struct test_cond_data *)opaque;

	MTY_MutexLock(data->mutex);

	if (MTY_CondWait(data->cond, data->mutex, 5000))
		data->counter++;

	MTY_MutexUnlock(data->mutex);

	return NULL;
}

static bool test_conditions() {
	struct test_cond_data data = {0};

	data.cond = MTY_CondCreate();
	test_cmp("MTY_CondCreate", data.cond != NULL);
	data.mutex = MTY_MutexCreate();

	MTY_Thread **t_test = calloc(test_thread_count, sizeof(MTY_Thread *));
	for (int32_t i = 0; i < test_thread_count; i++) {
		t_test[i] = MTY_ThreadCreate(test_thread_cond_1, &data);
	}

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_MutexLock(data.mutex);
		MTY_CondSignal(data.cond);
		MTY_MutexUnlock(data.mutex);
	}

	MTY_Sleep(50);

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_ThreadDestroy(&t_test[i]);
	}

	test_cmp("MTY_Mutex counter", data.counter == test_thread_count);

	data.counter = 0;
	for (int32_t i = 0; i < test_thread_count; i++) {
		t_test[i] = MTY_ThreadCreate(test_thread_cond_2, &data);
	}

	MTY_Sleep(50);

	MTY_MutexLock(data.mutex);
	MTY_CondSignalAll(data.cond);
	MTY_MutexUnlock(data.mutex);

	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_ThreadDestroy(&t_test[i]);
	}

	test_cmp("MTY_Mutex counter", data.counter == test_thread_count);


	MTY_CondDestroy(&data.cond);
	test_cmp("MTY_CondDestroy", data.cond == NULL);
	MTY_MutexDestroy(&data.mutex);

	free(t_test);
	return true;

}

struct test_mutex_data {
	int32_t counter;
	MTY_Mutex *mutex;
};

static void *test_thread_mutex_2(void *opaque)
{
	struct test_mutex_data *data = (struct test_mutex_data *)opaque;
	MTY_MutexLock(data->mutex);
	MTY_Sleep(100);
	MTY_MutexUnlock(data->mutex);

	return NULL;
}

static void *test_thread_mutex_1(void *opaque)
{
	struct test_mutex_data *data = (struct test_mutex_data *)opaque;

	MTY_MutexLock(data->mutex);
	int32_t i = data->counter;
	MTY_Sleep(5);
	data->counter = i + 1;
	MTY_MutexUnlock(data->mutex);

	return NULL;
}

static bool test_mutexes() {
	struct test_mutex_data data = {0};

	data.mutex = MTY_MutexCreate();
	test_cmp("MTY_MutexCreate", data.mutex != NULL);

	MTY_MutexLock(data.mutex);

	MTY_Thread **t_test = calloc(test_thread_count, sizeof(MTY_Thread *));
	for (int32_t i = 0; i < test_thread_count; i++) {
		t_test[i] = MTY_ThreadCreate(test_thread_mutex_1, &data);
	}

	MTY_Sleep(10);
	test_cmp("MTY_Mutex counter", data.counter == 0);

	MTY_MutexUnlock(data.mutex);
	for (int32_t i = 0; i < test_thread_count; i++) {
		MTY_ThreadDestroy(&t_test[i]);
	}

	test_cmp("MTY_Mutex counter", data.counter == test_thread_count);

	test_cmp("MTY_MutexTryLock", MTY_MutexTryLock(data.mutex));
	MTY_MutexUnlock(data.mutex);
	test_cmp("MTY_MutexTryLock", MTY_MutexTryLock(data.mutex));

	t_test[0] = MTY_ThreadCreate(test_thread_mutex_2, &data);
	MTY_Sleep(10);
	MTY_MutexUnlock(data.mutex);
	MTY_Sleep(10);
	MTY_ThreadDestroy(&t_test[0]);

	MTY_MutexDestroy(&data.mutex);
	test_cmp("MTY_MutexDestroy", data.mutex == NULL);

	free(t_test);
	return true;
}

struct test_thread_data {
	bool executed;
};

static void *test_thread_1(void *opaque)
{
	struct test_thread_data *data = (struct test_thread_data *)opaque;

	//Wait for 100 ms before setting the variable, so we can make sure MTY_ThreadDestroy is actually waiting
	MTY_Sleep(100);

	data->executed = true;

	return NULL;
}

static bool test_thread_creation()
{
	MTY_Thread *t_test = NULL;

	struct test_thread_data data = {0};

	t_test = MTY_ThreadCreate(test_thread_1, &data);
	test_cmp("MTY_ThreadCreate", t_test != NULL);

	MTY_ThreadDestroy(&t_test);
	test_cmp("MTY_Thread executed", data.executed);
	test_cmp("MTY_ThreadDestroy", t_test == NULL);

	data.executed = false;
	MTY_ThreadDetach(test_thread_1, &data);
	test_cmp("MTY_Thread executed", !data.executed);
	MTY_Sleep(150);
	test_cmp("MTY_Thread executed", data.executed);

	return true;
}

static bool thread_main()
{
	MTY_SetTimerResolution(1);
	if (!test_conditions())
		return false;

	if (!test_mutexes())
		return false;

	if (!test_thread_creation())
		return false;

	if (!test_threadpools())
		return false;

	if (!test_rw_locks())
		return false;

	if (!test_waitables())
		return false;

	MTY_RevertTimerResolution(1);

	return true;
}
