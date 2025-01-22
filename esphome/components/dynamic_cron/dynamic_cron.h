// Currently NOT WORKING


#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include <croncpp.h>
#include <iostream>
#include <iomanip>
#include <string>
// #include <ctime> do we need this for stringToTime() ?
#include <regex>
#include <vector>
#include <map>
#include <algorithm>
#include <Preferences.h>
#include <time.h>

#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/text/text.h"
#include "core.h"


namespace esphome {
namespace dynamic_cron {

//static const char *TAG = "dynamic_cron";

// Forward declarations that just barely work, given single-file code structure.
// To push the sub-component building entirely into c++, we would need to separate
// the code into .h and .cpp files. Otherwise we get bad-use-of-incomplete-class
// errors at compile time. Currently not an issue, since we build subcomponents
// from the py code, which the right way to do it in esphome.
class Schedule;
class CrontabTextField;
class BypassSwitch;
class IgnoreMissedSwitch;
class CronNextSensor;


class Schedule : public Component, public ScheduleCore {
    
public:
  
  // These are just to hold pointers to the subcomponents.
  // They aren't currently used or necessary, but may be nice to have in future.
  CrontabTextField    *crontab_text_field;
  BypassSwitch        *bypass_switch;
  IgnoreMissedSwitch  *ignore_missed_switch;
  CronNextSensor      *cron_next_sensor;
  
  
  // Esphome Component overrides
  void setup() override {
    if (timeIsValid() && !setup_complete) {
      initializePrefs();
      loadPrefs();
      ESP_LOGD(TAG, "Setup completed for %s, with id_hash %s", schedule_name.c_str(), id_hash.c_str());
      if (! timeIsValid(cronnext)) {
        setCronNext();
      }
      
      setup_complete = true;
    }
  }
  
  void loop() override {
    std::time_t now = std::time(NULL);
    double seconds = difftime(now, previous);
    
    if (seconds > loop_interval) {
    
      if (setup_complete) {
        //ESP_LOGD(TAG, "Looping: %i", now);
        cronLoop();
      }
      else {
        setup();
      }
      
      previous = std::time(NULL);
    }
  }
  
  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Dynamic Cron Schedule");
  }
  
  
  // Custom constructor method to create Schedule object.
  // NOTE: The function-pointer argument must have NO captures, if it's receiving a lambda.
  // Otherwise, the lambda won't be converted to a simple function/pointer.
  // So, if you pass in a lambda, the [] must be empty.
  //
  Schedule( // schedule-name, schedule-id, target-action-lambda-or-function-pointer
    std::string _name,
    std::string _id,
    bool(*_target_action_fptr)()
  ) :
    ScheduleCore(_name, _id, _target_action_fptr)
  {
    ESP_LOGD(TAG, "Initializing Schedule object '%s'", schedule_id.c_str());
  } // end Schedule(...).


  // These won't work, because the original methods need to be virtual,
  // but templated methods can't be virtual.
  //
  // // Forwards local call method from core.h to ESP logging macros.
  // template<typename... Args>
  // static void LOGD(const char *tag, const char *fmt, Args... args) {
  //   ESP_LOGD(tag, fmt, args...);
  // }
  // 
  // // Forwards local call method from core.h to ESP logging macros.
  // template<typename... Args>
  // static void LOGE(const char *tag, const char *fmt, Args... args) {
  //   ESP_LOGE(tag, fmt, args...);
  // }
  

}; // Schedule class


class BypassSwitch : public switch_::Switch, public Component  {
public:
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
  Schedule *schedule;
  bool last_state;
  
  explicit BypassSwitch(Schedule* _schedule) :
    schedule(_schedule),
    last_state(0)
  {
    //set_name("Disable");
    //set_object_id("disable_schedule_switch_");
    set_disabled_by_default(false);
    set_icon("mdi:timer-off-outline");
    set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    set_component_source("dynamic_cron");
    App.register_switch(this);
    App.register_component(this);
    schedule->bypass_switch = this;
  }
  
