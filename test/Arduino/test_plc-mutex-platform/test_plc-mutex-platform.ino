#include <unity.h>

// test_plc-mutex-platform.c is plain C; declare its test
// functions here (compiled as C++) with C linkage so RUN_TEST can call them.
extern "C" {
void test_plc_mutex_create_returns_a_valid_mutex(void);
void test_plc_mutex_destroy_fails_with_einval_for_null(void);
void test_plc_mutex_destroy_succeeds_for_an_unlocked_mutex(void);
void test_plc_mutex_destroy_fails_with_ebusy_for_a_locked_mutex(void);
void test_plc_mutex_acquire_succeeds_immediately_when_free(void);
void test_plc_mutex_acquire_times_out_when_already_held_by_the_same_thread(
	void);
void test_plc_mutex_acquire_times_out_when_held_by_another_thread(void);
void test_plc_mutex_release_succeeds_when_held(void);
void test_plc_mutex_release_fails_with_ealready_when_not_held(void);
}

void setup()
{
	UNITY_BEGIN();

	RUN_TEST(test_plc_mutex_create_returns_a_valid_mutex);

	RUN_TEST(test_plc_mutex_destroy_fails_with_einval_for_null);
	RUN_TEST(test_plc_mutex_destroy_succeeds_for_an_unlocked_mutex);
	RUN_TEST(test_plc_mutex_destroy_fails_with_ebusy_for_a_locked_mutex);

	RUN_TEST(test_plc_mutex_acquire_succeeds_immediately_when_free);
	RUN_TEST(
		test_plc_mutex_acquire_times_out_when_already_held_by_the_same_thread);
	RUN_TEST(test_plc_mutex_acquire_times_out_when_held_by_another_thread);

	RUN_TEST(test_plc_mutex_release_succeeds_when_held);
	RUN_TEST(test_plc_mutex_release_fails_with_ealready_when_not_held);

	UNITY_END();
}

void loop()
{
}
