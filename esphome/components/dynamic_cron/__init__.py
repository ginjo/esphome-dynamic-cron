from time import time
import logging
import yaml
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, text, text_sensor
from esphome.components.time import detect_tz
from esphome.helpers import sanitize, snake_case

_LOGGER = logging.getLogger(__name__)

from esphome.const import (
                      CONF_ID,
                      CONF_LAMBDA,
                      CONF_NAME,
                      CONF_MODE,
                      CONF_ICON,
                    )


# Imports do not load files or paths into the build directory.                          
# You need to use AUTO_LOAD.
AUTO_LOAD          = ['switch', 'text', 'text_sensor']
MULTI_CONF         = True

# Our own custom config options for default member values:
CONF_BYPASS        = 'disabled'
CONF_REMEMBER_NEXT = 'remember_next'
CONF_CRONTAB       = 'crontab'
CONF_CLEAR_PREFS   = 'clear_prefs'
CONF_TIME_FORMAT   = 'time_format'

CONF_BYPASS_SWITCH        = "disabled_switch"
CONF_REMEMBER_NEXT_SWITCH = "remember_next_switch"
CONF_CRON_NEXT_SENSOR     = "cron_next_sensor"
CONF_CRONTAB_TEXT         = "crontab_text"


### cg.add() puts code at top of main.cpp setup() function.
### cg.add_global() puts code at top of main.cpp.

cg.add_build_flag("-fexceptions")
cg.add_platformio_option("build_unflags", ["-fno-exceptions"])

cg.add_library(
    name="Croncpp",
    repository="https://github.com/mariusbancila/croncpp.git",
    version=None,
)


dynamiccron_ns      = cg.esphome_ns.namespace('dynamic_cron')
Schedule            = dynamiccron_ns.class_('Schedule', cg.Component)

BypassSwitch        = dynamiccron_ns.class_('BypassSwitch', switch.Switch, cg.Component)
RememberNextSwitch  = dynamiccron_ns.class_('RememberNextSwitch', switch.Switch, cg.Component)
CronNextSensor      = dynamiccron_ns.class_('CronNextSensor', text_sensor.TextSensor, cg.Component)
CrontabText         = dynamiccron_ns.class_('CrontabText', text.Text, cg.Component)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID):                            cv.declare_id(Schedule),
    cv.Optional(CONF_NAME):                            cv.string,
    cv.Required(CONF_LAMBDA):                          cv.returning_lambda,
    cv.Optional(CONF_BYPASS, default=False):           cv.boolean,
    cv.Optional(CONF_REMEMBER_NEXT, default=False):    cv.boolean,
    cv.Optional(CONF_CRONTAB, default=""):             cv.string,
    cv.Optional(CONF_CLEAR_PREFS, default=False):      cv.boolean,
    cv.Optional(CONF_TIME_FORMAT, default=""):         cv.string,
    # TODO: Convert this to a var CONF_FEATURE_SET
    cv.Optional("feature_set", default="basic"):       cv.string,
    
    cv.Optional(CONF_BYPASS_SWITCH): switch.switch_schema(BypassSwitch),
    cv.Optional(CONF_REMEMBER_NEXT_SWITCH): switch.switch_schema(RememberNextSwitch),
    cv.Optional(CONF_CRON_NEXT_SENSOR): text_sensor.text_sensor_schema(CronNextSensor),
    cv.Optional(CONF_CRONTAB_TEXT): text.text_schema(CrontabText),
}).extend(cv.COMPONENT_SCHEMA)


# --- Global state for managing build_src_filter ---
_feature_set = "basic"
_sources_configured = False


# This is a timestamp of when the firmware was built. We use it to make decisions
# during the Preferences initialization functions during the first-boot after flashing.
# Since we only need the timestamp once, we do it here, outside of the to_code() method.
# NOTE: This is number seconds since epoch. We round() to chop off the decimal places.
#
assign_global_timestamp = cg.RawStatement(f'esphome::dynamic_cron::TIMESTAMP = {round(time())};\n')
cg.add(assign_global_timestamp)

# This won't print proprtly at the beginning of the main.cpp setup() function.
#print_version = cg.RawStatement(f'esphome::dynamic_cron::printVersion();\n')
#cg.add(print_version)

# For debugging.
# print(CONFIG_SCHEMA)
# print(yaml.dump(CONFIG_SCHEMA, default_flow_style=False, sort_keys=False))


