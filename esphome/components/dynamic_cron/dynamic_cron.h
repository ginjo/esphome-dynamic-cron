// This appears to compile and run fine with my local DynamicCron esphome test project.
//
// TODO: Try this with the production Irrigation esphome project.
//
// √DONE: Handle crontab validation. Currently when it fails Croncpp, it CRASHES the esp32.
//
// TODO: Figure out a way to stop polling empty prefs fields, which spits out constant log lines.
//       This is only an issue when this component is first flashed and no settings have been saved yet.
//       If we poll a prefs field, and nothing comes back, we should remember that as long as the
//       device is running. We would still need to return the default value for that field,
//       but we would stop causing Preferences to log an error(s) every loop. Apparently
//       there is no way to tell Preferences to NOT log an error.
//       BUT, how would we know that nothing was returned from the Preferences get() method? We
//       would have to make the Preferences 'default' value null, or something, and then check for that.
//       SEE dynamic_cron_esphome.h for more info and potential solution on this TODO.
//
// TODO: Push crontab failure messages to the cronnext-string field in the browser.
//
//
// Maybe see here for polymorphic members vars:
// https://stackoverflow.com/questions/17035951/member-variable-polymorphism-argument-by-reference

#pragma once

#include <croncpp.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <ctime> // used for the time 'tm' struct
#include <regex>
#include <vector>
#include <map>
#include <algorithm>
#include <time.h>
#include <ctime>


namespace esphome {
namespace dynamic_cron {

static const char *LOGTAG = "dynamic_cron";

// This is the timestamp of the firmware build.
// This will be set in python and is seconds from epoch.
int TIMESTAMP;


class MyLogger {
public:
  // We created our own LOGx functions, since we need to access them independently
  // from esphome (especially during test runs).
  // Note: Remember that templated methods can't be virtual.
  //
  template<typename... Args>
  static void LOGD(const char *tag, const char *fmt, Args... args) {
      printf("[D][%s]: ", tag);
      printf(fmt, args...);
      printf("\n");
      //(std::cout << ... << args) << std::endl;
  }
  //
  template<typename... Args>
  static void LOGE(const char *tag, const char *fmt, Args... args) {
      printf("[E][%s]: ", tag);
      printf(fmt, args...);
      printf("\n");
      //(std::cout << ... << args) << std::endl;
  }
};


// Forward declaration.
class Schedule;
//class ScheduleMock; // so friend class will work?


// Core definition of the schedule object.
class ScheduleCore : public MyLogger {
  
  // We don't appear to need this... yet.
  // friend class ScheduleMock;
  
protected:
  
  // Basic data points.
  std::string   schedule_name;
  std::string   schedule_id;
  std::string   crontab;
  std::time_t   cronnext;
  bool          bypass;
  bool          ignore_missed;
  std::string   id_hash;
  std::time_t   previous;
  bool          setup_complete;
  String        crontab_default;
  bool          bypass_default;
  bool          ignore_missed_default;
  
  // Lamba for call to target action.
  // Can also receive basic function pointer.
  // Can NOT take lambda captures.
  // See the Schedule constructor (in dynamic_cron.h).
  bool(*target_action_fptr)();
  
  
public:
    
  double loop_interval; // seconds
  
  // // We created our own LOGx functions, since we need to access them independently
  // // from esphome (especially during test runs).
  // template<typename... Args>
  // static void LOGD(const char *tag, const char *fmt, Args... args) {
  //     printf("[D][%s]: ", tag);
  //     printf(fmt, args...);
  //     printf("\n");
  //     //(std::cout << ... << args) << std::endl;
  // }
  // //
  // template<typename... Args>
  // static void LOGE(const char *tag, const char *fmt, Args... args) {
  //     printf("[E][%s]: ", tag);
  //     printf(fmt, args...);
  //     printf("\n");
  //     //(std::cout << ... << args) << std::endl;
  // }
  // 
  
