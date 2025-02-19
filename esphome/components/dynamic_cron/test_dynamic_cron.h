#pragma once

#include <unity.h>
//#include <typeinfo>
#include "dynamic_cron.h"

#if !defined(IS_NATIVE) || IS_NATIVE != 1
  #include "test_dynamic_cron_esphome.h"
#endif


namespace esphome {
namespace dynamic_cron {
  

class ScheduleMock : public ScheduleCore {
public:
    
  ScheduleMock() :
    ScheduleCore("test-name", "test-id", []() { SLOGD("dynamic_cron", "Test lambda called"); return true; })
  {}
  
  // Added this destructor to debug mystery exception, didn't help
  //~ScheduleMock() noexcept(false) {}

  bool callLambda() {
    return target_action_fptr();
  }
  
  // Helper method to access protected ScheduleCore::splitString().
  // Returns single member of string vector from splitString() function.
  std::string getStringVectorMember(std::string _string, std::string _regexp, size_t index) {
    if (_string == "") { return ""; }
    std::string result = splitString(_string, _regexp)[index];
    return result;
  }
  
  // Helper method to set cronnext with older time,
  // since the official setCronNext() is protected.
  void setCronNextRaw(std::time_t input) {
    cronnext = input;
  }

  // Cuz cronLoop() is protected.
  void callCronLoop() {
    cronLoop();
  }    
};

// Declards a ScheduleMock object with default constructor.
// This will be used during each test run. See setUp().
ScheduleMock* ScheduleMockInst = nullptr;

void initScheduleMock(ScheduleMock* obj) {
  //std::cout << "inside initScheduleMock()\n";
  ScheduleMockInst = obj;
}


// TESTS - Covers most but not all functions in dynamic_cron.h, either directly or indirectly.
//         Does NOT cover anything in dynamic_cron_esphome.h (yet), as that file
//         requires links to arduino and esphome hardware objects.

void test_schedule_receives_name(void) {
  TEST_ASSERT_TRUE(ScheduleMockInst->getNameString() == "test-name");
}

void test_schedule_receives_id(void) {
  //std::cout << "Running a test\n";
  TEST_ASSERT_TRUE(ScheduleMockInst->getIdString() == "test-id");
}

void test_schedule_receives_lambda(void) {
  bool rslt = ScheduleMockInst->callLambda();
  TEST_ASSERT_TRUE(rslt);
}

void test_schedule_contains_schedules(void) {
  // Outputs the Schedules() array size, for debugging.
  //std::cout << ScheduleCore::Schedules().size() << "\n";
  // Compares the last member in Schedules(). We need that when running alongside
  // a fully built esphome project, since the first member will likely be
  // one of the project's Schedule instances.
  TEST_ASSERT_TRUE(ScheduleCore::Schedules().back() == ScheduleMockInst);
}

void test_schedule_calculates_cronnext(void) {
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  std::string cron_next = ScheduleMockInst->cronNextString();
  //std::string now = ScheduleMockInst->timeToString(); // What was this for?
  std::string time_only = ScheduleMockInst->getStringVectorMember(cron_next, " ", 1);
  // Test-message is not supported in the Unity framework provided with platformio.
  // Update: It now works!! Not sure why.
  //TEST_MESSAGE(now.c_str());
  //TEST_MESSAGE(cron_next.c_str());
  //TEST_MESSAGE(time_only.c_str());
  TEST_ASSERT_TRUE(time_only == "03:02:01");
  // Bad crontab should be handled
  ScheduleMockInst->setCrontab("1 2 3 * * * | foo bar baz");
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() == (std::time_t)0);
  // Fixed crontab should resolve
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() > (std::time_t)0);
  // Bypassed schedule should have no cronnext
  ScheduleMockInst->setBypass(true);
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() == (std::time_t)0);
  // Re-enabled schedule should resolve
  ScheduleMockInst->setBypass(false);
  TEST_ASSERT_TRUE(ScheduleMockInst->getCronNext() > (std::time_t)0);
}