# This gets called for each item in the dynamic_cron:[] array in the yaml config.
#
async def to_code(config):
    # print("=== DYNAMIC_CRON to_code() START ===")
    # print("raw validated config keys:", list(config.keys()))
    # print("raw validated config repr:", config)

    global _feature_set, _sources_configured
    
    # Only configures sources once, using the first instance's feature_set.
    # We need to do that, since there are global compilation settings,
    # not per-instance component settings.
    if not _sources_configured:
        _feature_set = config.get("feature_set", "basic")

        if _feature_set == "tests":
            cg.add_define("COMPILE_TESTS")
            
            cg.add_library(
                name="Unity",
                #repository="https://github.com/ThrowTheSwitch/Unity.git",
                repository=None,
                version="^2.5.2",
            )
                        
        _sources_configured = True

    
    schedule_name = str(config.get(CONF_NAME, config.get(CONF_ID)))
    
    if CONF_ID in config:
        id_ = config[CONF_ID].id
    else:
        id_ = sanitize(snake_case(config[CONF_NAME].id))
        
    #print(f'Schedule name, id: {schedule_name}, {id_}')
        
    lamb = await cg.process_lambda(
        # The 3rd param here is the lambda capture flag to be passed as the [<flag>] part of the c++ lambda.
        # It defaults to [=], which we don't want, since we're capturing as a function-pointer.
        # If you pass any vars through the lambda capture, c++ won't be able to convert
        # to a function pointer. And we like function pointer arg type, since it can receive a lambda OR function-pointer.
        # See here: https://stackoverflow.com/questions/23162654/c11-lambda-functions-implicit-conversion-to-bool-vs-stdfunction
        config[CONF_LAMBDA], [], '', return_type=bool
    )
    
    # Creates component class instance.
    # See for docs: https://github.com/esphome/esphome/blob/b7b2f3e61cabfcd71dbe891e8affdbd2e5128e9e/esphome/core/__init__.py#L321
    var = cg.new_Pvariable(config[CONF_ID], schedule_name, id_, lamb)
    await cg.register_component(var, config)
    
    # Sets defaults for user data.
    cg.add(var.setBypassDefault(config[CONF_BYPASS]))
    cg.add(var.setRememberNextDefault(config[CONF_REMEMBER_NEXT]))
    cg.add(var.setCrontabDefault(config[CONF_CRONTAB]))
    cg.add(var.setClearPrefs(config[CONF_CLEAR_PREFS]))
    cg.add(var.setTimeFormatDefault(config[CONF_TIME_FORMAT]))

    # Auto-detect POSIX TZ string so libc localtime/mktime (used by croncpp) are correct.
    try:
        tz_str = detect_tz()
        _LOGGER.info("dynamic_cron: auto-detected timezone '%s'", tz_str)
        cg.add(var.setTimezone(tz_str))
    except cv.Invalid:
        _LOGGER.warning(
            "dynamic_cron: could not auto-detect timezone. "
            "Scheduling may use wrong time offset."
        )
    
    
    ###  ESPHome Entities/Controls/Display  ###
    
    # Bypass switch
    if CONF_BYPASS_SWITCH in config:
      sw_config = config[CONF_BYPASS_SWITCH]
    
    else:
      sw_config = {
        CONF_NAME:    f'{schedule_name} disabled',
        CONF_ID:      f'{id_}_disabled',
        CONF_ICON:    "mdi:timer-off-outline"
      }
      sw_config = switch.switch_schema(BypassSwitch)(sw_config)
    
    # sw_config.setdefault(CONF_NAME, f"{schedule_name} disabled")
    # sw_config.setdefault(CONF_ID, f"{id_}_disabled")
    
    sw = await switch.new_switch(sw_config)
    cg.add(var.set_bypass_switch(sw))
    cg.add(sw.set_schedule(var))
    
    
    # Remember Next switch
    if CONF_REMEMBER_NEXT_SWITCH in config:
      rem_config = config[CONF_REMEMBER_NEXT_SWITCH]
    else:
      rem_config = {
        CONF_NAME:    f'{schedule_name} remember next',
        CONF_ID:      f'{id_}_remember_next',
        CONF_ICON:    "mdi:memory"
      }
      rem_config = switch.switch_schema(RememberNextSwitch)(rem_config)
    
    rem = await switch.new_switch(rem_config)
    cg.add(var.set_remember_next_switch(rem))
    cg.add(rem.set_schedule(var))
    
    
    # Next Run sensor (display)
    if CONF_CRON_NEXT_SENSOR in config:
      ts_config = config[CONF_CRON_NEXT_SENSOR]
    else:
      ts_config = {
        CONF_NAME:    f'{schedule_name} next run',
        CONF_ID:      f'{id_}_next_run',
        CONF_ICON:    "mdi:timer-outline"
      }
      ts_config = text_sensor.text_sensor_schema(CronNextSensor)(ts_config)
    
    ts = await text_sensor.new_text_sensor(ts_config)
    cg.add(var.set_cron_next_sensor(ts))
    cg.add(ts.set_schedule(var))
    
    # Crontab text (data entry field)
    if CONF_CRONTAB_TEXT in config:
      txt_config = config[CONF_CRONTAB_TEXT]
    else:
      txt_config = {
        CONF_NAME:    f'{schedule_name} crontab',
        CONF_ID:      f'{id_}_crontab',
        CONF_MODE:    'text',
        CONF_ICON:    "mdi:calendar-clock-outline"
      }
      txt_config = text.text_schema(CrontabText)(txt_config)
    
    txt = await text.new_text(txt_config)
    cg.add(var.set_crontab_text(txt))
    cg.add(txt.set_schedule(var))
