//
// TODO: Consider saving the prefs object within the Schedule, instead of loading it up every run though the loop.
//       Then compare Schedule fields against that prefs object, instead of against a new prefs object every time
//       through the loop. Then whenever you save anything to prefs NVS, get a new prefs object to store in the Schedule.
//
// TODO: The cronnext value showing in esphome, after an OTA update, is incorrect,
//       until the crontab string is changed, or the device is rebooted.
//       Note that the optional display of the multiple cronnext values is NOT incorrect
//       at any point during this issue.
//       It is not known if the actual next-start-time is incorrect, or if this is a display issue.
//
// NOTE: ESPHome is now using esp-idf v5, which uses 64-bit long-long fot time_t.
//       This is different from esp-idf v4, which used 32-bit long.

#pragma once

#include "Arduino.h"
#include <iostream>
#include <string>
#include <ctime>

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/time.h"
#include "esphome/core/preferences.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/text/text.h"

#include "preference_wrapper.h"
#include "dynamic_cron.h"


namespace esphome {
namespace dynamic_cron {

// Forward Declarations:
//
// To push the sub-component building entirely into c++, we would need to separate
// the code into .h and .cpp files. Otherwise we get bad-use-of-incomplete-class
// errors at compile time. Currently not an issue, since we build subcomponents
// from the py code, which is the right way to do it in esphome, I think.
//
class BypassSwitch;
class RememberNextSwitch;
class CronNextSensor;
class CrontabText;

const size_t            CRONTAB_MAX_LEN = 128;


class Schedule : public Component, public ScheduleCore {

protected:
  // Loop trackers
  std::time_t           cron_loop_previous_time;
  std::time_t           entity_update_previous_time;
  
  // If true, clears prefs at first boot after flash.
  bool                  clear_prefs;
  
  // Preference objects
  MyPreference<std::time_t>         initialized_pref;
  MyPreference<bool>                bypass_pref;
  MyPreference<bool>                remember_next_pref;
  MyPreference<std::time_t>         cronnext_pref;
  StringPreference<CRONTAB_MAX_LEN> crontab_pref;


public:
  double              cron_loop_interval; // seconds
  //double              save_prefs_interval; // seconds
  
  // These hold pointers to the subcomponents.
  BypassSwitch        *bypass_switch{nullptr};
  RememberNextSwitch  *remember_next_switch{nullptr};
  CronNextSensor      *cron_next_sensor{nullptr};
  // TODO: Change CrontabText and crontab_text to CrontabTextField, crontab_text_field
  // Don't forget to update __init__.py
  CrontabText         *crontab_text{nullptr};
  
  // Macro for defining setters for the above entity pointer variables.
  #define DEFINE_SETTER(MemberType, MemberName) \
    void set_##MemberName(MemberType *value) { this->MemberName = value; }
    
  DEFINE_SETTER(BypassSwitch, bypass_switch)
  DEFINE_SETTER(RememberNextSwitch, remember_next_switch)
  DEFINE_SETTER(CronNextSensor, cron_next_sensor)
  DEFINE_SETTER(CrontabText, crontab_text)
  
  bool                      last_bypass_state = false;
  bool                      last_remember_next_state = false;
  std::string               last_cron_next_state = "";
  std::string               last_crontab_text_state = "";
  
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
    ScheduleCore(_name, _id, _target_action_fptr),
    cron_loop_interval(10),
    //save_prefs_interval(60),
    clear_prefs(false)
  {
    LOGI("Constructing cron schedule '%s' %s", _name.c_str(), _id.c_str());
    cron_loop_previous_time = std::time(NULL);
    //save_prefs_previous_time = std::time(NULL);
  } // end Schedule(...).


