# homeID

## What does it do?

homeID Allows you to use a wiegand RFID reader such as a HID Prox reader, with an ESP8266 board such as NodeMCU or Wemos D1 Mini, to publish the card ID number to an MQTT server for use with access control!

The primary use case for this will be access control, you can turn a simple reader, into an MQTT enabled, IoT device, that can publish the ID of the tag it scans to an MQTT server, for a Home Automation platform to utilise.

You could use this to allow one or multiple users to unlock a door by scanning a card, keyfob, implant etc.

It could also be used to fine tune automations, for example, if a user opens a door by scanning their tag, the Home Automation Platform (Eg. Home Assistant) would know what user is opening the door, as it will know which tag was used to unlock, it can then fire a personalised greeting for the person entering through the door!


The parts required for this are very minimal. You really only need the reader itself (which you can grab on eBay for $10-20 or more, and an ESP8266. I like the Wemos D1 Mini because of its small size, but I've had trouble getting the OLED working in conjunction with the reader on the Wemos. The NodeMCU supports both without issues (but in a production enviroment you dont need the OLED!)

The code is written to correctly decode HID formatted tags, but any wiegand reader should work, I have also tested a random $5 'W26 EM410X reader' which works fine spitting out consistent IDs

## Which version do I need?

There are two versions provided in this repo, DisplayOnly and MQTT.

homeID-DisplayOnly will just display the card data on the OLED display and via the serial console(have to implement serial debugging...)

homeID-MQTT will connect to your Wi-Fi, then to your MQTT server, and publish the scanned tag's Card Code (ID) to the MQTT topic yhou specify.


## What you will need:

- ESP8266 with Hardware Interupt pins broken out (Such as NodeMCU 1.0 or Wemos D1 Mini)
- 5v compatible Wiegand reader (Such as a HID ProxPoint Plus, HID MiniProx or HID ThinLine II)
- 128*64 I2C OLED Display (Optional)

## How to wire everything up

To hook everything up, refer to the below pinout and wiring diagram:

**Wemos D1 Mini**
```
D2 > DATA1 (WHITE)
D1 > DATA0 (GREEN)
GND > GND (BLACK)
5V > 5V (RED)
```

**Node MCU 1.0**
```
// Reader
D2 > DATA1 (WHITE)
D1 > DATA0 (GREEN)
GND > GND (BLACK)
VV > 5V (RED)

// OLED
D6 > SDA
D7 > SCK
GND > GND
3V > VDD
```

[Wiring Diagram here]

## To Do

- [x] OTA Upgrades
- [x] Serial Debugging on OLED Only version
- [ ] Support Facility code / raw hex to MQTT
- [ ] Publish in JSON
- [ ] Wiring Diagram
- [ ] Video demo / photos
