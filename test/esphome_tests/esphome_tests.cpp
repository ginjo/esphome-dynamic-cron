// Additional functions for esphome testing.
// This test file and the dynamic_cron_esphome.h source file will ONLY
// run on esp32 hardware with the 'esphome' test environment,
// and not in the 'native' test environment.

#include <unity.h>
//#include "../esphome/components/dynamic_cron/dynamic_cron_esphome.h"
#include <esphome/components/dynamic_cron/dynamic_cron_esphome.h>
#include "test_dynamic_cron.cpp"



class ScheduleEsphomeMock : public esphome::dynamic_cron::Schedule {
public:
    
  ScheduleEsphomeMock() :
    Schedule("test-name", "test-id", []() { std::cout << "Test lambda called\n"; return true; })
  {}
  
  auto getPrefs() {
    return loadPrefs();
  }
  
};


// TESTS - covers portionsl of dyncamic_cron that interact directly with esphome functions and classes.

void test_prefs_initialized(void) {
  ScheduleEsphomeMock schedule;
  auto prefs = schedule.getPrefs();
  TEST_ASSERT_TRUE(prefs.initialized > 0);
}

void test_prefs_updates_timestamp(void) {
  // TODO: This won't test correctly if run with esphome build.
  // It will only work in the test environments.
  ScheduleEsphomeMock schedule;
  esphome::dynamic_cron::TIMESTAMP += 300;
  auto prefs = schedule.getPrefs();
  TEST_ASSERT_TRUE(esphome::dynamic_cron::TIMESTAMP == 1600000300);
}

