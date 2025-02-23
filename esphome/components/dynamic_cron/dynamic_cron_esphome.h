//
// TODO: Consider saving the prefs object within the Schedule, instead of loading it up every run though the loop.
//       Then compare Schedule fields against that prefs object, instead of against a new prefs object every time
//       through the loop. Then whenever you save anything to prefs NVS, get a new prefs object to store in the Schedule.

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
class CrontabTextField;
class BypassSwitch;
class RememberNextSwitch;
class CronNextSensor;


class Schedule : public Component, public ScheduleCore {

protected:
  // If true, clears prefs at first boot after flash.
  bool                clear_prefs;
  
  std::time_t         cron_loop_previous_time;
  std::time_t         save_prefs_previous_time;
    
public:
  double              cron_loop_interval; // seconds
  double              save_prefs_interval; // seconds
  
  // These are just to hold pointers to the subcomponents.
  // They aren't currently used or necessary, but may be nice to have in future.
  CrontabTextField    *crontab_text_field;
  BypassSwitch        *bypass_switch;
  RememberNextSwitch  *remember_next_switch;
  CronNextSensor      *cron_next_sensor;
  
  
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
    if (timeIsValid() && !setup_complete) {
      //initializePrefs();
      loadPrefs();
      LOGV("Setup completed for '%s' %s %s", schedule_name.c_str(), schedule_id.c_str(), id_hash.c_str());
      if (! timeIsValid(cronnext)) {
        setCronNext();
      }
      
      setup_complete = true;
    }
  }
  
  
  void loop() override {
    std::time_t now = std::time(NULL);
    double seconds_since_last_cron_loop = difftime(now, cron_loop_previous_time);
    double seconds_since_last_save      = difftime(now, save_prefs_previous_time);
  
    if (setup_complete && timeIsValid()) {
      //LOGV("Looping: %li", now);
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
      setup();
    }
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
  // Note that this SchedulePrefs class/struct is defined within the Schedule class.
  // It has certain specific and privileged behavior with regards to the Schedule class.
  //
  struct SchedulePrefs : public LoggerLocal<SchedulePrefs> {
    
  public:
    Preferences   api;
    Schedule*     schedule;
    
    std::time_t   initialized;  // TIMESTAMP of firmware in seconds-since-epoch at compile time (from __init__.py).
    std::string   crontab;
    bool          remember_next;
    bool          bypass;
    std::time_t   cronnext;
    
    SchedulePrefs(Schedule* _schedule) :
      schedule(_schedule)
      //LoggerLocal(_schedule->schedule_name)
    {
      //const char *idhash = schedule->id_hash.c_str();
      //schedule_name = schedule->schedule_name; // schedule_name field is inherited from LoggerLocal.
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
        LOGI("Initializing prefs namespace %s %s with stamp '%li'",
          schedule->schedule_id.c_str(),
          schedule->id_hash.c_str(),
          TIMESTAMP
        );
        
        // Sets the 'initialized' preference field to TIMESTAMP (seconds, from __init__.py).
        api.putLong("initialized", TIMESTAMP);
        
        size_t number_free_entries = api.freeEntries();
        LOGD("There are %u free entries available in the namespace table %s",
          number_free_entries,
          schedule->id_hash.c_str()
        );
      }
      
      // Retrieves the preference 'initialized' field.
      initialized = api.getLong("initialized", 0);

      // Clears preferences namespace, if conditions allow.
      //
      // Note that logically: !(x | y) == (!x & !y)
      //
      bool rslt = false;
      if (force == true || schedule->clear_prefs == true && TIMESTAMP != 0 && initialized != TIMESTAMP) {
        rslt = api.clear(); // && api.putLong("initialized", TIMESTAMP);
        if (rslt) {
          LOGI("Re-initialized prefs namespace %s %s with stamp '%li'",
            schedule->schedule_id.c_str(),
            schedule->id_hash.c_str(),
            TIMESTAMP
          );
        }
      }
      
      // Updates 'initialized' if different from TIMESTAMP.
      if (TIMESTAMP != 0 && initialized != TIMESTAMP) {
        api.putLong("initialized", TIMESTAMP);
        initialized = api.getLong("initialized", 0);
        
        LOGI("Updated prefs namespace %s %s with stamp '%li'",
          schedule->schedule_id.c_str(),
          schedule->id_hash.c_str(),
          initialized
        );
      }
            
      // Initializes data fields
      
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
        api.putLong("cronnext", 0);
      }

      api.end();
      return rslt;
      
    } // initialize()
    
    
    void load() {
      LOGV("Opening prefs %s for reading", schedule->id_hash.c_str());
      api.begin(schedule->id_hash.c_str(), true); // open prefs read-only
      
      LOGV("Loading crontab from prefs");
      crontab = api.getString("crontab", schedule->crontab_default).c_str();

      LOGV("Loading remember_next from prefs");
      remember_next = api.getBool("remember_next", schedule->remember_next_default);

      LOGV("Loading bypass from prefs");
      bypass = api.getBool("bypass", schedule->bypass_default);

      LOGV("Loading cronnext from prefs");
      cronnext = (std::time_t) api.getLong("cronnext", 0);

      api.end();
    }
  }; // SchedulePrefs struct
  
  
  // Loads persistent data from esp32 NVS.
  SchedulePrefs loadPrefs() {
    
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
    LOGD("Loaded cronnext: %li (%s)", cronnext, timeToString(cronnext).c_str());
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
        LOGI("Saving cronnext to prefs: %li", cronnext);
        api.putLong("cronnext", cronnext);
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
    ESPTime esp_time = ESPTime::from_epoch_local(now);
    bool rslt_esp = esp_time.is_valid();
    bool rslt_parent = ScheduleCore::timeIsValid(now);
    bool rslt_final = rslt_parent && rslt_esp;
    
    if (rslt_final) {
      // LOGV("timeIsValid() using additional check with ESPTime: %d", rslt_final);
    } else {
      LOGV("timeIsValid() using additional check with ESPTime: %d", rslt_final);
    }
    
    return rslt_final;
  }

}; // Schedule class


