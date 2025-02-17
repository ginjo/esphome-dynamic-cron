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
class IgnoreMissedSwitch;
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
  IgnoreMissedSwitch  *ignore_missed_switch;
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
    //LOGD(LOGTAG, "Initializing Schedule '%s' %s", _name.c_str(), _id.c_str());
    LOGD("Initializing Schedule %s", _id.c_str());
    cron_loop_previous_time = std::time(NULL);
    save_prefs_previous_time = std::time(NULL);
  } // end Schedule(...).


  // Esphome Component overrides
  void setup() override {
    if (timeIsValid() && !setup_complete) {
      //initializePrefs();
      loadPrefs();
      LOGD("Setup completed for %s, with id_hash %s", schedule_id.c_str(), id_hash.c_str());
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
      //LOGD("Looping: %li", now);
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
    // This method will trigger once for each scheduled loaded by esphome,
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
  struct SchedulePrefs : public LoggerLocal {
  public:
    Preferences   api;
    Schedule*     schedule;
    
    std::time_t   initialized;  // TIMESTAMP of firmware in seconds-since-epoch at compile time (from __init__.py).
    std::string   crontab;
    bool          ignore_missed;
    bool          bypass;
    std::time_t   cronnext;
    
    SchedulePrefs(Schedule* _schedule) :
      schedule(_schedule),
      LoggerLocal(_schedule->schedule_name)
    {
      //const char *idhash = schedule->id_hash.c_str();
      //schedule_name = schedule->schedule_name; // schedule_name field is inherited from LoggerLocal.
      initialize();
      load();
    }
    
    // Does this need to return a bool, or can it be void?
    bool initialize(bool force = false) {  // We're not using 'force' yet
      // LOGD("Opening prefs %s for initialization",
      //   schedule->id_hash.c_str()
      // );

      api.begin(schedule->id_hash.c_str(), false); // open prefs read-write
      
      if (TIMESTAMP == 0) {
        LOGE(
          "Firmware TIMESTAMP == 0 and could prevent proper management of schedule preferences between firmware flashes",
          NULL
        );
      }
      
      // Initializes namespace timestamp, if not already done.
      
      if (! api.isKey("initialized")) {
        LOGD("Initializing prefs namespace %s with stamp '%li'",
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
          LOGD("Re-initialized prefs namespace %s with stamp '%li'",
            schedule->id_hash.c_str(),
            TIMESTAMP
          );
        }
      }
      
      // Updates 'initialized' if different from TIMESTAMP.
      if (TIMESTAMP != 0 && initialized != TIMESTAMP) {
        api.putLong("initialized", TIMESTAMP);
        initialized = api.getLong("initialized", 0);
        
        LOGD("Updated prefs namespace %s with stamp '%li'",
          schedule->id_hash.c_str(),
          initialized
        );
      }
            
      // Initializes data fields
      
      if (! api.isKey("crontab")) {
        api.putString("crontab", String(schedule->crontab_default));
      }
      
      if (! api.isKey("ignore_missed")) {
        api.putBool("ignore_missed", schedule->ignore_missed_default);
      }
      
      if (! api.isKey("bypass")) {
        api.putBool("bypass", schedule->bypass_default);
      }
      
      if (! api.isKey("cronnext")) {
        api.putLong("cronnext", 0);
      }

      api.end();
      return rslt;
    }
    
    
    void load() {
      api.begin(schedule->id_hash.c_str(), true); // open prefs read-only
      
      //LOGD("Loading crontab from prefs");
      crontab = api.getString("crontab", schedule->crontab_default).c_str();

      //LOGD("Loading ignore_missed from prefs");
      ignore_missed = api.getBool("ignore_missed", schedule->ignore_missed_default);

      //LOGD("Loading bypass from prefs");
      bypass = api.getBool("bypass", schedule->bypass_default);

      //LOGD("Loading cronnext from prefs");
      cronnext = (std::time_t) api.getLong("cronnext", 0);

      api.end();
    }
  }; // SchedulePrefs struct
  
  
  // Loads persistent data from esp32 NVS.
  SchedulePrefs loadPrefs() {
    
    SchedulePrefs prefs = SchedulePrefs(this);
    
    crontab = prefs.crontab;
    ignore_missed = prefs.ignore_missed;
    bypass = prefs.bypass;

    if (! ignore_missed) {
      cronnext = prefs.cronnext;
    } else {
      cronnext = 0;
    }

    LOGD("Loaded crontab: %s", crontab.c_str());
    LOGD("Loaded ignore_missed: %d", ignore_missed);
    if (!ignore_missed) {
      LOGD("Loaded cronnext: %li (%s)", cronnext, timeToString(cronnext).c_str());
    }
    LOGD("Loaded bypass: %d", bypass);
    
    return prefs;
    
  } // loadPrefs()
  
  
  // Saves persistent data to esp32 NVS.
  void savePrefs() {
    SchedulePrefs prefs = SchedulePrefs(this);
    
    bool crontab_changed = (crontab != prefs.crontab);
    bool ignore_missed_changed = (ignore_missed != prefs.ignore_missed);
    bool cronnext_changed = (cronnext != prefs.cronnext && !ignore_missed && !bypass);
    bool bypass_changed = (bypass != prefs.bypass);

    // If any changes, then open prefs for writing.
    if (crontab_changed || ignore_missed_changed || cronnext_changed || bypass_changed) {
      
      Preferences api;

      LOGD("Opening prefs %s for writing", id_hash.c_str());
      api.begin(id_hash.c_str(), false); // open as read/write

      if (crontab_changed) {
        LOGD("Saving crontab to prefs %s", crontab.c_str());
        api.putString("crontab", String(crontab.c_str()));
      }

      if (ignore_missed_changed) {
        LOGD("Saving ignore_missed to prefs %d", ignore_missed);
        api.putBool("ignore_missed", ignore_missed);
      }

      if (cronnext_changed) {
        LOGD("Saving cronnext to prefs %li", cronnext);
        api.putLong("cronnext", cronnext);
      }

      if (bypass_changed) {
        LOGD("Saving bypass to prefs %i", bypass);
        api.putBool("bypass", bypass);
      }

      api.end();

    } // if any changes
  } // savePrefs()

}; // Schedule class


// ESPHOME COMPONENTS

class BypassSwitch : public switch_::Switch, public Component, public LoggerLocal {
public:
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
  Schedule *schedule;
  bool last_state;
  
  explicit BypassSwitch(Schedule* _schedule) :
    schedule(_schedule),
    last_state(0),
    LoggerLocal(_schedule->schedule_name)
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
    //ESP_LOGD(LOGTAG, "get_object_id(): %s", get_object_id().c_str());
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


class IgnoreMissedSwitch : public switch_::Switch, public Component, public LoggerLocal {
public:
  
  Schedule *schedule;
  bool last_state;
  
  IgnoreMissedSwitch(Schedule* _schedule) :
    schedule(_schedule),
    last_state(0),
    LoggerLocal(_schedule->schedule_name)
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
    //schedule_name = schedule->schedule_name; // schedule_name field is inherited from LoggerLocal.
  }
  
  void setup() {
    //ESP_LOGD(LOGTAG, "get_object_id(): %s", get_object_id().c_str());
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
    LOGD("IgnoreMissedSwitch::write_state(): %d", _state);
    schedule->setIgnoreMissed(_state);
  }
  
}; // IgnoreMissedSwitch class


class CronNextSensor : public text_sensor::TextSensor, public Component, public LoggerLocal {
public:
  
  Schedule *schedule;
  std::string last_state;
  
  CronNextSensor(Schedule* _schedule) :
    schedule(_schedule),
    last_state(""),
    LoggerLocal(_schedule->schedule_name)
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
    //ESP_LOGD(LOGTAG, "get_object_id(): %s", get_object_id().c_str());
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


class CrontabTextField : public text::Text, public Component, public LoggerLocal {
public:
  
  Schedule *schedule;
  std::string last_state;
  
  CrontabTextField(Schedule* _schedule) :
    schedule(_schedule),
    last_state(""),
    LoggerLocal(_schedule->schedule_name)
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
    //ESP_LOGD(LOGTAG, "get_object_id(): %s", get_object_id().c_str());
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
    //ESP_LOGD(LOGTAG, "CrontabTextField::control(): %d", &_state);
    schedule->setCrontab(_state);
  }
  
}; // CrontabTextField class


} // dynamic_cron namespace
} // esphome namespace

