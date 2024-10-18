import re

from esphome import automation
from esphome.automation import Condition
import esphome.codegen as cg
from esphome.components import logger
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv

from esphome.const import (
    CONF_ID,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_BK72XX,
    PLATFORM_RTL87XX,
    CONF_BROKER,
    CONF_PORT,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_CLIENT_ID,
    CONF_TOPIC_PREFIX,
    CONF_BIRTH_MESSAGE,
    CONF_WILL_MESSAGE,
    CONF_SHUTDOWN_MESSAGE,
    CONF_LOG_TOPIC,
    CONF_TOPIC,CONF_QOS,CONF_RETAIN,CONF_PAYLOAD,CONF_LEVEL,
    CONF_KEEPALIVE,CONF_REBOOT_TIMEOUT
)
from esphome.core import coroutine_with_priority, CORE

CONF_CLEAN_SESSION = "clean_session"
CONF_MQTT_ID = "mqtt_id"


def AUTO_LOAD():
    if CORE.is_esp8266 or CORE.is_libretiny:
        return ["async_tcp", "json"]
    return ["json"]

DEPENDENCIES = ["network"]

mqtt_client_ns = cg.esphome_ns.namespace("mqtt_client")
MQTTClientComponent = mqtt_client_ns.class_("MQTTClientComponent", cg.Component)

MQTTMessage = mqtt_client_ns.struct("MQTTMessage")

def _valid_topic(value):
    """Validate that this is a valid topic name/filter."""
    if value is None:  # Used to disable publishing and subscribing
        return ""
    if isinstance(value, dict):
        raise cv.Invalid("Can't use dictionary with topic")
    value = cv.string(value)
    try:
        raw_value = value.encode("utf-8")
    except UnicodeError as err:
        raise cv.Invalid("MQTT topic name/filter must be valid UTF-8 string.") from err
    if not raw_value:
        raise cv.Invalid("MQTT topic name/filter must not be empty.")
    if len(raw_value) > 65535:
        raise cv.Invalid(
            "MQTT topic name/filter must not be longer than 65535 encoded bytes."
        )
    if "\0" in value:
        raise cv.Invalid("MQTT topic name/filter must not contain null character.")
    return value


def subscribe_topic(value):
    """Validate that we can subscribe using this MQTT topic."""
    value = _valid_topic(value)
    for i in (i for i, c in enumerate(value) if c == "+"):
        if (i > 0 and value[i - 1] != "/") or (
            i < len(value) - 1 and value[i + 1] != "/"
        ):
            raise cv.Invalid(
                "Single-level wildcard must occupy an entire level of the filter"
            )

    index = value.find("#")
    if index != -1:
        if index != len(value) - 1:
            # If there are multiple wildcards, this will also trigger
            raise cv.Invalid(
                "Multi-level wildcard must be the last "
                "character in the topic filter."
            )
        if len(value) > 1 and value[index - 1] != "/":
            raise cv.Invalid(
                "Multi-level wildcard must be after a topic level separator."
            )

    return value


def publish_topic(value):
    """Validate that we can publish using this MQTT topic."""
    value = _valid_topic(value)
    if "+" in value or "#" in value:
        raise cv.Invalid("Wildcards can not be used in topic names")
    return value


def mqtt_payload(value):
    if value is None:
        return ""
    return cv.string(value)


def mqtt_qos(value):
    try:
        value = int(value)
    except (TypeError, ValueError):
        # pylint: disable=raise-missing-from
        raise cv.Invalid(f"MQTT Quality of Service must be integer, got {value}")
    return cv.one_of(0, 1, 2)(value)

def validate_message_just_topic(value):
    value = cv.publish_topic(value)
    return MQTT_MESSAGE_BASE({CONF_TOPIC: value})


MQTT_MESSAGE_BASE = cv.Schema(
    {
        cv.Required(CONF_TOPIC): publish_topic,
        cv.Optional(CONF_QOS, default=0): mqtt_qos,
        cv.Optional(CONF_RETAIN, default=True): cv.boolean,
    }
)

MQTT_MESSAGE_TEMPLATE_SCHEMA = cv.Any(
    None, MQTT_MESSAGE_BASE, validate_message_just_topic
)

