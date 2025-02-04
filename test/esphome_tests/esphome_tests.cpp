// Additional functions for esphome testing.
// This test file and the dynamic_cron_esphome.h source file will ONLY
// run on esp32 hardware with the 'esphome' test environment,
// and not in the 'native' test environment.

//#include "../test_esp/esphome.h"
#include <esphome/core/log.h>
#include <esphome/components/logger/logger.h>
#include <esphome/core/application.h>
#include <esphome/core/component.h>

#include <chrono>
#include <thread>
#include <unity.h>

// We already load 'time.h' and 'ctime' in dynamic_cron.h
// This is for direct manipulation of system time using timeval struct.
#include <sys/time.h>
// See here for faketime library, which could help isolate datetime manipulations:
//   https://github.com/wolfcw/libfaketime

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

void test_prefs(void) {
  ScheduleEsphomeMock* schedule = new ScheduleEsphomeMock;
  // auto prefs = schedule->getPrefs();
  // TEST_ASSERT_TRUE(prefs.initialized > 0);
  delete schedule;
}
