Smarkant
========

smarkant-arduino
----------------

This directory contains the firmware code for the ATmega328P microcontroller.
The firmware uses the Arduino framework and was developed with the PlatformIO
development environment. It uses the libraries Bounce2 and LinProcessor,
which are contained in the lib sub-directory.

smarkant-esp
------------

This directory contains the firmware code for the ESP8266 (ESP-12E) microcontroller.
The firmware uses the Arduino framework and was developed with the PlatformIO
development environment. It uses the libraries ArduinoJson, AWSSDK, AWSWebSockets,
and Paho MQTTClient, which are contained in the lib sub-directory.

smarkant-alexa
--------------

This directory contains an Alexa skill definition (intent schema and sample
utterances) and a Lambda function, written in JavaScript. The Lambda function
controls the AWS IoT shadow object of the Smarkant thing.

smarkant-kicad
--------------

This directory contains KiCad schematic files for the Smarkant controller.

smarkant-case
-------------

This directory contains 123D Design and STL files of a case for the
Smarkant controller.
