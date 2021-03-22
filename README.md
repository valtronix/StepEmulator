# StepEmulator
Create a device that will move a smartphone to emulate walking or running activity.

## How it works?
The device will hold your smartphone, then you set up how many steps and starts. The StepEmulator will then shake your smartphone to simulate this amount of steps and stop automatically at the end.

When it switched on, buzzer and display is tested then the preset number of step is displayed.

### Set up the number of steps
To change the value, just click on the button, a cursor will blink on one digit. You can increase/decrease this digit by turning the button clockwise or counterclockwise respectively. The carry will follow. If you want to fine change the number of steps, just click again on the button. The cursor will then move to the next digit to the right. If is no more digit at the right, the cursor will go back to the thousands position.

If you do not touch to the button for some seconds, the cursor will disapear.

### Emulate walking
To start the step emulator, do a long press on the button. It will start when button will be released.

To pause the step emulator, do a short click on the button.

When there is no more step remaining, "00 000" will blink on the display and the buzzer will beep. This will stop by pressing the button and step counter will be reinitialized.

## Access to internal configuration
You can modify a lot of internal settings by keeping button pressed for more than 1 second at power up. When the display will change to "88 888" to "[= ===]", you can release the button.

The cursor will blink on the first group of 2 digits. The cursor will switch between the 2 groups of digits each time you press the button.
 - The group of 2 digits at the left is the address.
 - The group of 3 digits at the right is the value for this address.

It you turn the button, it will increase or decrease the group of digits on which is the cursor. Changing the address will store the value and display the value corresponding to the new address.

Address | Meaning                                | Default value
-------:|----------------------------------------|---------------:
 00     | Position of the servo for step down    |    20
 01     | Position of the servo for step up      |   180
 02     | Number of steps by default (low byte)  |     3
 03     | Number of steps by default (high byte) |
 04     | Speed for walking (low byte)           | 
 05     | Speed for walking (high byte)          | 
 06     | Speed for running (low byte)           | 
 07     | Speed for running (high byte)          | 
 08     | Step ratio for walking                 |   50
 09     | Step ratio for running                 |   50
 
 **Warning:** Changing this settings can cause major failure.
 
