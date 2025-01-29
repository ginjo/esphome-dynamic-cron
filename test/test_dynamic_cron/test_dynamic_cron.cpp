// Main c++ test file for dynamic_cron external component.

// Loading order matters.
// If you load croncpp after ArduinoFake, it will bomb due to the macros & functions overridden by ArduinoFake.
// So ArduinoFake (or Arduino.h) should be loaded after croncpp.
// See https://forum.arduino.cc/t/include-chrono-causes-a-compile-error-in-an-otherwise-empty-skeleton-sketch/1147518/9
//
// This is the main test file for the dynamic_cron component.
// These tests use the Unity test framework.
//   https://docs.platformio.org/en/stable/advanced/unit-testing/frameworks/unity.html
//

#ifndef IS_NATIVE
#error "Must define IS_NATIVE in platformio.ini, either 1 or 0"
#endif

#include <chrono>
#include <thread>
#include <unity.h>

#include <croncpp.h>

#if IS_NATIVE == 1
  #include <ArduinoFake.h>
  #include <../esphome/components/dynamic_cron/dynamic_cron.h>
#else
  #include <esphome/core/log.h>
  #include <esphome/core/application.h>
  #include <../esphome/components/dynamic_cron/dynamic_cron_esphome.h>
#endif


namespace esphome {
namespace dynamic_cron {
  
  class ScheduleMock : public ScheduleCore {
  public:
      
    ScheduleMock() :
      ScheduleCore("test-name", "test-id", []() { std::cout << "Test lambda called\n"; return true; })
    {}
    
    // Added this to debug mystery exception, didn't help
    //~ScheduleMock() noexcept(false) {}
  
    bool callLambda() {
      return target_action_fptr();
    }
    
    // Helper method to access protected ScheduleCore::splitString().
    // Returns single member of string vector from splitString() function.
    std::string getStringVectorMember(std::string _string, std::string _regexp, size_t index) {
      std::string result = splitString(_string, _regexp)[index];
      return result;
    }
    
    // Helper method to set cronnext with older time,
    // since the official setCronNext() won't allow it.
    void setCronNextRaw(std::time_t input) {
      cronnext = input;
    }

    void callCronLoop() {
      cronLoop();
    }    
  };

} // esphome {
} // dynamic_cron


// Declards (instantiates?) a ScheduleMock object with default constructor.
esphome::dynamic_cron::ScheduleMock scheduleMockInst;


void setUp(void) {
  // set stuff up here
  
  // Will do this for every test, if we need it.
  //esphome::dynamic_cron::ScheduleMock scheduleMockInst;
}

void tearDown(void) {
  // clean stuff up here
}


// TESTS - Covers most, but not all functions in dynamic_cron.h.
//         Does NOT cover anything in dynamic_cron_esphome.h (yet), as that file
//         requires links to arduino and esphome hardware objects.

void test_schedule_receives_name(void) {
  bool rslt = scheduleMockInst.callLambda();
  TEST_ASSERT_TRUE(scheduleMockInst.getNameString() == "test-name");
}

void test_schedule_receives_id(void) {
  bool rslt = scheduleMockInst.callLambda();
  TEST_ASSERT_TRUE(scheduleMockInst.getIdString() == "test-id");
}

void test_schedule_receives_lambda(void) {
  bool rslt = scheduleMockInst.callLambda();
  TEST_ASSERT_TRUE(rslt);
  //TEST_MESSAGE("HI!!!");
}

void test_schedule_calculates_cronnext(void) {
  scheduleMockInst.setCrontab("1 2 3 * * *");
  std::string cron_next = scheduleMockInst.cronNextString();
  std::string time_only = scheduleMockInst.getStringVectorMember(cron_next, " ", 1);
  //TEST_MESSAGE(cron_next.c_str());
  //TEST_MESSAGE(time_only.c_str());
  TEST_ASSERT_TRUE(time_only == "03:02:01");
}

void test_schedule_contains_schedules(void) {
  // Outputs the Schedules() array size, for debugging.
  //std::cout << esphome::dynamic_cron::ScheduleCore::Schedules().size();
  TEST_ASSERT_TRUE(esphome::dynamic_cron::ScheduleCore::Schedules()[0] == &scheduleMockInst);
}

void test_schedule_cronNextExpired(void) {
  // Default cronnext should be legit.
  TEST_ASSERT_FALSE(scheduleMockInst.cronNextExpired());
  std::time_t old_time = scheduleMockInst.stringToTime("2020-01-01 14:23:45");
  scheduleMockInst.setCronNextRaw(old_time);
  // Old cronnext should be considered expired.
  TEST_ASSERT_TRUE(scheduleMockInst.cronNextExpired());
  scheduleMockInst.setCronNext(old_time);
  // setCronNext(old_time) is not legit user operation and should be filtered out,
  // resulting in legit cronnext.
  TEST_ASSERT_FALSE(scheduleMockInst.cronNextExpired());
}

void test_schedule_cronLoop(void) {
  std::time_t time1 = scheduleMockInst.getCronNext();
  scheduleMockInst.callCronLoop();
  std::time_t time2 = scheduleMockInst.getCronNext();
  // Lambda should not have been called, and cronnext should not have changed.
  TEST_ASSERT_EQUAL(time1, time2);
  std::time_t old_time = scheduleMockInst.stringToTime("2020-01-01 14:23:45");
  scheduleMockInst.setCronNextRaw(old_time);
  scheduleMockInst.callCronLoop();
  std::time_t new_time = scheduleMockInst.getCronNext();
  // Lambda should have been called, and cronnext should be updated.
  TEST_ASSERT_TRUE(new_time > old_time);
}


// TODO: Cover GetHash(), and Schedules(string-key).


int runUnityTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_schedule_receives_name);
  RUN_TEST(test_schedule_receives_id);
  RUN_TEST(test_schedule_receives_lambda);
  RUN_TEST(test_schedule_calculates_cronnext);
  RUN_TEST(test_schedule_contains_schedules);
  RUN_TEST(test_schedule_cronNextExpired);
  RUN_TEST(test_schedule_cronLoop);
  return UNITY_END();
}



// IMPLEMENTATIONS - you can remove unnecessary implementations if not using them //

/**
  * For native dev-platform or for some embedded frameworks
  */
//int main(void) {
int main( int argc, char **argv ) {
  TEST_MESSAGE("Test file main() running");
  return runUnityTests();
}

/**
  * For Arduino framework
  */
void setup() {
  TEST_MESSAGE("Test file setup() running");
  // Wait ~2 seconds before the Unity test runner
  // establishes connection with a board Serial interface
  delay(2000);

  runUnityTests();
}

void loop() {}