  // Esphome Component overrides
  void setup() override {

    LOGD("Setup beginning for '%s' %s", schedule_name.c_str(), schedule_id.c_str());
    
    // Init Bypass prefs
    //bypass_pref.init(global_preferences, fnv1a_hash((schedule_id + "_bypass").c_str()));
    bypass_pref.init(schedule_id + "_bypass");
    bypass = bypass_pref.load_with_default(bypass_default);
    LOGI("Loaded bypass: %d", bypass);
    
    // Init RememberNext prefs
    //remember_next_pref.init(global_preferences, fnv1a_hash((schedule_id + "_remember").c_str()));
    remember_next_pref.init(schedule_id + "_remember");
    remember_next = remember_next_pref.load_with_default(remember_next_default);
    LOGI("Loaded remember_next: %d", remember_next);
    
    // Init CronNext prefs
    //cronnext_pref.init(global_preferences, fnv1a_hash((schedule_id + "_cronnext").c_str()));
    cronnext_pref.init(schedule_id + "_cronnext");
      if (remember_next && !bypass) {
        cronnext = cronnext_pref.load_with_default(0);
      } else {
        cronnext_pref.save(0);
        cronnext = 0;
      }
    LOGI("Loaded cronnext: %lld", (long long)cronnext);
    
    // Init Crontab prefs
    //crontab_pref.init(global_preferences, fnv1a_hash((schedule_id + "_crontab").c_str()));
    crontab_pref.init(schedule_id + "_crontab");
    crontab = crontab_pref.load_with_default(crontab_default);
    LOGI("Loaded crontab: %s", crontab.c_str());

    // This isn't helping here, since system time is not yet set.
    if (timeIsValid() && !timeIsValid(cronnext) && !bypass) {
      LOGV("Cronnext not a valid time, calling setCronNext()");
      setCronNext();
    }
    
    // Push values to entity (have these been set up yet?)
    updateEntityData(bypass_switch, last_bypass_state, getBypass());
    updateEntityData(remember_next_switch, last_remember_next_state, getRememberNext());
    updateEntityData(cron_next_sensor, last_cron_next_state, cronNextString("---"));
    updateEntityData(crontab_text, last_crontab_text_state, getCrontab());
    
    if (timeIsValid()) {
      setup_complete = true;
      LOGD("Setup complete for '%s' %s", schedule_name.c_str(), schedule_id.c_str());
    }
    else {
      LOGD("Setup partially complete for '%s' %s", schedule_name.c_str(), schedule_id.c_str());
    }
  } // setup()
  
  
  void loop() override {
    std::time_t now = std::time(NULL);
    double seconds_since_last_cron_loop = difftime(now, cron_loop_previous_time);
    double seconds_since_last_entity_update = difftime(now, entity_update_previous_time);
    //LOGV("Looping: %lld", (long long)now);
  
    if (seconds_since_last_cron_loop > cron_loop_interval) {
      
      // Only cronLoop() if we're fully set up.
      if (setup_complete && timeIsValid()) {
        cronLoop();
      }
      // Else if time is valid, do stuff and mark setup as complete.
      else if (timeIsValid()) {
        if (!timeIsValid(cronnext) && !bypass) setCronNext();
        setup_complete = true;
        LOGD("Setup complete for '%s' %s", schedule_name.c_str(), schedule_id.c_str());
      }
      
      cron_loop_previous_time = std::time(NULL);
    }
    
    // Pushes data to entities periodically.
    //
    if (seconds_since_last_entity_update > 2) {
    
      if (setup_complete) {    // && timeIsValid()) {
        LOGV("bypass_switch last: %d, crnt: %d", last_bypass_state, getBypass());
        updateEntityData(bypass_switch, last_bypass_state, getBypass());
        LOGV("remember_next_switch last: %d, crnt: %d", last_remember_next_state, getRememberNext());
        updateEntityData(remember_next_switch, last_remember_next_state, getRememberNext());
        LOGV("cron_next_sensor last: %s, crnt: %s", last_cron_next_state.c_str(), cronNextString("---").c_str());
        updateEntityData(cron_next_sensor, last_cron_next_state, cronNextString("---"));
        LOGV("crontab_text last: %s, crnt: %s", last_crontab_text_state.c_str(), getCrontab().c_str());
        updateEntityData(crontab_text, last_crontab_text_state, getCrontab());
      }
      
      entity_update_previous_time = std::time(NULL);
    }

  } // loop()
  
  
  void dump_config() override {
    // This method will trigger once for each schedule loaded by esphome,
    // but it does not trigger when running the tests.
    //
    //ESP_LOGCONFIG(LOGTAG, "Dynamic Cron Schedule");
    //LOGD("Dynamic Cron Schedule ");
  }


  void setClearPrefs(bool val) {
    clear_prefs = val;
  }
  
  
  // Overides base setters to include preference storage.
  
  bool setBypass(bool val) override {
    // Note that sched-core setBypass() always calls setCronNext().
    bool rslt = ScheduleCore::setBypass(val);
    bypass_pref.save(rslt);
    return rslt;
  }
  
  bool setRememberNext(bool val) override {
    bool rslt = ScheduleCore::setRememberNext(val);
    remember_next_pref.save(rslt);
    // If remember_next is toggled, we always want to write something to cronnext_pref,
    // unless bypass is true (if bypass is true, cronnext and cronnext_pref should always be 0).
    if (!bypass) {
      if (remember_next == false) {
        cronnext_pref.save(0);
      }
      else {
        cronnext_pref.save(cronnext);
      }
    }
    return rslt;
  }
  
  std::time_t setCronNext() override {
    std::time_t rslt = ScheduleCore::setCronNext();
    // If cronnext is changed, we only save it to prefs if remember next, or if it's 0.
    if (rslt == 0 || remember_next) cronnext_pref.save(rslt);
    return rslt;
  }
  
