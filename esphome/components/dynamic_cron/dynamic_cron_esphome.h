// TODO: Fix the Preferences errors when the requested preference item
//       Has not been stored yet (on new or repurposed esp32 boards).
//       There is not actually any error happening, as our code handles this
//       just fine. But the Preferences library insists on spitting out log lines
//       making us think there is an actual error happening. This won't look good
//       to users who flash a new esp32 board with dynamic_cron, and suddenly see
//       a bunch or "errors".
//       SEE dyncamic_cron.h file for more info on this TODO.
//
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
// FIXBUG: If ignore-missed is set, then cronext string won't be sent to the browser.
//       It still won't show up, even if ignore-missed is disabled, while a legit crontab
//       exists.
//       Update: There appears to be a data related problem with handling cronnext, since
//       'Lawn' schedule is working fine, but 'Drip' schedule only shows '---' for cronnext.
//       Update: I improved the logging in setCronNext. Try testing it to see if it shows the problem.


#pragma once

#include "Arduino.h"
//#include "esphome.h"
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

#include "dynamic_cron.h"


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
    ScheduleCore(_name, _id, _target_action_fptr),
    clear_prefs(false)
  {
    ESP_LOGD(TAG, "Initializing Schedule object '%s' %s", _name.c_str(), _id.c_str());
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
  
  
  bool initializePrefs(bool force = false) {  // We're not using 'force' yet
    const char *idhash = id_hash.c_str();
    LOGD(TAG, "Opening prefs '%s' (%s) for initialization", schedule_name.c_str(), idhash);
    
    prefs.begin(idhash, false); // open read-write
    int _initialized = prefs.getInt("initialized", 0);
    // Log output for debugging:
    // LOGD(TAG, "Preferences '%s' (%s) is comparing TIMESTAMP '%i' with prefs.initialized '%i'",
    //           schedule_name.c_str(),
    //           idhash,
    //           TIMESTAMP,
    //           _initialized
    // );
    
    // If the baked-in TIMESTAMP differs from the one saved in this schedule's prefs, then clear prefs.
    // This will happen on first boot after a flash, if clear_prefs==true, unless TIMESTAMP == 0.
    // TIMESTAMP is baked into firmware by __init__.py.
    //
    // if (force == false && (_initialized == TIMESTAMP || TIMESTAMP == 0)) {
    //   rslt = false;
    // }
    // else if (clear_prefs == true || force == true) {
    //   rslt = prefs.clear() && prefs.putInt("initialized", TIMESTAMP);
    //   if (rslt) {
    //     LOGD(TAG, "Initialized prefs '%s' (%s) with stamp '%i'", schedule_name.c_str(), idhash, TIMESTAMP);
    //   }
    // }
    // else {
    //   rslt = false;
    // }
    //
    // Refactored if-then logic to reduce verbosity. Should be same logic now but less convoluted.
    // See here for online logic calculator: https://user.eng.umd.edu/~yavuz/logiccalc.html
    // Note that logically: !(x | y) == (!x & !y)
    //
    bool rslt = false;
    
    if (force == true || clear_prefs == true && TIMESTAMP != 0 && _initialized != TIMESTAMP) {
      rslt = prefs.clear() && prefs.putInt("initialized", TIMESTAMP);
      if (rslt) {
        LOGD(TAG, "Initialized prefs '%s' (%s) with stamp '%i'", schedule_name.c_str(), idhash, TIMESTAMP);
      }
    }
    
    prefs.end();
    return rslt;
  }
  
  
protected:

  // Access to the Preferences handler.
  Preferences prefs;
  
  bool clear_prefs; // If true, clears prefs at first boot after flash.
  
  
  // Loads persistent data from esp32 nvs.
  void loadPrefs() {
    const char *idhash = id_hash.c_str();
    
    LOGD(TAG, "Opening prefs '%s' (%s) for reading", schedule_name.c_str(), idhash);
    prefs.begin(idhash, true); // open read-only
    
    size_t number_free_entries = prefs.freeEntries();
    LOGD(TAG, "There are %u free entries available in the namespace table '%s'", number_free_entries, idhash);

    //LOGD(TAG, "Loading crontab from prefs '%s', with potential default '%s'", schedule_name.c_str(), crontab_default.c_str());
    LOGD(TAG, "Loading crontab from prefs '%s'", schedule_name.c_str());
    crontab = prefs.getString("crontab", crontab_default).c_str();

    LOGD(TAG, "Loading ignore_missed from prefs '%s'", schedule_name.c_str());
    ignore_missed = prefs.getBool("ignore_missed", ignore_missed_default);

    LOGD(TAG, "Loading bypass from prefs '%s'", schedule_name.c_str());
    bypass = prefs.getBool("bypass", bypass_default);

    // This has to load after all the others, since we may need to call setCronNext(),
    // which depends on the others being loaded.
    if (!ignore_missed) {
      LOGD(TAG, "Loading cronnext from prefs '%s'", schedule_name.c_str());
      cronnext = (std::time_t) prefs.getDouble("cronnext", 0);
    } else {
      // timeNow() might not be valid yet, but we'll try here anyway.
      // Otherwise, we have a hook in the cron loop that will pick this up.
      //setCronNext();
      cronnext = 0;
    }

    prefs.end(); // close

    LOGD(TAG, "Schedule '%s' loaded crontab: %s", schedule_name.c_str(), crontab.c_str());
    LOGD(TAG, "Schedule '%s' loaded ignore_missed: %i", schedule_name.c_str(), ignore_missed);
    if (!ignore_missed) {
      LOGD(TAG, "Schedule '%s' loaded cronnext: %d (%s)", schedule_name.c_str(), cronnext, timeToString(cronnext).c_str());
    }
    LOGD(TAG, "Schedule '%s' loaded bypass: %i", schedule_name.c_str(), bypass);
    
  } // loadPrefs()
  
  
  // Saves persistent data to esp32 nvs.
  void savePrefs() override {
    const char *idhash = id_hash.c_str();

    //LOGD(TAG, "Opening prefs '%s' (%s) to check for changed values", schedule_name.c_str(), idhash);
    prefs.begin(idhash, true); // open read-only
    bool crontab_changed = (crontab != std::string(prefs.getString("crontab", crontab_default).c_str()));
    bool ignore_missed_changed = (ignore_missed != prefs.getBool("ignore_missed", ignore_missed_default));
    bool cronnext_changed = (cronnext != (std::time_t) prefs.getDouble("cronnext", 0) && !ignore_missed);
    bool bypass_changed = (bypass != prefs.getBool("bypass", bypass_default));
    prefs.end(); // close

    // If any changes, then open prefs for writing.
    if (crontab_changed || ignore_missed_changed || cronnext_changed || bypass_changed) {

      LOGD(TAG, "Opening prefs '%s' (%s) for writing", schedule_name.c_str(), idhash);
      prefs.begin(idhash, false); // open as read/write

      if (crontab_changed) {
        LOGD(TAG, "Saving crontab to prefs '%s' (%s)", schedule_name.c_str(), crontab.c_str());
        prefs.putString("crontab", String(crontab.c_str()));
      }

      if (ignore_missed_changed) {
        LOGD(TAG, "Saving ignore_missed to prefs '%s' (%d)", schedule_name.c_str(), ignore_missed);
        prefs.putBool("ignore_missed", ignore_missed);
      }

      if (cronnext_changed) {
        LOGD(TAG, "Saving cronnext to prefs '%s' (%i)", schedule_name.c_str(), cronnext);
        prefs.putDouble("cronnext", cronnext);
      }

      if (bypass_changed) {
        LOGD(TAG, "Saving bypass to prefs '%s' (%d)", schedule_name.c_str(), bypass);
        prefs.putBool("bypass", bypass);
      }

      prefs.end();

    } // if any changes
  } // savePrefs()

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

