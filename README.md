Smarkant - Convert your IKEA Bekant table into an IoT device
============================================================

This work extends the [IKEA-Hackant](https://github.com/robin7331/IKEA-Hackant)
project from [Robin Reiter](https://github.com/robin7331), who reverse engineered
the controller board of an
[IKEA Bekant](http://www.ikea.com/us/en/catalog/products/S59022528) motorized
adjustable table. He added an Arduino board and four buttons to the controller,
so that you can store two table positions and easily recall them.

You can find Robin's work in his
[GitHub repository](https://github.com/robin7331/IKEA-Hackant).
[This Video](https://www.youtube.com/watch?v=AB75AxprXqQ) shows his project
development and the operation of the modified table.

In the Smarkant project i added an ESP-12E to the ATmega328P and communication
between the ESP8266 and the ATmega328P is done through I2C (I first tried to compile
the Arduino code targeting the ESP8266 platform, but since the LIN bus code
directly accesses the ATmega hardware, i've canceled this approach). The ESP8266
gives us WiFi and quite some processing power to implement a REST API to control
our table remotely. This REST interface could be used to implement a smartphone
app or a desktop widget for controlling the table. We could also implement a web
app directly on the ESP8266.

In this project the ESP8266 establishes an MQTT connection to the Amazon IoT
backend and listens for changes to the Smarkant device shadow object.

This project also contains an Alexa skill and an AWS Lamda function that sends
a REST message to the AWS IoT device which in turn sends a message to the table.
This allows us to control the table with voice commands (one can certainly argue
about whether this is more convenient than pressing a button).

The following image shows an overview of this system architecture:

![alt tag](https://github.com/grappendorf/smarkant/raw/master/smarkant-architecture.png)

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
