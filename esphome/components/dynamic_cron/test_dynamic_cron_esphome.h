// Additional functions for esphome testing.
// This test file and the dynamic_cron_esphome.h source file will ONLY
// run on esp32 hardware.
//
// This will NOT run in the 'native' test environment (but test_dynamic_cron.h will).

#pragma once

// Don't compile this file unless COMPILE_TESTS is defined.
#ifdef COMPILE_TESTS

#include <unity.h>
#include "dynamic_cron_esphome.h"


namespace esphome {
namespace dynamic_cron {
  

class ScheduleEsphomeMock : public Schedule {
public:
    
  ScheduleEsphomeMock(std::time_t test_id = std::time(NULL)) :
    Schedule("test-name", std::to_string(test_id), []() { std::cout << "Test lambda called and running!\n"; return true; })
  {}
  
  std::time_t get_initial_stamp() {
    LOGD("TEST TIMESTAMP: %lld, initial_stamp: %lld", TIMESTAMP, initial_stamp);
    return initial_stamp;
  }
  
  void set_initial_stamp(std::time_t val) {
    initial_stamp_pref.init(schedule_id);
    initial_stamp_pref.save(val);
  }
  
  bool get_bypass() {
    return bypass;
  }
  
  bool get_bypass_default() {
    return bypass_default;
  }
  
};  // class ScheduleEsphomeMock


// TESTS - covers portions of dyncamic_cron that interact directly with esphome functions and classes.

void test_prefs_initial(void) {
  ScheduleEsphomeMock schedule;
  //schedule.set_initial_stamp(0); // Should we test with this or no?
  schedule.setup();
  TEST_ASSERT_TRUE(schedule.get_initial_stamp() == esphome::dynamic_cron::TIMESTAMP);
}

void test_prefs_preserve_existing(void) {
  ScheduleEsphomeMock schedule;
  schedule.set_initial_stamp(123456);
  schedule.setup();
  TEST_ASSERT_TRUE(schedule.get_initial_stamp() != esphome::dynamic_cron::TIMESTAMP);
}

void test_prefs_clear_existing(void) {
  ScheduleEsphomeMock schedule;
  schedule.set_initial_stamp(456789);
  schedule.setClearPrefs(true);
  schedule.setup();
  TEST_ASSERT_TRUE(schedule.get_initial_stamp() == esphome::dynamic_cron::TIMESTAMP);
}

void test_prefs_unchanged(void) {
  ScheduleEsphomeMock schedule;
  schedule.set_initial_stamp(esphome::dynamic_cron::TIMESTAMP);
  schedule.setup();
  TEST_ASSERT_TRUE(schedule.get_initial_stamp() == esphome::dynamic_cron::TIMESTAMP);
}

void test_bypass_prefs(void) {
  ScheduleEsphomeMock schedule;
  schedule.setup();
  TEST_ASSERT_TRUE(schedule.get_bypass() == schedule.get_bypass_default());
}

// Schedule inherits from logger. TODO: improve this.
void test_inherited_logger_methods(void) {
  ScheduleEsphomeMock schedule;
  schedule.LOGD("Test inherited logger methods");
  TEST_ASSERT_TRUE(true);
}


} // esphome
} // dynamic_cron

#endif  // COMPILE_TESTS