MQTT_MESSAGE_SCHEMA = cv.Any(
    None,
    MQTT_MESSAGE_BASE.extend(
        {
            cv.Required(CONF_PAYLOAD): mqtt_payload,
        }
    ),
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MQTTClientComponent),

            cv.Required(CONF_BROKER): cv.string_strict,
            cv.Optional(CONF_PORT, default=1883): cv.port,
            cv.Optional(CONF_USERNAME, default=""): cv.string,
            cv.Optional(CONF_PASSWORD, default=""): cv.string,
            cv.Optional(CONF_CLEAN_SESSION, default=False): cv.boolean,
            cv.Optional(CONF_CLIENT_ID): cv.string,

            cv.Optional(CONF_BIRTH_MESSAGE): MQTT_MESSAGE_SCHEMA,
            cv.Optional(CONF_WILL_MESSAGE): MQTT_MESSAGE_SCHEMA,
            cv.Optional(CONF_SHUTDOWN_MESSAGE): MQTT_MESSAGE_SCHEMA,
            cv.Optional(CONF_TOPIC_PREFIX, default=lambda: CORE.name): publish_topic,
            cv.Optional(CONF_LOG_TOPIC): cv.Any(
                None,
                MQTT_MESSAGE_BASE.extend(
                    {
                        cv.Optional(CONF_LEVEL): logger.is_log_level,
                    }
                ),
                validate_message_just_topic,
            ),
            cv.Optional(CONF_KEEPALIVE, default="15s"): cv.positive_time_period_seconds,
            cv.Optional(
                CONF_REBOOT_TIMEOUT, default="15min"
            ): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX, PLATFORM_RTL87XX]),
)

MQTT_PUBLISH_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MQTTClientComponent),
        cv.Required(CONF_TOPIC): cv.templatable(publish_topic),
        cv.Required(CONF_PAYLOAD): cv.templatable(mqtt_payload),
        cv.Optional(CONF_QOS, default=0): cv.templatable(mqtt_qos),
        cv.Optional(CONF_RETAIN, default=False): cv.templatable(cv.boolean),
    }
)

