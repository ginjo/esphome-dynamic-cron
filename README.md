# ESPHome Dynamic Cron Scheduler

  This [ESPHome](https://esphome.io) External Component provides a cron interface for scheduling anything in ESPHome.
  Live editable crontab expressions, without requiring re-flash or reboot, set this component
  apart from the built-in ESPHome cron functionality.
  
  Features:
  
  * Live editable cron expressions, no re-flash or reboot required.
  * Multiple cron expressions for each schedule.
  * Multiple schedules to cover any number of ESPHome recurring tasks.
  * Remembers missed trigger times after power failure or reboot.
  * Automate trigger times entirely within ESPHome, no home-assistant required.

  Here is an example web GUI of a generic ESPHome project that defines two
  dynamic_cron schedules.

  ![Screenshot of dynamic_cron interface in ESPHome](img/DynamicCron.png)

## Requirements

  This component has two external dependencies that are automatically managed:
  1. [Croncpp](https://github.com/mariusbancila/croncpp) for parsing cron expressions
  2. [Preferences](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html) for storing settings to esp32 NVS
  
  See below for more info on the Croncpp and Preferences libraries.
  
  There are three things to be aware of when using this library:
  
  * The ESPHome firmware must be compiled using the Arduino framework, not the ESP-IDF framework.
  
  * You should define a `time` component in your ESPHome yaml config, as
    scheduling software needs a reliable time source.
    
  * When using this library, ESPHome will compile with build-flags `-std=gnu++17` and
    `-fexceptions`. This should not be a problem for most ESPHome projects, however
    there is a possibility of conflict with other external libraries that specifically
    disable these options.


## Setup

  Put the following code in your ESPHome yaml config.
  This loads the dynamic_cron library into the ESPHome project as an External Component
  and defines one or more schedules. Each schedule calls a user-defined
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
      - name: Irrigation
        lambda: |-
          ESP_LOGD("irrigation", "Starting irrigation");
          id(sprinkler_instance).start_full_cycle();
          return {true};
          
        # You must return true or false from the lambda.
  ```

  Here's a link to a fully functioning [ESPHome configuration file](test/example_esphome.yml)
  that demonstrates dynamic_cron.


## Options

  These are the available options you can use to configure each instance of a dynamic_cron schedule.
  See below for more detail on some of these options.
 
  * **name**: string, *optional*
  * **id**:   string, *optional*

    You should provide at least one of `name` or `id`, or you can provide both.
    If `name` is omitted, the ID will be used to form a default name.

  * **lambda**: any c++ code, *required*

    The `lambda` is called whenever the current time exceeds the cron next-run time.
    You *MUST* return `true` or `false` from the lambda.
  
    **True**  - Cron will update the next-run time, according to the cron expression(s). <br>
    **False** - Cron will *NOT* update the next-run time. Be careful with this, as it could
                lead to excessive looping of the target lambda.
              
  * **crontab**: string, *optional* `("")`
    
    Sets the default `crontab` string. The `crontab` string can be edited at runtime through
    the web interface or the API.
    
  * **disable**: boolean, *optional* `(false)`
  
    Sets the default `disabled` status. The current `disabled` status can be
    changed at runtime through the web interface or the API.
    
  * **remember_next**: boolean, *optional* `(false)`
  
    Sets the default `remember_next` status. The current `remember_next` status
    can be changed at runtime through the web interface or the API.
    
  * **clear_prefs**: boolean, *optional* `(false)`
  
    If this option is `true`, preferences for this schedule, keyed by the schedule `id`,
    will be cleared during the *first* boot after flashing *this specific* build of firmware.
    Subsequent boots with the same firmware will not clear preferences.
    
    If this option is false, preferences for this schedule will not be cleared during first boot,
    or any other boot with this firmware.
    
#### Preferences, Defaults, and Memory
  
  During normal operation, changes made to the `crontab`, `disable`, and `remember_next`
  controls, will be stored in NVS (non volatile storage). If `remember_next` is not
  set to `true`, the next-run time will also be stored. All of these settings will be remembered
  across reboots.
  
  Settings will *generally* be remembered across firmware updates, but only if ALL of the following are true:
  * The `id` of the schedule is not changed.
  * The `clear_prefs` configuration option is not set to `true`.
  
  When defaults are specified in the configuration, they will be used when ANY of the following are true:
  * `clear_prefs` option is set before building/flashing the firmware.
  * No preferences have ever been set for this schedule `id`.
  * During runtime, if a needed preference cannot be found (error or bug situation).
  
  If no defaults are specified in the configuration, the built-in defaults will be used.
  See the configuration options above for the built-in default of each option.
  

## Usage
    
  Once your ESP device is up and running, there will be 4 entities available for each schedule created.
  These entities can be accessed through the ESPHome web server or through the API, including Home Assistant.
  
  * Crontab (text field)
  * Next run time (text-sensor)
  * Disable schedule (switch)
  * Remember next run (switch)
  
  ### Cron Expressions (crontab)
  
  Enter one or more cron expressions in the Crontab text field.
  Multiple cron expressions are separated by space-bar-space, or literally " | ".
  All cron expressions entered will be used to determine the next-run time.
  
  Example entry in crontab field:
  
    0 0 0,5 * * mon,wed,fri | 0 30 2 * * mon,wed,fri
    
  This translates to *every Mon, Wed, Fri at midnight, 2:30am, and 5:00am*.

  For supported cron expression features and syntax, see the Croncpp documentation.
  * https://github.com/mariusbancila/croncpp
    
  ### Disable Schedule
  
  When this entity is turned ON, the schedule is disabled. No other functionality of ESPHome is affected.
  While a schedule is disabled, no next-run time is calculated.
  When this element is turned OFF, the schedule is activated and a new next-run time is calculated.
  
  ### Remember Next
  
  If Remember Next is turned on, the schedule will remember the next run time after a power failure
  or reboot. Otherwise, at boot up, the next run time will be calculated from the current point in time.
  
  This setting can be helpful, if you are scheduling frequent triggers, where making up a missed run is not important.
  Leaving this setting as false will reduce wear on NVS (non-volatile-storage) from frequent writes of the next run time.
  Conversely, if you have a schedule that *must* run once per day, turning this option on will help ensure
  the schedule runs in the event of a power outage that crosses the next run time.
  
  ### Missed Runs
  
  If a valid next-run was stored at the time of a power-outage or reboot event,
  that next-run will be "remembered" and started at the next power-on, if ALL of the following are true:
  
  * The stored next-run is in the past.
  * Disable Schedule is not set to `true`.
  * Remember Next is not set to `true`.
  
  ### Multiple Schedules
  
  You can create multiple schedules under the dynamic\_cron section.
  For example, you may have multiple sprinkler instances, one for lawn irrigation and one for drip irrigation.
  In this case, you might want a different schedule for each, as they have different watering pattern requirements.
  You can create a separate schedule for each of these sprinkler instances, and each schedule will be completely
  independent of the others.
  
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


## More info on Croncpp and Preferences:

  **Croncpp** is a c++ library for parsing cron expressions.

  * https://github.com/mariusbancila/croncpp
  * https://www.codeproject.com/Articles/1260511/cronpp-A-Cplusplus-Library-for-CRON-Expressions

  **Preferences** is part of the arduino-esp32 library
  and is provided as part of the esphome build environment.
  It is used to store persistent settings and scheduling data on the ESP32 device.

  * https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html
  * https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html


## Development

  To facilitate testing and further development, this repository includes a Docker directory with
  configuration and scripts to launch a bash session from the official `esphome/esphome` Docker image.
  
  *Note that these developer tools are in flux and may change without notice.*
  
  To enter the bash session within a container created from the `esphome/esphome` Docker image:
  
  * Clone this repository to a machine that has Docker installed.
  * `cd` into the cloned directory.
  * Run `docker/dev.sh` in your terminal from within the cloned directory.
  
  From there, you can run the tests associated with this repository, or you can experiment
  with alternate builds of esp32 firmware using the platformio IDE.
  
  ```
    # Tests generic portions of this component in the linux container.
    test/run.sh
    
    # With more verbose output
    test/run.sh -vv
    
    # Tests esp32/esphome-specific portions of this component on the esp32 hardware.
    test/run.sh -vv -e esphome
  ```
  
  ### Mapped Directories
  
  There are three relevant directories on the Docker host that are mapped into the `esphome/esphome` container.
  Each of these directories can be overridden with environment variable. The environment variables can be set
  on the command line or in the `.env` file within the root directory of this project.
  
  * **.platformio/** Where platformio stores external and downloaded libraries during the build process.
    The default location is within the root level of the project directory and can be overridden with the environment
    variable `DOT_PLATFORMIO`.
 
  * **.pio/** Where platformio stores build files and the resulting firmware during the build process.
    The default location is within the root level of the project directory and can be overridden with the environment
    variable `DOT_PIO`.   
     
  * **src/** Where platformio looks for the ESPHome source code during the build process.
    The default location points to the `/esphome` directory within the container and can be
    overridden with the environment variable `SRC_DIRECTORY`.   

