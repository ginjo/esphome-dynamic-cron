from time import time
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, text, text_sensor
from esphome.helpers import sanitize, snake_case
from esphome.const import (
                      CONF_ID,
                      CONF_LAMBDA,
                      CONF_NAME,
                      CONF_MODE,
                      )

import yaml

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

CONF_BYPASS_SWITCH        = "bypass_switch"
CONF_REMEMBER_NEXT_SWITCH = "remember_next_switch"
CONF_CRON_NEXT_SENSOR     = "cron_next_sensor"
CONF_CRONTAB_TEXT         = "crontab_text"

cg.add_build_flag("-std=gnu++17")
cg.add_build_flag("-fexceptions")
cg.add_platformio_option("build_unflags", ["-fno-exceptions", "-std=gnu++11"])

cg.add_library(
    name="Croncpp",
    repository="https://github.com/mariusbancila/croncpp.git",
    version=None,
)

cg.add_library(
    name="Preferences",
    repository=None,
    version=None,
)

# We need this, if we want to build/load/run Unity tests withing esphome firmware.
cg.add_library(
    name="Unity",
    #repository="https://github.com/ThrowTheSwitch/Unity.git",
    repository=None,
    version="^2.5.2",
)

dynamiccron_ns      = cg.esphome_ns.namespace('dynamic_cron')
# I don't think the rest of these classes are used in the py code.
# Update: I think these can be used to inject code that instantiates these classes.
#         See the esphome 'time' component for examples.
#         https://github.com/esphome/esphome/tree/dev/esphome/components/time
Schedule            = dynamiccron_ns.class_('Schedule', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID):                            cv.declare_id(Schedule),
    cv.Optional(CONF_NAME):                            cv.string,
    cv.Required(CONF_LAMBDA):                          cv.returning_lambda,
    cv.Optional(CONF_BYPASS, default=False):           cv.boolean,
    cv.Optional(CONF_REMEMBER_NEXT, default=False):    cv.boolean,
    cv.Optional(CONF_CRONTAB, default=""):             cv.string,
    cv.Optional(CONF_CLEAR_PREFS, default=False):      cv.boolean,
    cv.Optional(CONF_TIME_FORMAT, default=""):         cv.string,
    
    cv.Optional(CONF_BYPASS_SWITCH): switch.switch_schema(switch.Switch),
    cv.Optional(CONF_REMEMBER_NEXT_SWITCH): switch.switch_schema(switch.Switch),
    cv.Optional(CONF_CRON_NEXT_SENSOR): text_sensor.text_sensor_schema(Schedule),
    cv.Optional(CONF_CRONTAB_TEXT): text.text_schema(Schedule),
}).extend(cv.COMPONENT_SCHEMA)

FINAL_VALIDATE_SCHEMA = cv.All(
    CONFIG_SCHEMA
)


### cg.add() puts code at top of main.cpp setup() function.
### cg.add_global() puts code at top of main.cpp.

# This is a timestamp of when the firmware was built. We use it to make decisions
# during the Preferences initialization functions during the first-boot after flashing.
# Since we only need the timestamp once, we do it here, outside of the to_code() method.
# NOTE: This is number seconds since epoch. We round() to chop off the decimal places.
#
assign_global_timestamp = cg.RawStatement(f'esphome::dynamic_cron::TIMESTAMP = {round(time())};\n')
cg.add(assign_global_timestamp)

print_version = cg.RawStatement(f'esphome::dynamic_cron::printVersion();\n')
cg.add(print_version)

# print(CONFIG_SCHEMA)
# print(yaml.dump(CONFIG_SCHEMA, default_flow_style=False, sort_keys=False))

# This gets called for each item in the dynamic_cron:[] array in the yaml config.
#async def to_code(config):
# We use this form so cg.declare_id()() will work.
# See for docs: https://github.com/esphome/esphome/blob/dev/esphome/cpp_generator.py
async def to_code(config):
    print("=== DYNAMIC_CRON to_code() START ===")
    print("raw validated config keys:", list(config.keys()))
    print("raw validated config repr:", config)
    
    name = str(config.get(CONF_NAME, config.get(CONF_ID)))
    
    if CONF_ID in config:
        id_ = config[CONF_ID].id
    else:
        id_ = sanitize(snake_case(config[CONF_NAME].id))
        
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
    var = cg.new_Pvariable(config[CONF_ID], name, id_, lamb)
    await cg.register_component(var, config)
    
    # Sets defaults for user data.
    cg.add(var.setBypassDefault(config[CONF_BYPASS]))
    cg.add(var.setRememberNextDefault(config[CONF_REMEMBER_NEXT]))
    cg.add(var.setCrontabDefault(config[CONF_CRONTAB]))
    cg.add(var.setClearPrefs(config[CONF_CLEAR_PREFS]))
    cg.add(var.setTimeFormatDefault(config[CONF_TIME_FORMAT]))
    
    
    ### Entities/Controls/Display
    
    # Bypass switch
    sw_config = dict(config.get(CONF_BYPASS_SWITCH, {})) # copy so we don't mutate the original
    sw_config.setdefault(CONF_NAME, f"{name} disable")
    sw_config.setdefault(CONF_ID, f"{id_}_bypass")
    
    print("SW_CONFIG BEFORE NEW_SWITCH:", sw_config)
    print("type(sw_config.get(CONF_ID)):", type(sw_config.get(CONF_ID)), repr(sw_config.get(CONF_ID)))
    
    sw_config = switch.switch_schema(switch.Switch)(sw_config)
    sw = await switch.new_switch(sw_config)
    cg.add(var.set_bypass_switch(sw))
    
    # Remember Next switch
    rem_config = dict(config.get(CONF_REMEMBER_NEXT_SWITCH, {}))
    rem_config.setdefault(CONF_NAME, f"{name} remember next")
    rem_config.setdefault(CONF_ID, f"{id_}_remember_next")
    rem_config = switch.switch_schema(switch.Switch)(rem_config)
    rem = await switch.new_switch(rem_config)
    cg.add(var.set_remember_next_switch(rem))

    # Next Run sensor (display)
    ts_config = dict(config.get(CONF_CRON_NEXT_SENSOR, {}))
    ts_config.setdefault(CONF_NAME, f"{name} next run")
    ts_config.setdefault(CONF_ID, f"{id_}_next_run")
    ts_config = text_sensor.text_sensor_schema(text_sensor.TextSensor)(ts_config)
    ts = await text_sensor.new_text_sensor(ts_config)
    cg.add(var.set_cron_next_sensor(ts))
    
    # Crontab text (data entry field)
    txt_config = dict(config.get(CONF_CRONTAB_TEXT, {}))
    txt_config.setdefault(CONF_NAME, f"{name} crontab")
    txt_config.setdefault(CONF_ID, f"{id_}_crontab")
    txt_config.setdefault(CONF_MODE, 'text')
    txt_config = text.text_schema(text.Text)(txt_config)
    txt = await text.new_text(txt_config)
    cg.add(var.set_crontab_text(txt))
