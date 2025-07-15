# homie-cpp
Homie C++ header only library

ORIGINAL SOURCE FROM: https://github.com/Thalhammer/homie-cpp
ALL CREDITS TO ORIGINAL CREATOR
ORIGINAL LICENSE: MIT
BASED ON COMMIT: cd9276b2656701cf48c41de1bf4b4cfd5dcf0b0e

Modified for arduino needs

Supports Homie version 3.0 (redesign branch) both as a device and a master role.
It is not built around a specific mqtt library, so you can use what ever you like to.

#### Devicemode
In device mode you can publish a device and react to changes sent via mqtt.
Because mqtt allows only one testatment topic you need multiple connections
to the broker if you want to implement more than one device.

#### Master
Allows you to discover devices connected to a broker and set properties.
One connection can be used for multiple master instances (e.g. for multiple basetopics)
because we do not need a testament in master mode.
