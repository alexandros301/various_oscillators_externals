Various oscillators library for Pd, written by Alexandros Drymonitis.

This library contains various oscillator external Pd objects. It currently includes [powSine~], [randOsc~], [varShapesOsc~], [allOsc~], and [sineLoop~]. 
Hopefully it will grow.

[powSine~] is a sinusoid oscillator raised to a power in order to modulate the width of its positive and negative pulses. See 
powSine~-help.pd for more info

[randOsc~] is a random oscillator, not white noise. It gets a random value for each period and makes a ramp from the previous 
random value to the current random value. You can also raise it to a power to modulate the ramp, to make it either linear, 
exponential or logarithmic. See randOsc~-help.pd for more info.

[varShapesOsc~] is an oscillator that can smoothly change between all four standard oscillator waveforms (sine, triangle, sawtooth 
and square), as well as create shapes that stand in between. See varShapesOsc~-help.pd for more info.

[allOsc~] is an oscillator outputting all four standard waveforms.

[sineLoop~] is a sine wave feedback oscillator, translated from Pyo's corresponding oscillator source code.

All oscillators have signal inlets (except for the very last one, which is a control inlet to reset the oscillator's phase), in 
order to be able to modulate their parameters with other oscillators. They also have an inlet for phase modulation (except for 
[randOsc~], it becomes too glitchy, and [sineLoop~], maybe I'll add it in the future), to achieve what you can achieve with this:

[phasor~] [osc~]
|         |    __[0\
|    _____[*~ ]
[+~ ]
|
[cos~]


Setting arguments is disabled cause I found it too difficult to be able to use a value set via an argument until you connect a 
signal to the respective inlet, where the signal will override the argument. You are more than welcome to hack the source code to 
set this feature. If you do so, please let me know, it would be nice to have something like this :)
In the help patches there's a proposed work around for setting arguments.

For any questions or anything that has to do with these objects, drop me a line at alexdrymonitis[at]gmail[dot]com

August 2015
