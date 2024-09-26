
# # NOTE: Imports do not load files or paths into the build directory.
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, text, text_sensor
from esphome.helpers import sanitize, snake_case
from esphome.const import (
                      CONF_ID,
                      CONF_LAMBDA,
                      CONF_NAME
                      )
                          
# Loads paths/files into the build directory.
AUTO_LOAD = ['switch', 'text', 'text_sensor']
MULTI_CONF = True

# This isn't working, I think because there is no 'add_build_unflag' to get rid of the
# existing conflicting flags.
# See https://github.com/esphome/esphome/blob/21fbbc5fb9477704be8b1ec8293d2a729f7a0072/esphome/cpp_generator.py#L606

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

dynamiccron_ns      = cg.esphome_ns.namespace('dynamic_cron')
Schedule            = dynamiccron_ns.class_('Schedule', cg.Component)
BypassSwitch        = dynamiccron_ns.class_('BypassSwitch', switch.Switch, cg.Component)
CrontabTextField    = dynamiccron_ns.class_('CrontabTextField', text.Text, cg.Component)
CronNextSensor      = dynamiccron_ns.class_('CronNextSensor', text_sensor.TextSensor, cg.Component)
IgnoreMissedSwitch  = dynamiccron_ns.class_('IgnoreMissedSwitch', switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.Optional(CONF_NAME):     cv.string,
    cv.GenerateID(CONF_ID):     cv.declare_id(Schedule),
    cv.Required(CONF_LAMBDA):   cv.returning_lambda 
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    
    # name = str(config[CONF_NAME])
    # id_  = str(config[CONF_ID])
    
    name = str(config.get(CONF_NAME, config.get(CONF_ID)))
    
    if CONF_ID in config:
        id_ = str(config[CONF_ID])
    else:
        id_ = str(sanitize(snake_case(config[CONF_NAME])))
        
    lamb = await cg.process_lambda(
        # The 3rd param here is the lambda capture flag to be passed as the [<flag>] part of the c++ lambda.
        # It defaults to [=], which we don't want, since we're capturing as a function-pointer.
        # If you pass any vars through the lambda capture, c++ won't be able to convert
        # to a function pointer. And we like function pointer arg type, since it can receive a lambda OR function-pointer.
        # See here: https://stackoverflow.com/questions/23162654/c11-lambda-functions-implicit-conversion-to-bool-vs-stdfunction
        config[CONF_LAMBDA], [], '', return_type=bool
    )

    var = cg.new_Pvariable(config[CONF_ID], name, id_, lamb)
    await cg.register_component(var, config)


    bypass_switch = cg.RawStatement(
      f'esphome::dynamic_cron::BypassSwitch *bypass_switch_{id_} = new esphome::dynamic_cron::BypassSwitch({id_});\n' +
      f'bypass_switch_{id_}->set_name("{name} - Disable");\n' +
      f'bypass_switch_{id_}->set_object_id("bypass_switch_{id_}");\n'
    )
    cg.add(bypass_switch)
    
    ### This does not work as written.
    #cg.add(await bypass_switch.set_object_id("var_bypass_switch"))
    
    
    ignore_missed_switch = cg.RawStatement(
      f'esphome::dynamic_cron::IgnoreMissedSwitch *ignore_missed_switch_{id_} = new esphome::dynamic_cron::IgnoreMissedSwitch({id_});\n'
      f'ignore_missed_switch_{id_}->set_name("{name} - Ignore Missed");\n' +
      f'ignore_missed_switch_{id_}->set_object_id("ignore_missed_switch_{id_}");\n'
    )
    cg.add(ignore_missed_switch)
    
    
    cron_next_sensor = cg.RawStatement(
      f'esphome::dynamic_cron::CronNextSensor *cron_next_sensor_{id_} = new esphome::dynamic_cron::CronNextSensor({id_});\n' +
      f'cron_next_sensor_{id_}->set_name("{name} - Next Run");\n' +
      f'cron_next_sensor_{id_}->set_object_id("cron_next_sensor_{id_}");\n'
    )
    cg.add(cron_next_sensor)
    
    
    crontab_text_field = cg.RawStatement(
      f'esphome::dynamic_cron::CrontabTextField *crontab_text_field_{id_} = new esphome::dynamic_cron::CrontabTextField({id_});\n' +
      f'crontab_text_field_{id_}->set_name("{name} - Crontab");\n' +
      f'crontab_text_field_{id_}->set_object_id("crontab_text_field_{id_}");\n'
    )
    cg.add(crontab_text_field)
    

    # EXAMPLES
    # await switch.register_switch(var.bypass_switch, config)
    # await cg.register_component(var.bypass_switch, config)


