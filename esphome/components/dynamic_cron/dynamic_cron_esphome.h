
// NOTE: If ignore_missed is enabled, we don't save cronnext to prefs,
//       even if legit time is in cronnext field. This is not a problem, when there is
//       already data in the cronnext prefs field. It's only at first boot, when this issue arises.
//
// SOLUTION (maybe): Consolidate all prefs-retrieval calls to their own wrapper methods.
//       Within each wrapper, figure out a way to insert dummy data into each prefs field,
//       at boot, if the field is empty and the getter returns a default value.
//       But not the user-facing default though. Use an internal default that indicates that the prefs
//       field is empty. Whenever this default value is returned from the getter, return
//       the user-facing default back to the program.
//       
//       Update: The prefs object has a method to test for existence of a key: prefs.isKey("key-name").
//       See here: https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
//
// TODO: Consider setting cronnext to 0, whenever ignore_missed is set to true
//
// TODO: Consider saving the prefs object within the Schedule, instead of loading it up every run though the loop.
//       Then compare Schedule fields against that prefs object, instead of against a new prefs object every time
//       through the loop. Then whenever you save anything to prefs NVS, get a new prefs object to store in the Schedule.
//
// TODO: Consider an esphome text field for the user to enter a time-formatting expression,
//       for the display of the cronnext time in the browser.


#pragma once

#include "Arduino.h"
// #include "esphome/core/component.h"
// #include "esphome/core/application.h"
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

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/text/text.h"

#include "dynamic_cron.h"


namespace esphome {
namespace dynamic_cron {


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
      //initializePrefs();
      loadPrefs();
      LOGD(LOGTAG, "Setup completed for %s, with id_hash %s", schedule_name.c_str(), id_hash.c_str());
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
        //LOGD(LOGTAG, "Looping: %lu", now);
        cronLoop();
      }
      else {
        setup();
      }
      