  // Custom constructor method to create ScheduleCore object.
  // NOTE: The function-pointer argument must have NO captures, if it's receiving a lambda.
  // Otherwise, the lambda won't be converted to a simple function/pointer.
  // So, if you pass in a lambda, the [] must be empty.
  //
  ScheduleCore( // schedule-name, schedule-id, target-action-lambda-or-function-pointer
    std::string _name,
    std::string _id,
    bool(*_target_action_fptr)()
  ) :
    schedule_name(_name),
    schedule_id(_id),
    crontab(""),
    crontab_default(""),
    cronnext(0),
    bypass(false),
    bypass_default(false),
    ignore_missed(false),
    ignore_missed_default(false),
    target_action_fptr(_target_action_fptr),
    loop_interval(5),
    id_hash(""),
    setup_complete(false)
  {
    //LOGD(LOGTAG, "Initializing ScheduleCore object '%s' %s", _name.c_str(), _id.c_str());
    id_hash = GetHash(schedule_id);
    previous = std::time(NULL);
    AddToSchedules(this);
  } // end ScheduleCore(...).


  // Globally accessible wrapper for access to all_schedules static var.
  // Are we still using this? YES
  static std::vector<ScheduleCore*>& Schedules() {
      static std::vector<ScheduleCore*> all_schedules;
      return all_schedules;
  }
  
  
  // Gets indexed member of Schedules.
  // Are we still using this? Not internally, but maybe from esphome config?
  static ScheduleCore* Schedules(int index) {
    if (!Schedules().empty()) {
      return Schedules()[index];
    }
    else {
      return nullptr;
    }
  }
  
  
  // Gets item of Schedules vector by schedule_id (as if Schedules was a map).
  // Are we still using this? Not internally, but maybe from esphome config?
  static ScheduleCore* Schedules(std::string _id) {
    if (!Schedules().empty()) {
      for (auto& x : Schedules()) {
        //if (x->schedule_id == _id) { // compares pointers, which might be different even if value matches.
        //if (std::strcmp(x->schedule_id, _id) == 0) { // strcmp() is for char* strings.
        if (x->schedule_id == _id) {
          return x;
        }
      }
    }
    return nullptr;
  }
 

  // Gets human-readable time of cronnext field (not the calc).
  // TODO: Figure out how to show the error from a malformed crontab expression.
  //       I think if a cron expression fails, the error should be sent to an
  //       instance variable (string). If that instance var is not "", then return that
  //       instead of nothing or '---' from this method..
  //       Then any time the cronNextVector returns valid results,
  //       clear the crontab error variable.
  std::string cronNextString(std::string _default="") {
    if (cronnext == 0) {
      //std::string str(_default);
      //return str;
      return _default;
    }
    else {
      return timeToString(cronnext);
    }
  }


  // Returns multiple sequential cronNextCalc results, as a map of {time_t, cron-next-string}.
  std::map<std::time_t, std::string> cronNextMap(int count = 1, std::string _crontab = "", std::time_t ref_time = 0) {

    if (_crontab == "") { _crontab = crontab; }
    if (ref_time == 0) { ref_time = timeNow(); }

    std::map<std::time_t, std::string> out {};
    std::time_t this_time_t = ref_time;
    std::string this_time_s;

    if (_crontab == "" || ref_time == 0) { return out; }

    for (int i=count; i > 0; i--) {
      this_time_t = cronNextCalc(_crontab, this_time_t);
      this_time_s = timeToString(this_time_t);
      out.insert({this_time_t, this_time_s});
      // LOGD(LOGTAG, "i: %i, this_time_t: %i, this_time_s: %s", i, this_time_t, this_time_s.c_str());
    }

    return out;
  }


  // Is cronnext time older than now?
  bool cronNextExpired() {
    std::time_t now = timeNow();
    bool out = false;
    
    // TODO: Can we drop the crontab=="" condition, so we can manually set cronnext
    //       without setting a crontab? Or will that break something?
    //       What happens now, if we set cronnext manually with an empty crontab?
    
    if (crontab == "" || cronnext == 0 || bypass) {
      out = false;
    } else {
      out = (std::difftime(cronnext, now) < 0);
    }
    
    //LOGD(LOGTAG, "cronNextExpired() cronnext, now: %s, %s", timeToString(cronnext).c_str(), timeToString(now).c_str());
    //LOGD(LOGTAG, "cronNextExpired() result: %i", out);
    
    return out;
  }


  // Getter for cronnext time_t field.
  std::time_t getCronNext() {
    return cronnext;
  }  


