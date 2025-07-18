import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)
from esphome.core import coroutine_with_priority, CORE
from esphome import automation, controller
from esphome.components.mqtt import MQTTClientComponent, MQTTMessage
from esphome.components import logger
from . import homie_schema
from types import LambdaType
import os, re

MQTT_CLIENT = "mqtt_client"
HOMIE_DEVICE = "homie_device"

AUTO_LOAD = ["mqtt"]
DEPENDENCIES = ["network"]

class CONFIG:
    PREFIX="prefix"
    QOS="qos"
    RETAINED="retained"
    LOG_LEVEL = "log_level"
    STATS_INTERVAL = "stats_interval"


mqtt_homie_ns = cg.esphome_ns.namespace("mqtt_homie")
HomieClient = mqtt_homie_ns.class_("HomieClient", cg.Component)
HomieDevice = mqtt_homie_ns.class_("HomieDevice", cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HomieClient),
            cv.GenerateID(HOMIE_DEVICE): cv.declare_id(HomieDevice),
            cv.GenerateID(MQTT_CLIENT): cv.use_id(MQTTClientComponent),
            cv.Optional(CONFIG.PREFIX, default="homie"): cv.string,

            cv.Optional(CONFIG.STATS_INTERVAL, default="60s"): cv.update_interval,

            cv.Optional(CONFIG.QOS, default="1"): homie_schema.qos,
            cv.Optional(CONFIG.RETAINED, default="true"): cv.boolean,

            cv.Optional(CONFIG.LOG_LEVEL, default="warn"): logger.is_log_level,
        }
    ).extend(cv.COMPONENT_SCHEMA).extend(cv.polling_component_schema("1s")),
)

def make_homie_message(config, topic, payload):
    if config is None:
        return cg.optional(cg.TemplateArguments(MQTTMessage))

    prefix = config[CONFIG.PREFIX]

    exp = cg.StructInitializer(
        MQTTMessage,
        ("topic", f"{prefix}/{CORE.name}/{topic}"),
        ("payload", payload),
        ("qos", config[CONFIG.QOS]),
        ("retain", config[CONFIG.RETAINED]),
    )
    return exp

@coroutine_with_priority(45.0)
async def to_code(config):
    mqtt_client = await cg.get_variable(config[MQTT_CLIENT])

    homie_client = cg.new_Pvariable(config[CONF_ID], mqtt_client)
    await cg.register_component(homie_client, config)

    homie_device = cg.new_Pvariable(config[HOMIE_DEVICE])
    await cg.register_component(homie_device, config)

    cg.add(mqtt_client.set_last_will(make_homie_message(config, "$state", "lost")))

    cg.add(homie_device.set_stats_interval(config[CONFIG.STATS_INTERVAL]))

    cg.add(homie_client.setup_logging(logger.LOG_LEVELS[config[CONFIG.LOG_LEVEL]]))
    cg.add(homie_client.start_homie(homie_device,
                                    config[CONFIG.PREFIX],
                                    config[CONFIG.QOS],
                                    config[CONFIG.RETAINED],
                                    ))


class HomieNodeBase:
    pass

class HomieController(controller.BaseController):
    CONTROLLER_NAME = "homie"
    CONF_HOMIE_ID = "homie_id"

    CLASS_TYPE = {
        "esphome/switch": "Switch",
        "esphome/sensor": "Sensor",
        "esphome/binary_sensor": "BinarySensor",
        "esphome/number": "Number",
        "esphome/alarm_control_panel": "AlarmControlPanel",
        "esphome/button": "Button",
        "esphome/climate": "Climate",
        "esphome/cover": "Cover",
        "esphome/date": "Date",
        "esphome/time": "Time",
        "esphome/date_time": "DateTime",
        "esphome/event": "Event",
        "esphome/fan": "Fan",
        "esphome/light": "Light",
        "esphome/lock": "Lock",
        "esphome/select": "Select",
        "esphome/text": "Text",
        "esphome/text_sensor": "TextSensor",
        "esphome/update": "Update",
        "esphome/valve": "Valve",
    }

    def __init__(self):
        self.known_classes = self.CLASS_TYPE
        pass

    def extend_component_schema(self, component: str, schema):
        class_type = self.known_classes[component]
        if isinstance(class_type, str):
            NodeTemplate = mqtt_homie_ns.class_(f"HomieNode{class_type}", cg.Component)
        elif isinstance(class_type, LambdaType):
            NodeTemplate = class_type()
        else:
            NodeTemplate = class_type

        return schema.extend(
            {
                cv.OnlyWith(self.CONF_HOMIE_ID, "mqtt_homie"): cv.declare_id(NodeTemplate),
                cv.GenerateID(HOMIE_DEVICE): cv.use_id(HomieDevice),
            }
        )

    def register_node_class(self, node: HomieNodeBase):
        self.known_classes = self.known_classes | node.CLASS_TYPE

    async def register_component(self, component: str, var, config):
        node_id = config.get(self.CONF_HOMIE_ID)
        if not node_id:
            return
        node = cg.new_Pvariable(node_id, var)
        await cg.register_component(node, {})
        homie_device = await cg.get_variable(config[HOMIE_DEVICE])
        cg.add(homie_device.attach_node(node))

def make_homie_cpp_merged():
    FILES = [
        "utils.h",
        "datatype.h",
        "device_state.h",
        "mqtt_event_handler.h",
        "client_event_handler.h",
        "mqtt_client.h",
        "property.h",
        "node.h",
        "device.h",
        "client.h",
    ]
    base_path = os.path.dirname(os.path.realpath(__file__))
    with open(os.path.join(base_path, "homie-cpp-merged-generated.h"), "w") as output:
        output.write("#pragma once\n")
        output.write("#include <set>\n")
        output.write("#include <memory>\n")
        output.write("#include <map>\n")
        output.write("#include <string>\n")
        output.write("#include <vector>\n")
        for f in FILES:
            with open(os.path.join(base_path, "homie-cpp", f), "rb") as source:
                content = source.read().decode("utf-8")
                #remove all 'pragma once' and includes
                content = re.sub(r'#(pragma once|include).*\n', '', content)
                output.write(content)
                output.write('\n')





controller.register_secondary_controller(HomieController())
make_homie_cpp_merged()
