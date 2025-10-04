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
#include <Preferences.h>
#include <ctime>

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/core/time.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/text/text.h"

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


class Schedule : public Component, public ScheduleCore {

protected:
  // If true, clears prefs at first boot after flash.
  bool                clear_prefs;
  
  std::time_t         cron_loop_previous_time;
  std::time_t         save_prefs_previous_time;
    
public:
  double              cron_loop_interval; // seconds
  double              save_prefs_interval; // seconds
  
  // These hold pointers to the subcomponents.
  BypassSwitch        *bypass_switch{nullptr};
  RememberNextSwitch  *remember_next_switch{nullptr};
  CronNextSensor      *cron_next_sensor{nullptr};
  CrontabText         *crontab_text{nullptr};
  //   
  //   switch_::Switch            *bypass_switch{nullptr};
  //   switch_::Switch            *remember_next_switch{nullptr};
  //   text_sensor::TextSensor   *cron_next_sensor{nullptr};
  //   text::Text                *crontab_text{nullptr};
  
  // Macro for defining setters for the above entity pointer variables.
  #define DEFINE_SETTER(MemberType, MemberName) \
    void set_##MemberName(MemberType *value) { this->MemberName = value; }
    
  DEFINE_SETTER(BypassSwitch, bypass_switch)
  DEFINE_SETTER(RememberNextSwitch, remember_next_switch)
  DEFINE_SETTER(CronNextSensor, cron_next_sensor)
  DEFINE_SETTER(CrontabText, crontab_text)
  
  bool                      last_bypass_state = 0;
  bool                      last_remember_next_state = 0;
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
    save_prefs_interval(60),
    clear_prefs(false)
  {
    LOGI("Initializing cron schedule '%s' %s", _name.c_str(), _id.c_str());
    cron_loop_previous_time = std::time(NULL);
    save_prefs_previous_time = std::time(NULL);
  } // end Schedule(...).


  // Esphome Component overrides
  void setup() override {
    // Disable for production
    //printVersion();
    //LOGV("About to call timeIsValid() in Schedule.setup()");
    
    if (timeIsValid() && !setup_complete) {
      loadPrefs();

      setup_complete = true;

      LOGV("Setup completed for '%s' %s %s", schedule_name.c_str(), schedule_id.c_str(), id_hash.c_str());
      if (! timeIsValid(cronnext)) {
        setCronNext();
      }
    }
  }
  
  
  void loop() override {
    std::time_t now = std::time(NULL);
    double seconds_since_last_cron_loop = difftime(now, cron_loop_previous_time);
    double seconds_since_last_save      = difftime(now, save_prefs_previous_time);
  
    if (setup_complete && timeIsValid()) {
      //LOGV("Looping: %lld", (long long)now);
      if (seconds_since_last_cron_loop > cron_loop_interval) {
        cronLoop();
        cron_loop_previous_time = std::time(NULL);
      }
      
      if (seconds_since_last_save > save_prefs_interval) {
        savePrefs();
        save_prefs_previous_time = std::time(NULL);
      }
    }
    
    else {
      if (seconds_since_last_cron_loop > cron_loop_interval) {
        setup();
        cron_loop_previous_time = std::time(NULL);
      }
    }
    
    updateEntityData(bypass_switch, last_bypass_state, getBypass());
    updateEntityData(remember_next_switch, last_remember_next_state, getRememberNext());
    updateEntityData(cron_next_sensor, last_cron_next_state, cronNextString("---"));
    updateEntityData(crontab_text, last_crontab_text_state, getCrontab());
  }
  
  
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
  
  
protected:
  
  // The SchedulePrefs class/struct instance should:
  //   * Initialize prefs for this schedule.
  //   * Retrieve data for all keys.
  // Then use this object to:
  //   * Set schedule fields from prefs.
  //   * compare schedule fields with existing prefs, looking for changes.
  // When an instance of SchedulePrefs is created, it should be passed the
  // ScheduleCore instance, which should then be stored in the SchedulePrefs instance.
  //
  // For list of Preferences data types see:
  //   https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
  //
  // For list of C format specifiers (for printf, etc.) see:
  //   https://www.geeksforgeeks.org/format-specifiers-in-c/
  //
  // For linux epoch converter see: https://www.epochconverter.com/
  //
  // Note that this SchedulePrefs class/struct is a sub class defined within the Schedule class.
  // As such, it has certain specific and privileged behavior with regards to the parent Schedule class.
  //
  struct SchedulePrefs : public LoggerLocal<SchedulePrefs> {
    
  public:
    Preferences   api;
    Schedule*     schedule;
    
    std::time_t   initialized;  // TIMESTAMP of firmware in seconds-since-epoch at compile time (from __init__.py).
                                // TIMESTAMP is hardcoded in python at firmware compile time.
                                // TIMESTAMP is declared in dynamic_cron.h, using 'inline'.
                                // TIMESTAMP is defined in main.cpp at the top (not any more).
                                // TIMESTAMP is assigned in main.cpp in the setup() function.
    std::string   crontab;
    bool          remember_next;
    bool          bypass;
    std::time_t   cronnext;
    
    SchedulePrefs(Schedule* _schedule) :
      schedule(_schedule)
    {
      initialize();
      load();
    }
    