@coroutine_with_priority(40.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Add required libraries for ESP8266 and LibreTiny
    if CORE.is_esp8266 or CORE.is_libretiny:
        # https://github.com/heman/async-mqtt-client/blob/master/library.json
        cg.add_library("heman/AsyncMqttClient-esphome", "2.0.0")

    cg.add_define("USE_MQTT_CLIENT")
    cg.add_define("USE_SECONDARY_CONTROLER")
    cg.add_global(mqtt_client_ns.using)

    cg.add(var.set_broker_address(config[CONF_BROKER]))
    cg.add(var.set_broker_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_clean_session(config[CONF_CLEAN_SESSION]))
    if CONF_CLIENT_ID in config:
        cg.add(var.set_client_id(config[CONF_CLIENT_ID]))
    # cg.add(var.set_topic_prefix(config[CONF_TOPIC_PREFIX]))

    if CONF_BIRTH_MESSAGE in config:
        cg.add(var.set_birth_message(exp_mqtt_message(config[CONF_BIRTH_MESSAGE])))

    if CONF_WILL_MESSAGE in config:
        cg.add(var.set_last_will(exp_mqtt_message(config[CONF_WILL_MESSAGE])))

    if CONF_SHUTDOWN_MESSAGE in config:
        cg.add(var.set_shutdown_message(exp_mqtt_message(config[CONF_SHUTDOWN_MESSAGE])))

    if CONF_LOG_TOPIC in config:
        log_topic = config[CONF_LOG_TOPIC]
        cg.add(var.set_log_message_template(exp_mqtt_message(log_topic)))

        if CONF_LEVEL in log_topic:
            cg.add(var.set_log_level(logger.LOG_LEVELS[log_topic[CONF_LEVEL]]))

# #     if CONF_SSL_FINGERPRINTS in config:
# #         for fingerprint in config[CONF_SSL_FINGERPRINTS]:
# #             arr = [
# #                 cg.RawExpression(f"0x{fingerprint[i:i + 2]}") for i in range(0, 40, 2)
# #             ]
# #             cg.add(var.add_ssl_fingerprint(arr))
# #         cg.add_build_flag("-DASYNC_TCP_SSL_ENABLED=1")

    cg.add(var.set_keep_alive(config[CONF_KEEPALIVE]))
    cg.add(var.set_reboot_timeout(config[CONF_REBOOT_TIMEOUT]))

# #     # esp-idf only
# #     if CONF_CERTIFICATE_AUTHORITY in config:
# #         cg.add(var.set_ca_certificate(config[CONF_CERTIFICATE_AUTHORITY]))
# #         cg.add(var.set_skip_cert_cn_check(config[CONF_SKIP_CERT_CN_CHECK]))
# #         if CONF_CLIENT_CERTIFICATE in config:
# #             cg.add(var.set_cl_certificate(config[CONF_CLIENT_CERTIFICATE]))
# #             cg.add(var.set_cl_key(config[CONF_CLIENT_CERTIFICATE_KEY]))

# #         # prevent error -0x428e
# #         # See https://github.com/espressif/esp-idf/issues/139
# #         add_idf_sdkconfig_option("CONFIG_MBEDTLS_HARDWARE_MPI", False)

# #     if CONF_IDF_SEND_ASYNC in config and config[CONF_IDF_SEND_ASYNC]:
# #         cg.add_define("USE_MQTT_CLIENT_IDF_ENQUEUE")
# #     # end esp-idf

# #     for conf in config.get(CONF_ON_MESSAGE, []):
# #         trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf[CONF_TOPIC])
# #         cg.add(trig.set_qos(conf[CONF_QOS]))
# #         if CONF_PAYLOAD in conf:
# #             cg.add(trig.set_payload(conf[CONF_PAYLOAD]))
# #         await cg.register_component(trig, conf)
# #         await automation.build_automation(trig, [(cg.std_string, "x")], conf)

# #     for conf in config.get(CONF_ON_JSON_MESSAGE, []):
# #         trig = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf[CONF_TOPIC], conf[CONF_QOS])
# #         await automation.build_automation(trig, [(cg.JsonObjectConst, "x")], conf)

# #     for conf in config.get(CONF_ON_CONNECT, []):
# #         trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
# #         await automation.build_automation(trigger, [], conf)

# #     for conf in config.get(CONF_ON_DISCONNECT, []):
# #         trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
# #         await automation.build_automation(trigger, [], conf)


def exp_mqtt_message(config):
    if config is None:
        return cg.optional(cg.TemplateArguments(MQTTMessage))
    exp = cg.StructInitializer(
        MQTTMessage,
        ("topic", config[CONF_TOPIC]),
        ("payload", config.get(CONF_PAYLOAD, "")),
        ("qos", config[CONF_QOS]),
        ("retain", config[CONF_RETAIN]),
    )
    return exp

# # MQTTPublishAction = mqtt_ns.class_("MQTTPublishAction", automation.Action)
# # MQTTPublishJsonAction = mqtt_ns.class_("MQTTPublishJsonAction", automation.Action)
# # MQTTMessageTrigger = mqtt_ns.class_(
# #     "MQTTMessageTrigger", automation.Trigger.template(cg.std_string), cg.Component
# # )
# # MQTTJsonMessageTrigger = mqtt_ns.class_(
# #     "MQTTJsonMessageTrigger", automation.Trigger.template(cg.JsonObjectConst)
# # )
# # MQTTConnectTrigger = mqtt_ns.class_("MQTTConnectTrigger", automation.Trigger.template())
# # MQTTDisconnectTrigger = mqtt_ns.class_(
# #     "MQTTDisconnectTrigger", automation.Trigger.template()
# # )
# # MQTTComponent = mqtt_ns.class_("MQTTComponent", cg.Component)
# # MQTTConnectedCondition = mqtt_ns.class_("MQTTConnectedCondition", Condition)

# # MQTTDiscoveryUniqueIdGenerator = mqtt_ns.enum("MQTTDiscoveryUniqueIdGenerator")
# # MQTT_DISCOVERY_UNIQUE_ID_GENERATOR_OPTIONS = {
# #     "legacy": MQTTDiscoveryUniqueIdGenerator.MQTT_LEGACY_UNIQUE_ID_GENERATOR,
# #     "mac": MQTTDiscoveryUniqueIdGenerator.MQTT_MAC_ADDRESS_UNIQUE_ID_GENERATOR,
# # }

# # MQTTDiscoveryObjectIdGenerator = mqtt_ns.enum("MQTTDiscoveryObjectIdGenerator")
# # MQTT_DISCOVERY_OBJECT_ID_GENERATOR_OPTIONS = {
# #     "none": MQTTDiscoveryObjectIdGenerator.MQTT_NONE_OBJECT_ID_GENERATOR,
# #     "device_name": MQTTDiscoveryObjectIdGenerator.MQTT_DEVICE_NAME_OBJECT_ID_GENERATOR,
# # }

# # def validate_config(value):
# #     # Populate default fields
# #     out = value.copy()
# #     topic_prefix = value[CONF_TOPIC_PREFIX]
# #     # If the topic prefix is not null and these messages are not configured, then set them to the default
# #     # If the topic prefix is null and these messages are not configured, then set them to null
# #     if CONF_BIRTH_MESSAGE not in value:
# #         if topic_prefix != "":
# #             out[CONF_BIRTH_MESSAGE] = {
# #                 CONF_TOPIC: f"{topic_prefix}/status",
# #                 CONF_PAYLOAD: "online",
# #                 CONF_QOS: 0,
# #                 CONF_RETAIN: True,
# #             }
# #         else:
# #             out[CONF_BIRTH_MESSAGE] = {}
# #     if CONF_WILL_MESSAGE not in value:
# #         if topic_prefix != "":
# #             out[CONF_WILL_MESSAGE] = {
# #                 CONF_TOPIC: f"{topic_prefix}/status",
# #                 CONF_PAYLOAD: "offline",
# #                 CONF_QOS: 0,
# #                 CONF_RETAIN: True,
# #             }
# #         else:
# #             out[CONF_WILL_MESSAGE] = {}
# #     if CONF_SHUTDOWN_MESSAGE not in value:
# #         if topic_prefix != "":
# #             out[CONF_SHUTDOWN_MESSAGE] = {
# #                 CONF_TOPIC: f"{topic_prefix}/status",
# #                 CONF_PAYLOAD: "offline",
# #                 CONF_QOS: 0,
# #                 CONF_RETAIN: True,
# #             }
# #         else:
# #             out[CONF_SHUTDOWN_MESSAGE] = {}
# #     if CONF_LOG_TOPIC not in value:
# #         if topic_prefix != "":
# #             out[CONF_LOG_TOPIC] = {
# #                 CONF_TOPIC: f"{topic_prefix}/debug",
# #                 CONF_QOS: 0,
# #                 CONF_RETAIN: True,
# #             }
# #         else:
# #             out[CONF_LOG_TOPIC] = {}
# #     return out


# # def validate_fingerprint(value):
# #     value = cv.string(value)
# #     if re.match(r"^[0-9a-f]{40}$", value) is None:
# #         raise cv.Invalid("fingerprint must be valid SHA1 hash")
# #     return value


# CONFIG_SCHEMA = cv.All(
#     cv.Schema(
#         {
#             cv.GenerateID(): cv.declare_id(MQTTClientComponent),
# #             cv.SplitDefault(CONF_IDF_SEND_ASYNC, esp32_idf=False): cv.All(
# #                 cv.boolean, cv.only_with_esp_idf
# #             ),
# #             cv.Optional(CONF_CERTIFICATE_AUTHORITY): cv.All(
# #                 cv.string, cv.only_with_esp_idf
# #             ),
# #             cv.Inclusive(CONF_CLIENT_CERTIFICATE, "cert-key-pair"): cv.All(
# #                 cv.string, cv.only_on_esp32
# #             ),
# #             cv.Inclusive(CONF_CLIENT_CERTIFICATE_KEY, "cert-key-pair"): cv.All(
# #                 cv.string, cv.only_on_esp32
# #             ),
# #             cv.SplitDefault(CONF_SKIP_CERT_CN_CHECK, esp32_idf=False): cv.All(
# #                 cv.boolean, cv.only_with_esp_idf
# #             ),
# #             cv.Optional(CONF_USE_ABBREVIATIONS, default=True): cv.boolean,

# #             cv.Optional(CONF_SSL_FINGERPRINTS): cv.All(
# #                 cv.only_on_esp8266, cv.ensure_list(validate_fingerprint)
# #             ),

# #             cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
# #                 {
# #                     cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MQTTConnectTrigger),
# #                 }
# #             ),
# #             cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
# #                 {
# #                     cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
# #                         MQTTDisconnectTrigger
# #                     ),
# #                 }
# #             ),
# #             cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(
# #                 {
# #                     cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(MQTTMessageTrigger),
# #                     cv.Required(CONF_TOPIC): cv.subscribe_topic,
# #                     cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
# #                     cv.Optional(CONF_PAYLOAD): cv.string_strict,
# #                 }
# #             ),
# #             cv.Optional(CONF_ON_JSON_MESSAGE): automation.validate_automation(
# #                 {
# #                     cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
# #                         MQTTJsonMessageTrigger
# #                     ),
# #                     cv.Required(CONF_TOPIC): cv.subscribe_topic,
# #                     cv.Optional(CONF_QOS, default=0): cv.mqtt_qos,
# #                 }
# #             ),
#         }
#     ).extend(cv.COMPONENT_SCHEMA),
# #     validate_config,
#     cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_BK72XX]),
# )

# # @automation.register_action(
# #     "mqtt.publish", MQTTPublishAction, MQTT_PUBLISH_ACTION_SCHEMA
# # )

# # MQTT_PUBLISH_JSON_ACTION_SCHEMA = cv.Schema(
# #     {
# #         cv.GenerateID(): cv.use_id(MQTTClientComponent),
# #         cv.Required(CONF_TOPIC): cv.templatable(cv.publish_topic),
# #         cv.Required(CONF_PAYLOAD): cv.lambda_,
# #         cv.Optional(CONF_QOS, default=0): cv.templatable(cv.mqtt_qos),
# #         cv.Optional(CONF_RETAIN, default=False): cv.templatable(cv.boolean),
# #     }
# # )

# # @automation.register_action(
# #     "mqtt.publish_json", MQTTPublishJsonAction, MQTT_PUBLISH_JSON_ACTION_SCHEMA
# # )
# # async def mqtt_publish_json_action_to_code(config, action_id, template_arg, args):
# #     paren = await cg.get_variable(config[CONF_ID])
# #     var = cg.new_Pvariable(action_id, template_arg, paren)
# #     template_ = await cg.templatable(config[CONF_TOPIC], args, cg.std_string)
# #     cg.add(var.set_topic(template_))

# #     args_ = args + [(cg.JsonObject, "root")]
# #     lambda_ = await cg.process_lambda(config[CONF_PAYLOAD], args_, return_type=cg.void)
# #     cg.add(var.set_payload(lambda_))
# #     template_ = await cg.templatable(config[CONF_QOS], args, cg.uint8)
# #     cg.add(var.set_qos(template_))
# #     template_ = await cg.templatable(config[CONF_RETAIN], args, bool)
# #     cg.add(var.set_retain(template_))
# #     return var


# # def get_default_topic_for(data, component_type, name, suffix):
# #     allowlist = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"
# #     sanitized_name = "".join(
# #         x for x in name.lower().replace(" ", "_") if x in allowlist
# #     )
# #     return f"{data.topic_prefix}/{component_type}/{sanitized_name}/{suffix}"

# # @automation.register_condition(
# #     "mqtt.connected",
# #     MQTTConnectedCondition,
# #     cv.Schema(
# #         {
# #             cv.GenerateID(): cv.use_id(MQTTClientComponent),
# #         }
# #     ),
# # )
# # async def mqtt_connected_to_code(config, condition_id, template_arg, args):
# #     paren = await cg.get_variable(config[CONF_ID])
#     # return cg.new_Pvariable(condition_id, template_arg, paren)