  std::string setCrontab(std::string str) override {
    // Note that sched-core setCrontab() always calls setCronNext().
    std::string rslt = ScheduleCore::setCrontab(str);
    crontab_pref.save(rslt);
    return rslt;
  }
  
  // TODO: Handle 'setCronNext(std::time_t input)' signature, when we start using it.
  
  
protected:
  
  // Wrapper around ScheduleCore::timeIsValid()
  // Adds validating time against ESPTime
  //
  // NOTE: All callable log lines in this method could run many
  //       times per second, if conditions permit. Only enable
  //       them if necessary for debugging.
  //
  bool timeIsValid(std::time_t now = std::time(NULL)) {
    //LOGV("Schedule::timeIsValid() calling ESPTime::from_epoch_local()");
    ESPTime esp_time = ESPTime::from_epoch_local(now);

    //LOGV("Schedule::timeIsValid() calling esp_time.is_valid()");
    bool rslt_esp = esp_time.is_valid();

    //LOGV("Schedule::timeIsValid() calling ScheduleCore::timeIsValid()");
    bool rslt_parent = ScheduleCore::timeIsValid(now);
    bool rslt_final = rslt_parent && rslt_esp;
    
    // What is this for?
    //     if (rslt_final) {
    //       LOGV("timeIsValid() using additional check with ESPTime: %d", rslt_final);
    //     } else {
    //       LOGV("timeIsValid() using additional check with ESPTime: %d", rslt_final);
    //     }
    
    return rslt_final;
  }
  
  // Method template to update each entity only when data has changed.
  // Call this in the Schedule loop.
  //
  template<typename EntityT, typename StateT>
  void updateEntityData(EntityT *entity, StateT &last_state, const StateT &new_state) {
    if (!entity) return;  // guard against null
    //LOGD("updateEntityData(%s)", entity->get_object_id().c_str());
    if (new_state != last_state) {
      LOGD("updateEntityData(%s) publishing...", entity->get_object_id().c_str());
      entity->publish_state(new_state);
      last_state = new_state;
    }
  }

}; // Schedule class



// ESPHOME ENTITY SUB-COMPONENTS
//
// TODO: Refactor default icon settings – move them to .py file.
//       See here for refactoring default entity icons:
//       https://github.com/esphome/esphome/blob/dev/esphome/const.py

class BypassSwitch : public switch_::Switch, public Component, public LoggerLocal<BypassSwitch> {
public:
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
  Schedule *schedule{nullptr};
  bool last_state{false};
  
  void setup() override {
    //set_disabled_by_default(false);
    set_icon("mdi:timer-off-outline");
    // set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    LOGV("get_object_id(): %s", get_object_id().c_str());
  }
  
  void set_schedule(Schedule *_schedule) {
    schedule = _schedule;
  }
  
  void write_state(bool _state) {
    LOGD("BypassSwitch::write_state(): %d", _state);
    schedule->setBypass(_state);
  }
  
}; // BypassSwitch class


class RememberNextSwitch : public switch_::Switch, public Component, public LoggerLocal<RememberNextSwitch> {
public:
  
  Schedule *schedule{nullptr};
  bool last_state{false};

  void setup() override {
    //set_disabled_by_default(false);
    set_icon("mdi:memory");
    //set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    LOGV("get_object_id(): %s", get_object_id().c_str());
  }

  void set_schedule(Schedule *_schedule) {
    schedule = _schedule;
  }
  
  void write_state(bool _state) {
    LOGD("RememberNextSwitch::write_state(): %d", _state);
    schedule->setRememberNext(_state);
  }
  
}; // RememberNextSwitch class


class CronNextSensor : public text_sensor::TextSensor, public Component, public LoggerLocal<CronNextSensor> {
public:
  
  Schedule *schedule{nullptr};
  std::string last_state{""};

  void setup() override {
    //set_disabled_by_default(false);
    set_icon("mdi:timer-outline");
    LOGV("get_object_id(): %s", get_object_id().c_str());
  }
  
  void set_schedule(Schedule *_schedule) {
    schedule = _schedule;
  }

}; // CronNextSensor class


class CrontabText : public text::Text, public Component, public LoggerLocal<CrontabText> {
public:
  
  Schedule *schedule{nullptr};
  std::string last_state{""};

  void setup() override {
    //set_disabled_by_default(false);
    set_icon("mdi:calendar-clock-outline");
    traits.set_min_length(0);
    traits.set_max_length(255);
    traits.set_mode(text::TEXT_MODE_TEXT);
    LOGV("get_object_id(): %s", get_object_id().c_str());
  }

  void set_schedule(Schedule *_schedule) {
    schedule = _schedule;
  }
  
  void control(const std::string &_state) {
    LOGV("CrontabText::control(): %d", &_state);
    schedule->setCrontab(_state);
  }
  
}; // CrontabText class


} // dynamic_cron namespace
} // esphome namespace

