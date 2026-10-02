#include <unity.h>

extern "C" {
void test_plc_delay_us_returns_at_once_for_0(void);
void test_plc_delay_us_waits_at_least_the_delay(void);
void test_plc_delay_us_waits_at_least_a_delay_shorter_than_a_tick(void);
void test_plc_delay_us_waits_at_least_the_delay_across_a_second(void);
void test_plc_delay_us_keeps_waiting_after_a_handled_signal(void);
void test_plc_delay_us_lets_a_lower_priority_task_run_for_whole_ticks(void);
void test_plc_time_us_fails_with_efault_for_null(void);
void test_plc_time_us_advances_at_least_by_a_delay(void);
}

void setup()
{
	UNITY_BEGIN();

	RUN_TEST(test_plc_delay_us_returns_at_once_for_0);
	RUN_TEST(test_plc_delay_us_waits_at_least_the_delay);
	RUN_TEST(test_plc_delay_us_waits_at_least_a_delay_shorter_than_a_tick);
	RUN_TEST(test_plc_delay_us_waits_at_least_the_delay_across_a_second);
	RUN_TEST(test_plc_delay_us_keeps_waiting_after_a_handled_signal);
	RUN_TEST(
		test_plc_delay_us_lets_a_lower_priority_task_run_for_whole_ticks);
	RUN_TEST(test_plc_time_us_fails_with_efault_for_null);
	RUN_TEST(test_plc_time_us_advances_at_least_by_a_delay);

	UNITY_END();
}

void loop()
{
}