  // Sets cronnext time_t from crontab field.
  // TODO: Allow a user-entered value to be passed. See below for prototype (works in tests).
  void setCronNext() {
    if (timeIsValid()) {  // If system time is not valid, skip all of this.
      // LOGD(LOGTAG, "setCronNext() '%s', timeIsValid(): TRUE", schedule_name.c_str());
      if (crontab == (std::string)"" || bypass) {
        cronnext = 0;

        // LOGD(LOGTAG, "setCronNext() '%s' to [0], while crontab: %s, bypass: %d",
        //           schedule_name.c_str(),
        //           crontab.c_str(),
        //           bypass
        // );
      }
      else {
        cronnext = cronNextCalc();
        
        // LOGD(LOGTAG, "setCronNext() '%s' to [%lu, %s]",
        //   schedule_name.c_str(),
        //   //schedule_id.c_str(),
        //   cronnext,
        //   timeToString(cronnext).c_str()
        // );
      }
      
      LOGD(LOGTAG, "setCronNext() '%s' to [%lu, %s], while crontab: %s, bypass: %d",
                schedule_name.c_str(),
                cronnext,
                timeToString(cronnext).c_str(),
                crontab.c_str(),
                bypass
      );
    }
    
    else {
      LOGD(LOGTAG, "setCronNext() '%s', timeIsValid(): FALSE");
    }
  }


  // Experimental overload sets cron_next from user input time_t.
  // The design logic was: if input is valid-time, ! bypass, > now, < cronNextCalc(), then cronnext=input;
  // however it might not be exactly that in the code.
  //
  // To get time_t from user input string, use:
  // 
  //   std::time_t parsed = stringToTime(input);
  //
  void setCronNext(std::time_t input) {
    if (
      timeIsValid() &&
      timeIsValid(input) &&
      ! bypass &&
      difftime(input, timeNow()) > 0 &&
      difftime(cronNextCalc(), input) > 0
      // TODO: Why does input need to be < cronNextCalc()?
      // It allows a one-off run, while still maintaining a legit crontab schedule.
      // If no crontab exists, then input can be any time in the future. In that case,
      // we need to make sure to clear out the manuall cronnext after it's used,
      // otherwise it'll trigger with every loop.
    ){
      //LOGD(LOGTAG, "Setting cronnext from input '%s' %lu", timeToString(input).c_str(), input);
      cronnext = input;
      
      LOGD(LOGTAG, "Setting cronnext with input for '%s' %s [%lu, %s]",
        schedule_name.c_str(),
        schedule_id.c_str(),
        input,
        timeToString(input).c_str()
      );
    }
    else {
      LOGD(LOGTAG, "setCronNext() skipping due to invalid input or current time '%s' %s [%lu, %s]",
        schedule_name.c_str(),
        schedule_id.c_str(),
        input,
        timeToString(input).c_str()
      );
      
      setCronNext();
    }
  }


  // Gets string from crontab field.
  //std::string getCrontab() { // returns copy of crontab.
  // This returns a reference to crontab and is more efficient.
  const std::string& getCrontab() const {
    return crontab;
  }


  // Sets crontab with given string.
  std::string setCrontab(std::string str) {
    LOGD(LOGTAG, "Setting crontab for '%s' %s [%s]", schedule_name.c_str(), schedule_id.c_str(), str.c_str());
    crontab = str;
    setCronNext();
    return crontab;
  }


  // Gets bypass bool field.
  bool getBypass() {
    return bypass;
  }


  // Sets bypass bool field and resets cronnect accordingly.
  bool setBypass(bool val) {
    bypass = val;
    setCronNext();
    return val;
  }

  // Gets ignore_missed bool field.
  bool getIgnoreMissed() {
    return ignore_missed;
  }


  // Sets ignore_missed bool field.
  bool setIgnoreMissed(bool val) {
    ignore_missed = val;
    // Do we really need this here?
    //setCronNext();
    return val;
  }
  
  
  // Gets schedule_id string field.
  std::string getIdString() {
    return schedule_id;
  }
  
  
  // Gets schedule_name string field.
  std::string getNameString() {
    return schedule_name;
  }
  
  
  void setBypassDefault(bool val) {
    bypass_default = val;
  }
  
  
  void setIgnoreMissedDefault(bool val) {
    ignore_missed_default = val;
  }
  
  
  void setCrontabDefault(String val) {
    crontab_default = val;
  }


