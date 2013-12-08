# Pa55ware. A simple, DIY hardware password manager.

Pa55ware ia a hardware password manager that can be built with a small amount of
parts and be kept with you.

**Important** Please keep in mind that this is still a work in progress. Several
things are missing in order to provide a basic security level. Please have a
look at the bug tracker or in the wiki for more information.

## Features

### Usage
* LCD screen to select your accounts
* 4 touch buttons to select entries
* The device acts as a USB keyboard and can type the password on the computer.
* Can handle 2-factor authentication (Only TOTP for the moment).

### Security
* Every information is encrypted using AES
* The AES key stored on the device is cleared after configured amount of false
  PIN entries

You can find more information and documentation on the project's wiki here :
[https://github.com/Baldanos/pa55ware/wiki](https://github.com/Baldanos/pa55ware/wiki)
