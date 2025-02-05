// Additional functions for esphome testing.
// This test file and the dynamic_cron_esphome.h source file will ONLY
// run on esp32 hardware with the 'esphome' test environment,
// and not in the 'native' test environment.

//#include "../test_esp/esphome.h"
// #include <esphome/core/log.h>
// #include <esphome/components/logger/logger.h>
// #include <esphome/core/application.h>
// #include <esphome/core/component.h>

#include "../esphome/components/dynamic_cron/dynamic_cron_esphome.h"


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
  ScheduleEsphomeMock schedule;
  esphome::dynamic_cron::TIMESTAMP += 300;
  auto prefs = schedule.getPrefs();
  TEST_ASSERT_TRUE(esphome::dynamic_cron::TIMESTAMP == 1600000300);
}

