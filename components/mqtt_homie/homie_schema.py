import esphome.config_validation as cv

qos = cv.int_range(0,2)

def homie_config_key(value: str):
    value = cv.string(value)
    value = cv.publish_topic(value)
    if not value.startswith("$"):
        raise cv.Invalid(f"Invalid domain: {value}")
    return value
