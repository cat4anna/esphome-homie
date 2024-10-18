import esphome.codegen as cg
import esphome.config_validation as cv
from ..mqtt_client import MQTTClientComponent, MQTTMessage
from esphome.const import (
    CONF_ID,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_BK72XX,
    PLATFORM_RTL87XX,
)
from esphome.core import coroutine_with_priority, CORE
from esphome import automation, controler
from esphome.controler import ComponentType

MQTT_CLIENT = "mqtt_client"
HOMIE_DEVICE = "homie_device"

AUTO_LOAD = ["mqtt_client"]
DEPENDENCIES = ["wifi"]

CONF_HOMIE_PREFIX="prefix"
CONF_HOMIE_QOS="qos"
CONF_HOMIE_RETAINED="retained"

mqtt_homie_ns = cg.esphome_ns.namespace("mqtt_homie")
HomieClient = mqtt_homie_ns.class_("HomieClient", cg.Component)
HomieDevice = mqtt_homie_ns.class_("HomieDevice", cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HomieClient),
            cv.GenerateID(HOMIE_DEVICE): cv.declare_id(HomieDevice),
            cv.GenerateID(MQTT_CLIENT): cv.use_id(MQTTClientComponent),
            cv.Optional(CONF_HOMIE_PREFIX, default="homie/"): cv.string,

            cv.Optional(CONF_HOMIE_QOS, default="1"): cv.int_range(0,2),
            cv.Optional(CONF_HOMIE_RETAINED, default="true"): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA).extend(cv.polling_component_schema("10s")),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX, PLATFORM_RTL87XX]),
)

@coroutine_with_priority(45.0)
async def to_code(config):
    # cg.add_define("USE_SECONDARY_CONTROLER")

    mqtt_client = await cg.get_variable(config[MQTT_CLIENT])

    homie_client = cg.new_Pvariable(config[CONF_ID], mqtt_client)
    await cg.register_component(homie_client, config)

    homie_device = cg.new_Pvariable(config[HOMIE_DEVICE])
    await cg.register_component(homie_device, config)

    cg.add(homie_client.start_homie(homie_device,
                                    config[CONF_HOMIE_PREFIX],
                                    config[CONF_HOMIE_QOS],
                                    config[CONF_HOMIE_RETAINED],
                                    ))

class HomieController(controler.BaseControler):
    CONF_HOMIE_ID = "homie_id"

    def __init__(self):
        pass

    def extend_component_schema(self, component: ComponentType, schema):
        NodeTemplate = mqtt_homie_ns.class_(f"HomieNode{component.value.capitalize()}", cg.Component)
        return schema.extend(
            {
                cv.OnlyWith(self.CONF_HOMIE_ID, "mqtt_homie"): cv.declare_id(NodeTemplate),
                cv.GenerateID(HOMIE_DEVICE): cv.use_id(HomieDevice),
            }
        )

    async def register_component(self, component: ComponentType, var, config):
        node_id = config.get(self.CONF_HOMIE_ID)
        if not node_id:
            return
        node = cg.new_Pvariable(node_id, var)
        await cg.register_component(node, {})
        homie_device = await cg.get_variable(config[HOMIE_DEVICE])
        cg.add(homie_device.attach_node(node))

controler.add_secondary_controller(HomieController())