  void setup() {
    //ESP_LOGD(TAG, "get_object_id(): %s", get_object_id().c_str());
  }
  
  void loop() override {
    bool new_state = schedule->getBypass();
    
    if (new_state != last_state) {
      state = new_state;
      last_state = state;
      publish_state(state);
    }
  }
  
  void write_state(bool _state) {
    ESP_LOGD(TAG, "BypassSwitch::write_state(): %i", _state);
    schedule->setBypass(_state);
  }
  
}; // BypassSwitch class


class IgnoreMissedSwitch : public switch_::Switch, public Component {
public:
  
  Schedule *schedule;
  bool last_state;
  
  IgnoreMissedSwitch(Schedule* _schedule) :
    schedule(_schedule),
    last_state(0)
  {
    //set_name("Ignore Missed");
    //set_object_id("ignore_missed_switch_");
    set_disabled_by_default(false);
    set_icon("mdi:timer-off-outline");
    set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    set_component_source("dynamic_cron");
    App.register_switch(this);
    App.register_component(this);
    schedule->ignore_missed_switch = this;
  }
  
  void setup() {
    //ESP_LOGD(TAG, "get_object_id(): %s", get_object_id().c_str());
  }
  
  void loop() override {
    bool new_state = schedule->getIgnoreMissed();
    
    if (new_state != last_state) {
      state = new_state;
      last_state = state;
      publish_state(state);
    }
  }
  
  void write_state(bool _state) {
    ESP_LOGD(TAG, "IgnoreMissedSwitch::write_state(): %i", _state);
    schedule->setIgnoreMissed(_state);
  }
  
}; // IgnoreMissedSwitch class


class CronNextSensor : public text_sensor::TextSensor, public Component {
public:
  
  Schedule *schedule;
  std::string last_state;
  
  CronNextSensor(Schedule* _schedule) :
    schedule(_schedule),
    last_state("")
  {
    //set_name("Next Run");
    //set_object_id("cron_next_sensor_");
    //set_disabled_by_default(false);
    set_icon("mdi:timer-outline");
    set_component_source("dynamic_cron");
    App.register_text_sensor(this);
    App.register_component(this);
    schedule->cron_next_sensor = this;
  }
  
  void setup() {
    //ESP_LOGD(TAG, "get_object_id(): %s", get_object_id().c_str());
  }
  
  void loop() override {
    std::string new_state = schedule->cronNextString("---");
    //state = schedule->cronNextString("---");
    
    if (new_state != last_state) {
      state = new_state;
      last_state = state;
      publish_state(state);
    }
  }
  
}; // CronNextSensor class


class CrontabTextField : public text::Text, public Component {
public:
  
  Schedule *schedule;
  std::string last_state;
  
  CrontabTextField(Schedule* _schedule) :
    schedule(_schedule),
    last_state("")
  {
    //set_name("Crontab");
    //set_object_id("crontab_text_field_");
    set_disabled_by_default(false);
    set_icon("mdi:calendar-clock-outline");
    traits.set_min_length(0);
    traits.set_max_length(255);
    traits.set_mode(text::TEXT_MODE_TEXT);
    set_component_source("dynamic_cron");
    App.register_text(this);
    App.register_component(this);
    schedule->crontab_text_field = this;
  }
  
  void setup() {
    //ESP_LOGD(TAG, "get_object_id(): %s", get_object_id().c_str());
  }
  
  void loop() override {
    std::string new_state = schedule->getCrontab();
    
    if (new_state != last_state) {
      state = new_state;
      last_state = state;
      publish_state(state);
    }
  }
  
  void control(const std::string &_state) {
    //ESP_LOGD(TAG, "CrontabTextField::control(): %i", &_state);
    schedule->setCrontab(_state);
  }
  
  
}; // CrontabTextField class


} // dynamic_cron namespace
} // esphome namespace

