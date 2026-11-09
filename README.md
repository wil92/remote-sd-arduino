# Remote SD Card Access over Network

This project provides a simple way to access an SD card connected to a microcontroller over a network. It allows you to
#read and write files on the SD card remotely using standard network protocols.

This project is useful for applications where you need to manage files on an SD card without physically accessing the
device, like 3D printers, IoT devices, or embedded systems.

## Features

- Read and write files on the SD card over the network via HTTP.
- Simple web interface for file management.
- Lightweight and easy to deploy on microcontrollers.

## Requirements

- Python 3.x
  - htmlmin (library for minifying HTML)
- PlatformIO
- ESP32-C3 Super mini or compatible microcontroller

## Installation

1. Clone this repository to your local machine: `git clone`
2. Navigate to the project directory: `cd remote-sd-card-access`
3. Install html minifier: `sudo pip install minify_html`

## Testing

```
// run tests
pio test -e tests -vvv
```

## Documentation

- [Build configuration](doc/configuration.md)
- [Configuration reset](doc/configuration-reset.md)
- [Coding rules](docs/coding-rules.md)

## ESP32-C3 Super mini Pinout

| **Pin Name** | **GPIO** | **Function**                           |
|--------------|----------|----------------------------------------|
| 0            | GPIO0    | ADC1, PWM                             |
| 1            | GPIO1    | ADC1, PWM                             |
| 2            | GPIO2    | ADC1, Strapping Pin (Boot Mode)        |
| 3            | GPIO3    | ADC1, PWM                             |
| 4            | GPIO4    | JTAG, ADC1                            |
| 5            | GPIO5    | JTAG                                  |
| 8            | GPIO8    | Status LED (inverted), Strapping Pin  |
| 9            | GPIO9    | BOOT Button, Strapping Pin            |
| 10           | GPIO10   | PWM                                   |
| 20           | GPIO20   | General-purpose I/O                   |
| 21           | GPIO21   | General-purpose I/O                   |

## Important Links

- [ESP32-C3 Super mini github](https://github.com/sidharthmohannair/Tutorial-ESP32-C3-Super-Mini)