      previous = std::time(NULL);
    }
  }
  
  void dump_config() override {
    //ESP_LOGCONFIG(LOGTAG, "Dynamic Cron Schedule");
    LOGD(LOGTAG, "Dynamic Cron Schedule %s", schedule_name.c_str());
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
    ScheduleCore(_name, _id, _target_action_fptr),
    clear_prefs(false)
  {
    LOGD(LOGTAG, "Initializing Schedule '%s' %s", _name.c_str(), _id.c_str());
  } // end Schedule(...).


  // These won't work here, because the original methods need to be virtual,
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


  void setClearPrefs(bool val) {
    clear_prefs = val;
  }
  
  
protected:
  
  // If true, clears prefs at first boot after flash.
  bool clear_prefs;
  
  
  // The SchedulePrefs class instance should:
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
  struct SchedulePrefs {
  public:
    Preferences   api;
    Schedule*     schedule;
    
    int           initialized;  // TIMESTAMP of firmware in seconds-since-epoch at compile time (from __init__.py).
    std::string   crontab;
    bool          ignore_missed;
    bool          bypass;
    std::time_t   cronnext;
    
    SchedulePrefs(Schedule* _schedule) :
      schedule(_schedule)
    {
      //const char *idhash = schedule->id_hash.c_str();
      initialize();
      load();
    }
    
    // Does this need to return a bool, or can it be void?
    bool initialize(bool force = false) {  // We're not using 'force' yet
      // LOGD(LOGTAG, "Opening prefs '%s' %s for initialization",
      //   schedule->schedule_name.c_str(),
      //   schedule->id_hash.c_str()
      // );

      api.begin(schedule->id_hash.c_str(), false); // open prefs read-write
      
      if (TIMESTAMP == 0) {
        LOGE(LOGTAG, "Firmware TIMESTAMP == 0 and could prevent proper management of schedule preferences between firmware flashes");
      }
      
      // Initializes namespace timestamp, if not already done.
      
      if (! api.isKey("initialized")) {
        LOGD(LOGTAG, "Initializing prefs namespace '%s' %s with stamp '%i'",
          schedule->schedule_name.c_str(),
          schedule->id_hash.c_str(),
          TIMESTAMP
        );
        
        // Sets the 'initialized' preference field to TIMESTAMP (seconds, from __init__.py).
        api.putInt("initialized", TIMESTAMP);
        
        size_t number_free_entries = api.freeEntries();
        LOGD(LOGTAG, "There are %u free entries available in the namespace table '%s' %s",
          number_free_entries,
          schedule->schedule_name.c_str(),
          schedule->id_hash.c_str()
        );
      }
      
      // Retrieves the preference 'initialized' field.
      initialized = api.getInt("initialized", 0);

      // Clears preferences namespace, if conditions allow.
      //
      // Note that logically: !(x | y) == (!x & !y)
      //
      bool rslt = false;
      if (force == true || schedule->clear_prefs == true && TIMESTAMP != 0 && initialized != TIMESTAMP) {
        rslt = api.clear(); // && api.putInt("initialized", TIMESTAMP);
        if (rslt) {
          LOGD(LOGTAG, "Re-initialized prefs namespace '%s' %s with stamp '%i'",
            schedule->schedule_name.c_str(),
            schedule->id_hash.c_str(),
            TIMESTAMP
          );
        }
      }
      
      // Updates 'initialized' if different from TIMESTAMP.
      if (TIMESTAMP != 0 && initialized != TIMESTAMP) {
        api.putInt("initialized", TIMESTAMP);
        initialized = api.getInt("initialized", 0);
        
        LOGD(LOGTAG, "Updated prefs namespace '%s' %s with stamp '%i'",
          schedule->schedule_name.c_str(),
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
        api.putULong64("cronnext", 0);
      }

      api.end();
      return rslt;
    }
    
    
    void load() {
      api.begin(schedule->id_hash.c_str(), true); // open prefs read-only
      
      //LOGD(LOGTAG, "Loading crontab from prefs '%s'", schedule->schedule_name.c_str());
      crontab = api.getString("crontab", schedule->crontab_default).c_str();

      //LOGD(LOGTAG, "Loading ignore_missed from prefs '%s'", schedule->schedule_name.c_str());
      ignore_missed = api.getBool("ignore_missed", schedule->ignore_missed_default);

      //LOGD(LOGTAG, "Loading bypass from prefs '%s'", schedule->schedule_name.c_str());
      bypass = api.getBool("bypass", schedule->bypass_default);

      //LOGD(LOGTAG, "Loading cronnext from prefs '%s'", schedule->schedule_name.c_str());
      cronnext = (std::time_t) api.getULong64("cronnext", 0);

      api.end();
    }
  }; // SchedulePrefs
  
  
  // Loads persistent data from esp32 nvs.
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

    LOGD(LOGTAG, "Schedule '%s' loaded crontab: %s", schedule_name.c_str(), crontab.c_str());
    LOGD(LOGTAG, "Schedule '%s' loaded ignore_missed: %d", schedule_name.c_str(), ignore_missed);
    if (!ignore_missed) {
      LOGD(LOGTAG, "Schedule '%s' loaded cronnext: %lu (%s)", schedule_name.c_str(), cronnext, timeToString(cronnext).c_str());
    }
    LOGD(LOGTAG, "Schedule '%s' loaded bypass: %d", schedule_name.c_str(), bypass);
    
    return prefs;
    
  } // loadPrefs()
  
  
  // Saves persistent data to esp32 NVS.
  void savePrefs() override {
    SchedulePrefs prefs = SchedulePrefs(this);
    
    bool crontab_changed = (crontab != prefs.crontab);
    bool ignore_missed_changed = (ignore_missed != prefs.ignore_missed);
    bool cronnext_changed = (cronnext != prefs.cronnext && !ignore_missed);
    bool bypass_changed = (bypass != prefs.bypass);

    // If any changes, then open prefs for writing.
    if (crontab_changed || ignore_missed_changed || cronnext_changed || bypass_changed) {
      
      Preferences api;

      LOGD(LOGTAG, "Opening prefs '%s' (%s) for writing", schedule_name.c_str(), id_hash.c_str());
      api.begin(id_hash.c_str(), false); // open as read/write

      if (crontab_changed) {
        LOGD(LOGTAG, "Saving crontab to prefs '%s' (%s)", schedule_name.c_str(), crontab.c_str());
        api.putString("crontab", String(crontab.c_str()));
      }

      if (ignore_missed_changed) {
        LOGD(LOGTAG, "Saving ignore_missed to prefs '%s' (%d)", schedule_name.c_str(), ignore_missed);
        api.putBool("ignore_missed", ignore_missed);
      }

      if (cronnext_changed) {
        LOGD(LOGTAG, "Saving cronnext to prefs '%s' (%lu)", schedule_name.c_str(), cronnext);
        api.putULong64("cronnext", cronnext);
      }

      if (bypass_changed) {
        LOGD(LOGTAG, "Saving bypass to prefs '%s' (%d)", schedule_name.c_str(), bypass);
        api.putBool("bypass", bypass);
      }

      api.end();

    } // if any changes
  } // savePrefs()

}; // Schedule class


class BypassSwitch : public switch_::Switch, public Component, public MyLogger {
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
    LOGD(LOGTAG, "BypassSwitch::write_state(): %d", _state);
    schedule->setBypass(_state);
  }
  
}; // BypassSwitch class


class IgnoreMissedSwitch : public switch_::Switch, public Component, public MyLogger {
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
    LOGD(LOGTAG, "IgnoreMissedSwitch::write_state(): %d", _state);
    schedule->setIgnoreMissed(_state);
  }
  
}; // IgnoreMissedSwitch class


class CronNextSensor : public text_sensor::TextSensor, public Component, public MyLogger {
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


class CrontabTextField : public text::Text, public Component, public MyLogger {
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

