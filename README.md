# alphabit
The alphabit is a three-channel looper effects pedal built using the Electro-Smith [Daisy Seed](https://www.electro-smith.com/daisy/daisy)(an ARM Cortex-M7 MCU). It has three looping modes, with controls for reverse playback, time stretching/compression, and a sample rate of 96kHz. It also has tone filter "ghost knobs" (only accessible when holding down the byp footswitch). 

---

### Controls & Modes
#### Controls:
- **Potentiometers -** 8 total  
Two per channel (time stretch/compression and volume control)  
Two (mix and volume) for the entire unit

- **Switches -** 4 total  
One per channel (forward/reversed playback)  
One for the entire unit (selecting one of the three looping modes)

- **Footswitches -** 4 total  
One per channel (loop recording, playback, and clearing)  
One for the entire unit (bypass, collective loop control, and additional features)

#### Modes:

- **1 -** One loop, recorded on channel A, is played back by all three channels, with each channel's controls affecting an independent copy of the loop.

- **2 -** The loops recorded on channels B and C are stretched or compressed to match the duration of channel A's loop.

- **3 -** All three loop channels play their respective loops, simultaneously, independent of each other.

### Using the Controls
#### Footswitches:
##### byp
<h6>Pressing the bypass footswitch (the leftmost) once toggles the state of the pedal, bypassed or enabled. When bypassed, the output will be the "dry" signal (the unaffected input). When enabled, the output will be a the recorded loops or the dry signal, if no loops have been recorded.

Double pressing this footswitch will clear any recorded loops.

Holding it down will trigger the "Ghost Knob" mode, signified by the byp LED displaying a cyan color. While in this mode, the user can set the frequency of the tone filter for each channel. By default, the filters are disabled, but they can be enabled and set by turning the corresponding Level knob. To disable the filter, turn the knob fully clockwise. Releasing the footswitch will leave the Ghost Knob mode. Any filter settings will be cleared when the loop is cleared.<h6>

##### Loop Channel footswitches
<h6>A loop can be recorded on the corresponding channel by holding down the footswitch for the desired duration. The maximum looptime is 30 seconds, but the loop will automatically overdub if this is reached.
Pressing the footswitch (after a loop has been recorded) will toggle the playback of the loop, enabling or disabling it.
Double clicking the footswitch will clear the loop.<h6>

  
#### Knobs:
##### Mix
<h6> The Mix knob sets the wet/dry mix between the incoming signal and the playback of the loops.<h6>
  
##### Vol
<h6> The Volume knob affects the final output volume. If the knob is set at "noon" there will be no volume change between the input and output. Turning the knob counter-clockwise away from noon will reduce the output volume and turning it clockwise will increase volume. <h6>
  
##### Time Knobs
<h6> The time knobs 