// ESPHOME ENTITY COMPONENTS

class BypassSwitch : public switch_::Switch, public Component, public LoggerLocal<BypassSwitch> {
public:
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
  Schedule *schedule;
  bool last_state;
  
  explicit BypassSwitch(Schedule* _schedule) :
    schedule(_schedule),
    last_state(0)
    //LoggerLocal(_schedule->schedule_name)
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
    //schedule_name = schedule->schedule_name; // schedule_name field is inherited from LoggerLocal.
  }
  
  void setup() {
    LOGV("get_object_id(): %s", get_object_id().c_str());
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
    LOGD("BypassSwitch::write_state(): %d", _state);
    schedule->setBypass(_state);
  }
  
}; // BypassSwitch class


class RememberNextSwitch : public switch_::Switch, public Component, public LoggerLocal<RememberNextSwitch> {
public:
  
  Schedule *schedule;
  bool last_state;
  
  RememberNextSwitch(Schedule* _schedule) :
    schedule(_schedule),
    last_state(0)
    //LoggerLocal(_schedule->schedule_name)
  {
    //set_name("Remember Next");
    //set_object_id("remember_next_switch_");
    set_disabled_by_default(false);
    set_icon("mdi:timer-off-outline");
    set_restore_mode(switch_::SWITCH_RESTORE_DISABLED);
    set_component_source("dynamic_cron");
    App.register_switch(this);
    App.register_component(this);
    schedule->remember_next_switch = this;
    //schedule_name = schedule->schedule_name; // schedule_name field is inherited from LoggerLocal.
  }
  
  void setup() {
    LOGV("get_object_id(): %s", get_object_id().c_str());
  }
  
  void loop() override {
    bool new_state = schedule->getRememberNext();
    
    if (new_state != last_state) {
      state = new_state;
      last_state = state;
      publish_state(state);
    }
  }
  
  void write_state(bool _state) {
    LOGD("RememberNextSwitch::write_state(): %d", _state);
    schedule->setRememberNext(_state);
  }
  
}; // RememberNextSwitch class


class CronNextSensor : public text_sensor::TextSensor, public Component, public LoggerLocal<CronNextSensor> {
public:
  
  Schedule *schedule;
  std::string last_state;
  
  CronNextSensor(Schedule* _schedule) :
    schedule(_schedule),
    last_state("")
    //LoggerLocal(_schedule->schedule_name)
  {
    //set_name("Next Run");
    //set_object_id("cron_next_sensor_");
    //set_disabled_by_default(false);
    set_icon("mdi:timer-outline");
    set_component_source("dynamic_cron");
    App.register_text_sensor(this);
    App.register_component(this);
    schedule->cron_next_sensor = this;
    //schedule_name = schedule->schedule_name; // schedule_name field is inherited from LoggerLocal.
  }
  
  void setup() {
    LOGV("get_object_id(): %s", get_object_id().c_str());
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


class CrontabTextField : public text::Text, public Component, public LoggerLocal<CrontabTextField> {
public:
  
  Schedule *schedule;
  std::string last_state;
  
  CrontabTextField(Schedule* _schedule) :
    schedule(_schedule),
    last_state("")
    //LoggerLocal(_schedule->schedule_name)
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
    //schedule_name = schedule->schedule_name; // schedule_name field is inherited from LoggerLocal.
  }
  
  void setup() {
    LOGV("get_object_id(): %s", get_object_id().c_str());
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
    LOGV("CrontabTextField::control(): %d", &_state);
    schedule->setCrontab(_state);
  }
  
}; // CrontabTextField class


} // dynamic_cron namespace
} // esphome namespace

