# Mock Rain Sensor

This project uses a ESP8266 based relay board to emulate a rain sensor for an inground sprinkler system.
This is the board I used:
![Image of the board](https://images-na.ssl-images-amazon.com/images/I/61YSCKZnOhL._AC_SX679_.jpg)

### Prerequisites


```
A board to flash(duh) and the equipment to flash it, like a usb to uart adapter.

An openweathermaps.com api key, a free key is fine

Latitude and Longitude of where you are retrieving weather data for.
```

### Installing

First run:

```
make menuconfig
```

You will need to configure things like:
Your wifi network ssid and password.
Your openweathermap API key.
The Latitude and Longitude for the place your weather sensor is located.
The serial port that your board is using.

Then run:

```
make flash
```
You may have to put your board in flashing mode, this varies board to board.


### Acknowledgement

To ryaske for this nice diagram of how to flash the board
https://github.com/arendst/Tasmota/issues/453#issuecomment-549166178

### Screenshot
![screenshot](/sc.jpg?raw=true "screenshot")
