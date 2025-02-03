// Additional functions for esp32 testing.
// This fails to compile properly.
// I've tried a lot of hacks to get it going.

//#include "../test_esp/esphome.h"
#include <esphome/core/log.h>
#include <esphome/components/logger/logger.h>
#include <esphome/core/application.h>
#include <esphome/core/component.h>

//#include <esphome/core/log.cpp>
//#include <esphome/components/logger/logger.cpp>
//#include <esphome/core/application.cpp>
//#include <esphome/core/component.cpp>

#include "../esphome/components/dynamic_cron/dynamic_cron_esphome.h"



class ScheduleEspMock : public esphome::dynamic_cron::Schedule {
public:
    
  ScheduleEspMock() :
    Schedule("test-name", "test-id", []() { std::cout << "Test lambda called\n"; return true; })
  {}
  
  auto getPrefs() {
    return loadPrefs();
  }
  
};


void test_prefs(void) {
  ScheduleEspMock* schedule = new ScheduleEspMock;
  // auto prefs = schedule->getPrefs();
  // TEST_ASSERT_TRUE(prefs.initialized > 0);
  // delete schedule;
}