void test_schedule_cronNextExpired(void) {
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  // Default cronnext should be legit.
  TEST_ASSERT_FALSE(ScheduleMockInst->cronNextExpired());
  // Old cronnext should be considered expired.
  std::time_t old_time = ScheduleMockInst->stringToTime("2020-01-01 12:34:56");
  ScheduleMockInst->setCronNextRaw(old_time);
  TEST_ASSERT_TRUE(ScheduleMockInst->cronNextExpired());
  // Missing crontab prevents expired from returning true, even if cronnext is expired.
  // Do we really want that?
  ScheduleMockInst->setCrontab("");
  TEST_ASSERT_FALSE(ScheduleMockInst->cronNextExpired());
  // setCronNext() with an old time is not legit user operation and will be filtered out.
  // resulting in legit cronnext.
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  ScheduleMockInst->setCronNext(old_time);
  TEST_ASSERT_FALSE(ScheduleMockInst->cronNextExpired());
}

void test_schedule_cronLoop(void) {
  // cronnext should not have changed (and lambda should not have been called).
  ScheduleMockInst->setCrontab("1 2 3 * * *");
  std::time_t cronnext1 = ScheduleMockInst->getCronNext();
  ScheduleMockInst->callCronLoop();
  std::time_t cronnext2 = ScheduleMockInst->getCronNext();
  TEST_ASSERT_TRUE(std::difftime(cronnext1, cronnext2) == 0);
  // cronnext should have changed (and lambda should have been called).
  std::time_t old_time = ScheduleMockInst->stringToTime("2020-01-01 12:34:56");
  ScheduleMockInst->setCronNextRaw(old_time);
  ScheduleMockInst->callCronLoop();
  std::time_t new_time = ScheduleMockInst->getCronNext();
  TEST_ASSERT_TRUE(std::difftime(new_time, old_time) > 0);
}


// RUNNER

int TEST_RUN_COUNT = 0;
int TEST_RUN_MAX   = 1;

int DynamicCronTestRunner(void) {
  std::cout << "BEGIN DYNAMIC CRON TESTS\n";
  UNITY_BEGIN();
  
  // Runs these tests in all environments, including esphome build.
  RUN_TEST(test_schedule_receives_name);
  RUN_TEST(test_schedule_receives_id);
  RUN_TEST(test_schedule_receives_lambda);
  RUN_TEST(test_schedule_contains_schedules);
  RUN_TEST(test_schedule_calculates_cronnext);
  RUN_TEST(test_schedule_cronNextExpired);
  RUN_TEST(test_schedule_cronLoop);

  // Runs these tests in esp environment and esphome build.
  #if !defined(IS_NATIVE) || IS_NATIVE != 1
    RUN_TEST(test_prefs_initialized);
    RUN_TEST(test_prefs_matches_timestamp);
    RUN_TEST(test_inherited_logger_methods);
  #endif
  
  // Runs these tests only in esp environment (not esphome build).
  #if defined(IS_NATIVE) && IS_NATIVE !=1
    RUN_TEST(test_prefs_updates_timestamp);
  #endif

  std::cout << "END DYNAMIC CRON TESTS\n";
  return UNITY_END();
}

int RunDynamicCronTests(int max = 0) {
  if (max > 0) { TEST_RUN_MAX = max; }

  if (TEST_RUN_COUNT < TEST_RUN_MAX) {
    TEST_RUN_COUNT += 1;
    return DynamicCronTestRunner();
  }
  else {
    return 1;
  }
}


} // esphome
} // dynamic_cron


// UNITY SETUP / TEARDOWN
// These must be top-level functions.

void setUp(void) {
  // Performed before ever test is run.
  
  //std::cout << "SETUP\n";
  
  // We need to use dynamic (heap?) memory,
  // otherwise the object goes out of scope and is deleted,
  // even though the var is declared at the top-level.
  // This way, the object persists until we delete it.
  // Note: I think 'new' always returns a pointer.
  esphome::dynamic_cron::ScheduleMockInst = new esphome::dynamic_cron::ScheduleMock;
}

void tearDown(void) {
  // Performed after every test is run.
  
  //std::cout << "TEARDOWN\n";
  
  delete esphome::dynamic_cron::ScheduleMockInst;
}