  // Builds human-readable string from time_t.
  // See here for printing time_t data:
  //   https://stackoverflow.com/questions/18422384/how-to-print-time-t-in-a-specific-format
  std::string timeToString(std::time_t timet = std::time(NULL)) {
    // I disabled the timeIsValid() check here to prevent circular definition,
    // since I want to use timeToString() in the timeIsValid() funcion.
    // If we need to re-activate timeIsValid() here, remove timeToString() from timeIsValid().
    //
    if (timet != 0) {   //timeIsValid()) {
      struct tm * timetm;
      // Converts time_t to tm (a fancy time object), cuz that's what strftime wants.
      timetm = localtime(&timet);
      char str[24];
      strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", timetm);
      //LOGD(LOGTAG, "From inside timeToString() '%s'", str);
      return (std::string)str;
    }
    else {
      return "";
    }
  }
  
  
  std::time_t stringToTime(std::string input) {
    //std::string timeString = "2024-09-28 16:25:00"; // Example time string
  
    // Create a tm struct to store the parsed time
    struct tm tm_struct = {};
  
    // Parse the time string using strptime
    if (strptime(input.c_str(), "%Y-%m-%d %H:%M:%S", &tm_struct) == nullptr) {
        LOGE(LOGTAG, "Error parsing time string");
        return 0;
    }
  
    // Convert the tm struct to a time_t value
    time_t t_time = mktime(&tm_struct);
  
    // Log the time_t value
    //LOGD(LOGTAG, "stringToTime() parsed time '%s' in seconds since epoch: %lu", input.c_str(), t_time);
    // Log the reverse operation.
    //LOGD(LOGTAG, "stringToTime() reverse operation: %s", timeToString(t_time).c_str());
  
    return t_time;
  }
  
  
protected:
  
  // This is a mock function for testing.
  // This is needed here so this file can compile independantly of ../dynamic_cron_esphome.h.
  // This expects to be overridden in the dynamic_cron_esphome.h file.
  virtual void savePrefs() {
    // nothing happening here, nothing to see...
  }


  // Adds a schedule object to a globally accessible vector array 'all_schedules'.
  // Are we still using this?
  static void AddToSchedules(ScheduleCore* schedule) {
      //LOGD(LOGTAG, "Adding Schedule '%s' %s to Schedules vector", schedule->schedule_name.c_str(), schedule->schedule_id.c_str());
      Schedules().push_back(schedule);
  }
  

  // Calls cronLoop() method of all Schedules().
  // Deprecated. Now we use esphome loop() method that's part of every Component instance.
  static void CronLooper() {
    //LOGD(LOGTAG, "CronLooper() called");
    for (auto s : Schedules()) {
      s->cronLoop();
    }
  }


  // Compares cronnext with current time and calls lambda.
  // Calls savePrefs().
  void cronLoop() {
    if (timeIsValid() && cronNextExpired()) {
      LOGD(LOGTAG, "Calling lambda for schedule '%s'", schedule_name.c_str());
      bool result = target_action_fptr();
      if (result) {
        setCronNext();
      }
    }
    // We try to setCronNext() at pref loading, but timeNow() might not be valid then,
    // so we try to clean it up here. We don't want to run setCronNext(), unless
    // absolutely necessary, since we might eventually allow user-input for a one-off.
    //
    // If cronnext isn't updating as often as you'd like, check this out:
    // TODO (done!): What if we disable this? It's running more often than it needs to,
    // especially when ignore_missed is true.
    // Update: Initial testing with this commented out... working fine 2024-10-05.
    // Update: Now calling setCronNext() during setup, if !isValidTime(cronnext).
    //
    // else if (timeIsValid() && !bypass && ignore_missed && cronnext == 0) {
    //   setCronNext();
    // }
    savePrefs();
  }


