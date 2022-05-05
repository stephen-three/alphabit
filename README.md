# alphabit
The alphabit is a three-channel looper effects pedal built using the Electro-Smith [Daisy Seed](https://www.electro-smith.com/daisy/daisy)(an ARM Cortex-M7 MCU). It has three looping modes, with controls for reverse playback, time stretching/compression, and a sample rate of 96kHz. It also has tone filter "ghost knobs" (only accessible when holding down the byp footswitch). 

---

---

## Controls & Modes
### Controls:
- **Potentiometers -** 8 total  
Two per channel (time stretch/compression and volume control)  
Two (mix and volume) for the entire unit

- **Switches -** 4 total  
One per channel (forward/reversed playback)  
One for the entire unit (selecting one of the three looping modes)

- **Footswitches -** 4 total  
One per channel (loop recording, playback, and clearing)  
One for the entire unit (bypass, collective loop control, and additional features)

### Modes:

- **1 -** One loop, recorded on channel A, is played back by all three channels, with each channel's controls affecting an independent copy of the loop.

- **2 -** The loops recorded on channels B and C are stretched or compressed to match the duration of channel A's loop.

- **3 -** All three loop channels play their respective loops, simultaneously, independent of each other.

---

## Using the Controls
### Footswitches:
#### byp
Pressing the bypass footswitch (the leftmost) once toggles the state of the pedal, bypassed or enabled. When bypassed, the output will be the "dry" signal (the unaffected input) and the byp LED will be unlit. When enabled, byp LED will be lit White and the output will be either the recorded loops or the dry signal, if no loops have been recorded.

Double pressing this footswitch will clear any recorded loops.

Holding it down will trigger the "Ghost Knob" mode, signified by the byp LED displaying Cyan. While in this mode, the user can set the frequency of the tone filter for each channel. By default, the filters are disabled, but they can be enabled and set by turning the corresponding Level knob. To disable the filter, turn the knob fully clockwise. Releasing the footswitch will leave the Ghost Knob mode. Any filter settings will be cleared when the loop is cleared.

#### Loop Channel footswitches
A loop can be recorded on the corresponding channel by holding down the footswitch for the desired duration. The maximum looptime is 30 seconds, but the loop will automatically overdub if this is reached. When recording, both LEDs will display red.
Pressing the footswitch (after a loop has been recorded) will toggle the playback of the loop, enabling or disabling it. When a loop restarts at its beginning, the loop LED (near the Mix and Volume Knobs) will light up. Each channel has its own corresponding color- A: Magenta, B: Cyan: C: Yellow.
Double clicking the footswitch will clear the loop, indicated by the loop LED blinking green three times.

### Knobs:
#### Mix
The Mix knob sets the wet/dry mix between the incoming signal and the playback of the loops.
  
#### Vol
The Volume knob affects the final output volume. If the knob is set at "noon" there will be no volume change between the input and output. Turning the knob counter-clockwise away from noon will reduce the output volume and turning it clockwise will increase volume. 
  
#### Time Knobs
The Time knobs control the speed of the loop's playback, which can range from a quarter slower to four times faster. With the knob at noon, the loop will play at its original speed. Turning the knob counter-clockwise will reduce the speed and turning it clockwise will increase the speed. The pitch of the loop is proportionally affected by the speed because the loop is being read back at a different rate, much like slowed cassette tapes. In mode 2, since loops B and C are stretched and compressed to match loop A's duration, only channel A's Time knob will have an effect.
  
#### Level Knobs
The Level knobs control the relative volume of the loops. These can be used to mix the output and highlight one loop over another. These knobs are also used to set the tone filters for the corresponding channels when the byp footswitch is held down.

### Switches:
#### Mode
The mode switch determines the looping mode- 1, 2, or 3.

#### Reverse switches
The Reverse switches determine the direction of the playback for the corresponding channel. When flipped, the loop will play in reverse.
  
### DIP Switch:
#### Footswitch Behavior
A DIP switch located inside the enclosure on the pedal's PCB. The position of this switch (and then powering on the pedal again) determines the behavior of the footswitches, notably hold-to-record vs press-to-toggle-rec. In this mode, a single press will begin the recording the following press will end the recording. A double press will toggle the playback of the loop and holding the footswitch for a short time (0.6 sec) will clear the loop. In either mode a double press on the byp footswitch will pause the playback of all the loops.
