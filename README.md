# Ducky Defense
This is a program meant as a light-weight defense against [USB rubber duckies](https://shop.hak5.org/products/usb-rubber-ducky-deluxe) and similar devices. It hijacks all incoming usb keyboards and makes sure that a human is at the computer by forcing you to type a number every time you plug in a new keyboard (akin to how a bluetooth pairing code is sometimes implemented).

This program is meant to start as root at user login, and opens a window when you plug in a keyboard. The only keys that are allowed (the rest are silently ignored) are 0 through 9 and backspace. If you type the code incorrectly, your keyboard will go to "jail" and you will not be able to type. To fix this, unplug the keyboard from the host machine and plug it back in.

## Dependencies
- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear/)
- X11
# Notes
- make sure the `uinput` kernel module is loaded (```sudo modprobe uinput```)