    // Does this need to return a bool, or can it be void?
    bool initialize(bool force = false) {  // We're not using 'force' yet
      LOGV("Opening prefs %s %s for initialization", schedule->schedule_id.c_str(), schedule->id_hash.c_str());

      api.begin(schedule->id_hash.c_str(), false); // open prefs read-write
      
      if (TIMESTAMP == 0) {
        LOGW(
          "Firmware TIMESTAMP == 0 and could prevent proper management of schedule preferences between firmware flashes",
          NULL
        );
      }
      
      // Initializes namespace timestamp, if not already done.
      
      if (! api.isKey("initialized")) {
        LOGI("Initializing prefs namespace %s %s with stamp '%lld'",
          schedule->schedule_id.c_str(),
          schedule->id_hash.c_str(),
          (long long)TIMESTAMP
        );
        
        // Sets the 'initialized' preference field to TIMESTAMP (seconds, from __init__.py).
        api.putLong64("initialized", TIMESTAMP);
        
        size_t number_free_entries = api.freeEntries();
        LOGD("There are %u free entries available in the namespace table %s",
          number_free_entries,
          schedule->id_hash.c_str()
        );
      }
      
      // Retrieves the preference 'initialized' field.
      initialized = api.getLong64("initialized", 0);

      // Clears preferences namespace, if conditions allow.
      //
      // Note that logically: !(x | y) == (!x & !y)
      //
      bool rslt = false;
      if (force == true || schedule->clear_prefs == true && TIMESTAMP != 0 && initialized != TIMESTAMP) {
        rslt = api.clear(); // && api.putLong64("initialized", TIMESTAMP);
        if (rslt) {
          LOGI("Re-initialized prefs namespace %s %s with stamp '%lld'",
            schedule->schedule_id.c_str(),
            schedule->id_hash.c_str(),
            (long long)TIMESTAMP
          );
        }
      }
      
      // Updates 'initialized' if different from TIMESTAMP.
      if (TIMESTAMP != 0 && initialized != TIMESTAMP) {
        api.putLong64("initialized", TIMESTAMP);
        initialized = api.getLong64("initialized", 0);
        
        LOGI("Updated prefs namespace %s %s with stamp '%lld'",
          schedule->schedule_id.c_str(),
          schedule->id_hash.c_str(),
          (long long)initialized
        );
      }
            
      // Initializes data fields with defaults.
      
      if (! api.isKey("crontab")) {
        api.putString("crontab", String(schedule->crontab_default));
      }
      
      if (! api.isKey("remember_next")) {
        api.putBool("remember_next", schedule->remember_next_default);
      }
      
      if (! api.isKey("bypass")) {
        api.putBool("bypass", schedule->bypass_default);
      }
      
      if (! api.isKey("cronnext")) {
        api.putLong64("cronnext", 0);
      }

      api.end();
      return rslt;
      
    } // initialize()
    
    
    // Loads stored prefs into local variables.
    void load() {
      LOGV("Opening prefs %s for reading", schedule->id_hash.c_str());
      api.begin(schedule->id_hash.c_str(), true); // open prefs read-only
      
      LOGV("Reading crontab from prefs");
      crontab = api.getString("crontab", schedule->crontab_default).c_str();

      LOGV("Reading remember_next from prefs");
      remember_next = api.getBool("remember_next", schedule->remember_next_default);

      LOGV("Reading bypass from prefs");
      bypass = api.getBool("bypass", schedule->bypass_default);

      LOGV("Reading cronnext from prefs");
      cronnext = (std::time_t) api.getLong64("cronnext", 0);

      api.end();
    }
  }; // SchedulePrefs struct
  
  
  // Loads persistent data from SchedulePrefs vars into Schedule vars.
  SchedulePrefs loadPrefs() {
    LOGV("Beginning loadPrefs()");

    SchedulePrefs prefs = SchedulePrefs(this);
    
    crontab = prefs.crontab;
    remember_next = prefs.remember_next;
    bypass = prefs.bypass;

    if (remember_next && !bypass) {
      cronnext = prefs.cronnext;
    } else {
      cronnext = 0;
    }

    LOGD("Loaded crontab: %s", crontab.c_str());
    LOGD("Loaded remember_next: %d", remember_next);
    LOGD("Loaded cronnext: %lld (%s)", (long long)cronnext, timeToString(cronnext).c_str());
    LOGD("Loaded bypass: %d", bypass);
    
    return prefs;
    
  } // loadPrefs()
  
  
  // Saves persistent data to esp32 NVS.
  void savePrefs() {
    SchedulePrefs prefs = SchedulePrefs(this);
    
    bool crontab_changed = (crontab != prefs.crontab);
    bool remember_next_changed = (remember_next != prefs.remember_next);
    bool cronnext_changed = (cronnext != prefs.cronnext && remember_next && !bypass);
    bool bypass_changed = (bypass != prefs.bypass);

    // If any changes, then open prefs for writing.
    if (crontab_changed || remember_next_changed || cronnext_changed || bypass_changed) {
      
      Preferences api;

      LOGD("Opening prefs %s %s for writing", schedule_id.c_str(), id_hash.c_str());
      api.begin(id_hash.c_str(), false); // open as read/write

      if (crontab_changed) {
        LOGI("Saving crontab to prefs: '%s'", crontab.c_str());
        api.putString("crontab", String(crontab.c_str()));
      }

      if (remember_next_changed) {
        LOGI("Saving remember_next to prefs: %d", remember_next);
        api.putBool("remember_next", remember_next);
      }

      if (cronnext_changed) {
        LOGI("Saving cronnext to prefs: %lld", (long long)cronnext);
        api.putLong64("cronnext", cronnext);
      }

      if (bypass_changed) {
        LOGI("Saving bypass to prefs: %i", bypass);
        api.putBool("bypass", bypass);
      }

      api.end();

    } // if any changes
  } // savePrefs()
  
  
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
  
  
  template<typename EntityT, typename StateT>
  void updateEntityData(EntityT *entity, StateT &last_state, const StateT &new_state) {
    if (!entity) return;  // guard against null
    if (new_state != last_state) {
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
  bool last_state{0};
  
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
  bool last_state{0};

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

