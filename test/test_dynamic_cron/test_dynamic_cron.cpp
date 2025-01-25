
// Loading order matters.
// If you load croncpp after ArduinoFake, it will bomb due to the macros & functions overridden by ArduinoFake.
// So ArduinoFake (or Arduino.h) should be loaded after croncpp.
// See https://forum.arduino.cc/t/include-chrono-causes-a-compile-error-in-an-otherwise-empty-skeleton-sketch/1147518/9

#include <unity.h>
#include <croncpp.h>
#include <ArduinoFake.h>
//#include <Preferences.h>
//#include </DynamicCron/esphome/components/dynamic_cron/dynamic_cron.h.bak>
// We'll need to remove all calls to Preferences from this core file.
// Or, we need to mock out all Preferences calls to FS (file system),
// since there is no FS library for linux (it's esp32 only).
//#include </DynamicCron/esphome/components/dynamic_cron/core.h>

String str = "hello";

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_function_should_doBlahAndBlah(void) {
  // test stuff
}

void test_function_should_doAlsoDoBlah(void) {
  // more test stuff
}

int runUnityTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_function_should_doBlahAndBlah);
  RUN_TEST(test_function_should_doAlsoDoBlah);
  return UNITY_END();
}


// YOU CAN REMOVE UNNECESSARY IMPLEMENTATIONS, IF NOT USING THEM //

/**
  * For native dev-platform or for some embedded frameworks
  */
int main(void) {
  return runUnityTests();
}

/**
  * For Arduino framework
  */
void setup() {
  // Wait ~2 seconds before the Unity test runner
  // establishes connection with a board Serial interface
  delay(2000);

  runUnityTests();
}

void loop() {}