  // Gets next time_t, given cron expression(s) string in crontab.
  std::time_t cronNextCalc(std::string _crontab = "", std::time_t ref_time = 0) {
    if (_crontab == ""){ _crontab = crontab; }
    if (ref_time == 0) { ref_time = timeNow(); }

    // Returns 0 if no crontab or ref_time.
    if (_crontab == "" || ref_time == 0) { return 0; }

    // Requests sorted vector of nexts given crontab parsing string regex.
    std::string regex_str = " *\\| *";
    auto nexts = vectorOfNext(splitString(_crontab, regex_str), ref_time);

    // Logs next-run for each crontab.
    // for (auto& item: nexts)
    // {
    //   LOGD(LOGTAG, "Sorted cron-next: %s", timeToString(item).c_str());
    // }

    // Returns first (soonest) time_t from vector-of-nexts.
    return nexts[0];
  }


  // Returns current time as time_t.
  std::time_t timeNow() {
    return std::time(NULL);
  }


  // Is current (or given) time valid (synced & legit)?
  // Even if it's a valid system time, it must be within a reasonable range,
  // so it can't be 0 (1969, 1970, something like that, depending on locale).
  // We're not actually checking with ESPHome, just with the core c++ time.
  // TODO: Consider comparing (also) against the dynamic_cron firmware timestamp,
  // since it is in seconds-since-epoch.
  //
  bool timeIsValid(std::time_t now = std::time(NULL)) {
    //LOGD(LOGTAG, "About to calculate within timeIsValid()", "");
    
    // We previously tested against esptime.
    //return id(esptime).now().is_valid();
    
    //time_t now;
    //std::time(&now); // same as: now = time(NULL)
    
    struct tm now_tm;
    now_tm = *localtime(&now);
    //LOGD(LOGTAG, "now_tm.tm_year: %i", now_tm.tm_year);
    
    // Valid if year is >= 1969 (1970 is the start of 'epoch' time).
    bool rslt = ((now_tm.tm_year + 1900) > 2019);
    
    //if (! rslt) {
    //  LOGE(LOGTAG, "timeIsValid() failed with '%s'", timeToString(now).c_str());
    //};
    
    return rslt;
  }


  // Function to split std::string on regex.
  std::vector<std::string> splitString(const std::string str, const std::string regex_str) {
      std::regex regexz(regex_str);
      return {std::sregex_token_iterator(str.begin(), str.end(), regexz, -1),
              std::sregex_token_iterator()};
  }


  // Returns sorted vector of next time_t values for given vector-of-crontab-strings.
  std::vector<std::time_t> vectorOfNext(std::vector<std::string> crontabs, std::time_t ref_time = 0) {
    if (ref_time == 0) { ref_time = timeNow(); }
    std::time_t _ref_time = ref_time;
    std::vector<std::time_t> start_times;

    // Adds seconds to ref_time, just to make sure it's ahead of input ref_time.
    // To do that, we have to convert to tm and back to time_t.
    // But it looks like it's not needed!
    struct tm * timetm = localtime(&_ref_time);
    //timetm->tm_sec += 5;
    _ref_time = std::mktime(timetm);

    for (auto& item: crontabs)
    {
      try {
        auto cron_obj = cron::make_cron(item);
        std::time_t next = cron::cron_next(cron_obj, _ref_time);
        start_times.push_back(next);
      }
      catch (cron::bad_cronexpr const &ex) {
        LOGE(LOGTAG, "Not a valid cron expression '%s' %s", item.c_str(), ex.what());
        //start_times.push_back(std::time_t(0));
        return {std::time_t(0)};
      }
    }

    // Sorts (in-place) vector of start_times values from soonest to furthest.
    std::sort(start_times.begin(), start_times.end(), [_ref_time](std::time_t& a, std::time_t& b)
      { 
        double diff_a = std::difftime(a, _ref_time);
        double diff_b = std::difftime(b, _ref_time);
        return diff_a<diff_b;
      }
    );  

    return start_times;
  }
  
  
  // Creates a hash from a string.
  static std::string GetHash(std::string input, int len = 15) {
    //const std::string input = _input;
    const std::hash<std::string> hasher;
    const auto hashResult = hasher(input);
    
    // Convert to hex string
    std::stringstream stream;
    stream << std::hex << hashResult;
    std::string result( stream.str() );

    // Output substring of the hex string.
    //std::cout<<result;
    return (("H"+ result).substr(0,len));
  }

  
}; // ScheduleCore class


} // dynamic_cron namespace
} // esphome namespace

