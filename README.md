# ESPHome Dynamic Cron Scheduler

  This ESPHome External Component provides a cron interface for scheduling anything in ESPHome.
  The project was originally created to schedule ESPHome Sprinkler controllers,
  but it can schedule anything in your ESPHome project.
  
  Features:
  
  * Automate trigger times entirely within ESPHome, no home-assistant required.
  * Live editable cron expressions, no re-flash or reboot required.
  * Multiple cron expressions for each schedule instance.
  * Multiple schedule instances to cover all of your ESPHome recurring tasks.
  * Remembers missed trigger times after power failure or reboot (optional).

## Requirements

  This component has two dependencies, Croncpp and Preferences, but you don't need to worry about those,
  as they are managed automatically within the dynamic\_cron library.
  See below for more info on the Croncpp and Preferences libraries.
  
  There are two things to be aware of when using this library:
  
  * You should define a ```time``` component in your ESPHome yaml config.
    Scheduling software doesn't work well without a reliable time source.
    
  * ESPHome will compile using build-flags ```-std=gnu++17``` and ```-fexceptions```.
    This should not be a problem for most ESPHome projects, however there is a possibility
    of conflict with other external libraries that specifically disable these options.
  
## Setup

  Put the following code in your ESPHome yaml config.
  This loads the dynamic_cron library into your ESPHome project as an External Component
  and defines one or more schedule instances. Each schedule instance calls a user-defined
  lambda, when triggered by the cron scheduler.
  
  ```yaml
    
    esphome:
      ...
      ...
    
    external_components:
      - source:
          type: git
          url: https://github.com/ginjo/esphome-dynamic-cron
    
    dynamic_cron:
      - name: Lawn
        lambda: |-
          id(sprinkler_instance).start_full_cycle();
          return {true};
  ```
  
  You must return ```true``` or ```false``` from the lambda.
  * *True* tells cron to update the next-run time.
  * *False* will NOT update the next-run time. Be careful with this, as it could
    cause excessive looping of your target action.
    
## Usage

  ### Cron Expressions
    
  Once your ESP device is up and running, there will be 4 elements available for each
  schedule instance created.
  
  * Crontab text field
  * Next run time text-sensor
  * Disable schedule switch
  * Ignore missed switch
  
  Enter one or more cron expressions in the Crontab text field.
  Multiple cron expressions are separated by space-bar-space, or literally " | ".
  All cron expressions entered will be used to determine the next-run time.
  Example entry in crontab field:
  
    0 0 0,5 * * mon,wed,fri | 0 30 2 * * mon,wed,fri
    
  This translates to *every Mon, Wed, Fri at midnight, 2:30am, and 5:00am*.

  For supported cron expression features and syntax, see the Croncpp documentation.
  * https://github.com/mariusbancila/croncpp .
    
  ### Disable Schedule
  
  When this entity is turned ON, the schedule are disabled. No other functionality of ESPHome is affected.
  While schedules are disabled, no next-run time is calculated.
  When this element is turned OFF, schedules are activated and a new next-run time is calculated.
  
  ### Ignore Missed
  
  If Ignore Missed is turned on, the schedule will not remember the next-run time after a power failure
  or reboot. At boot up, the next-run time will be calculated from the current point in time.
  
  This setting can be helpful, if you're scheduling frequent triggers, where making up a missed one is not important.
  This will reduce wear on non-volatile-storage from frequent write-outs of the next-run time.
  
  Also see Missed Runs below.
  
  ### Multiple Schedule Instances
  
  You can create multiple Schedule instances under the dynamic\_cron section.
  For example, you may have multiple Sprinkler instances, one for lawn irrigation and one for drip irrigation.
  In this case, you might want a separate schedule for each, as each has different watering pattern requirements.
  You can create a separate Schedule for each of these instances, and each schedule instance will be completely
  separate and independent of the others.
  
  ```
    dynamic_cron:
      - name: Lawn
        lambda: |-
          id(sprinkler_lawn).start_full_cycle();
          return {true};
      - name: Drip
        lambda: |-
          id(sprinkler_drip).start_full_cycle();
          return {true};
  ```
    
  ### Missed Runs
  
  If a valid next-run was stored at the time of a power-outage or reboot event,
  that next-run will be "remembered" and started at the next power-on, if ALL of the following are true:
  
  * The stored next-run is in the past.
  * Disable Schedule is not set.
  * Ignore Missed is not set.

## More info on Croncpp and Preferences:

  "Croncpp" is a c++ library for handling cron expressions.

  * https://github.com/mariusbancila/croncpp
  * https://www.codeproject.com/Articles/1260511/cronpp-A-Cplusplus-Library-for-CRON-Expressions

  "Preferences" is part of the arduino-esp32 library,
  and it is provided as part of the esphome build environment.
  It's used to store persistent scheduling data on the ESP32 device.

  * https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
  * https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html

