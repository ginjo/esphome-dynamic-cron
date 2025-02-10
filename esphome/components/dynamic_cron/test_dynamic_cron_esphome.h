// Additional functions for esphome testing.
// This test file and the dynamic_cron_esphome.h source file will ONLY
// run on esp32 hardware with the 'esphome' test environment,
// and not in the 'native' test environment.

#pragma once

#include <unity.h>
#include "dynamic_cron_esphome.h"

// We now include this at the bottom, because it needs this file loaded first.
// #include "test_dynamic_cron.h"


namespace esphome {
namespace dynamic_cron {
  

class ScheduleEsphomeMock : public Schedule {
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

// Change this, or add another test, to ...has_matching_timestamp
void test_prefs_updates_timestamp(void) {
  // TODO: This won't test correctly if run with esphome build.
  // It will only work in the test environments.
  ScheduleEsphomeMock schedule;
  TIMESTAMP += 300;
  auto prefs = schedule.getPrefs();
  TEST_ASSERT_TRUE(TIMESTAMP == 1600000300);
}


} // esphome
} // dynamic_cron

//#include "test_dynamic_cron.h"
