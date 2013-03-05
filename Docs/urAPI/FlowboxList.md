Global Flowboxes
================
The following flowboxes are defined globally and can be used directly or as prototypes for cloning them. They can be accessed in lua in the global name space as _G["FB"..name] where name can be any of the flowbox names below.

Sources
=======

Sources are flowboxes which do not have an input. They are typically driven by user input and provide relevant input data as output.

Accel
-------
### Description
Provides hardware specific 3-axis accelerometer data. This flowbox is self-timed and provides a stable rate. It cannot be instanced.

### Typical native rate
rate = 33.3Hz

### Outputs
- X (0): X-Axis acceleration (-1,1)
- Y (1): Y-Axis acceleration (-1,1)
- Z (2): Z-Axis acceleration (-1,1)

Cam
--------
### Description
Provides hardware specific camera-derived data. This flowbox is self-timed and provides a stable rate. It cannot be instanced. It is subject to other camera settings.

### Typical native rate
rate = 30.0Hz

### Outputs
- Bright (0): Overall Brightness (0,1)
- Red (1): Overall Red channel contribution (0,1)
- Green (2): Overall Green channel contribution (0,1)
- Blue (3): Overall Blue channel contribution (0,1)
- Edge (4): Overall Edge/gradient contribution (0,1)

Compass
---------
### Description
Provides hardware specific 3-axis magnetic field and compass data. This flowbox is self-timed and provides a stable rate. It cannot be instanced.

### Typical native rate
rate = not specificed

### Outputs
- X (0): X-Axis magnetic strength (-1,1)
- Y (1): Y-Axis magnetic strength (-1,1)
- Z (2): Z-Axis magnetic strength (-1,1)
- North (3): Compass geographic north (-1,1) -> (-180,180) degrees

Location
----------
### Description
Provides hardware specific GPS data. This flowbox is self-timed and provides a stable rate. It cannot be instanced.

### Typical native rate
rate = not specificed

### Outputs
- Lat (0): Geographical latitude (-1,1) -> (-180,180) degrees
- Long (1): Geographical longitude (-1,1) -> (-180,180) degrees

Mic
-----
### Description
Provides hardware microphone data. This flowbox is self-timed and provides a stable rate. It cannot be instanced.

### Typical native rate
rate = 48000Hz

### Output
- Out (0): Microphone data (-1,1)

NetIn
-----
### Description
Provides network data. This flowbox is self-timed but does not provide a stable rate. It must be instanced.

### Typical native rate
rate = variable, depending on incoming network data timing

### Output
- Out (0): Incoming Network data (-1,1)

Push
------
### Description
Provides data pushed by an urMus lua program. This flowbox is self-timed but does not provide a stable rate. It must be instanced. Calling its :Push(data) method will increase time and send data to its output.

### Typical native rate
rate = variable, depending on the pattern Push:Push() is called

### Output
- Out(0): Programmatic pushed data (-1,1)

RotRate
---------
### Description
Provides hardware specific 3-axis gyroscope data. This flowbox is self-timed and provides a stable rate. It cannot be instanced.

### Typical native rate
rate = 60Hz

### Outputs
- x (0): X-axis angular velocity (-1,1)
- y (1): Y-axis angular velocity (-1,1)
- z (2): Z-axis angular velocity (-1,1)

Touch
-------
### Description
Provides multi-touch contact coordinates for up to 11 contacts (less may be available for certain hardware). This flowbox is self-timed but does not provide a stable rate. It cannot be instanced.

### Typical native rate
rate = variable, depending on multi-touch event pattern

### Outputs
- x1 (0): x position of first multi-touch contact (-1,1)
- y1 (1): y position of first multi-touch contact (-1,1)
- x2 (2): x position of second multi-touch contact (-1,1)
- y2 (3): y position of second multi-touch contact (-1,1)
- x3 (4): x position of third multi-touch contact (-1,1)
- y3 (5): y position of third multi-touch contact (-1,1)
- x4 (6): x position of forth multi-touch contact (-1,1)
- y4 (7): y position of forth multi-touch contact (-1,1)
- x5 (8): x position of fifth multi-touch contact (-1,1)
- y5 (9): y position of fifth multi-touch contact (-1,1)
- x6 (10): x position of sixth multi-touch contact (-1,1)
- y6 (11): y position of sixth multi-touch contact (-1,1)
- x7 (12): x position of seventh multi-touch contact (-1,1)
- y7 (13): y position of seventh multi-touch contact (-1,1)
- x8 (14): x position of eighth multi-touch contact (-1,1)
- y8 (15): y position of eighth multi-touch contact (-1,1)
- x9 (16): x position of nineth multi-touch contact (-1,1)
- y9 (17): y position of nineht multi-touch contact (-1,1)
- x10 (18): x position of tenth multi-touch contact (-1,1)
- y10 (19): y position of tenth multi-touch contact (-1,1)
- x11 (20): x position of eleventh multi-touch contact (-1,1)
- y11 (21): y position of eleventh multi-touch contact (-1,1)

Manipulators
============

Manipulators are flowboxes which have both at least one input and one output. They are typically manipulate data or generate controllable data. Traditionally these are associated with unit generators or filters. A manipulator can have coupled input/output pairs. This means that a timed update at one side must require a timed update on the other side. A number of manipulators are straight from the [Synthesis Toolkit (STK)](https://ccrma.stanford.edu/software/stk/) by Perry Cook and Gary Scavone.

SinOsc
--------
### Description
Generates a sine wave. It is not self-timed and derives its time update from being pulled at the output Out. It provides not couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Amp (1): Amplitude (0,1)
- SRate (2): Rate (-1,0,1) -> (-4,0,4) with 0.25->1 being standard rate
- Time (3): Time(-1,1)

### Output
- Out (0): (-1,1)

Avg
-----
### Description
Computes a sliding average of the incoming data. It is not self-timed and has coupled input/outputs.

### Source of Rate
Any change to input In (0) or output Out (0)

### Couples
In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Len (1): (-1,1) -> (1,511), default = 256

Dist3
--------
### Description
Computes the distance of an input 3-vector against a stored 3-vector. While Train is 1 it will store the inputs in internal storage. If train is -1 it will compute the distance. It is not self-timed and has no couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- In1 (0): (-1,1)
- In2 (1): (-1,1)
- In3 (2): (-1,1)
- Train (3): (-1,1)

### Output
- Out (0): (-1,1)

Add
-----
### Description
Provides the sum of two inputs at the output. It is not self-timed and has no couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- In1 (0): (-1,1)
- In2 (1): (-1,1)

### Output
- Out (0): (-1,1)

Min
-----
### Description
Provides the minimum of two inputs at the output. It is not self-timed and has no couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- In1 (0): (-1,1)
- In2 (1): (-1,1)

### Output
- Out (0): (-1,1)

Max
-----
### Description
Provides the maximum of two inputs at the output. It is not self-timed and has no couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- In1 (0): (-1,1)
- In2 (1): (-1,1)

### Output
- Out (0): (-1,1)

MinS
------
### Description
Indicates which is the minimum of two inputs at the output. Outputs -1 if In1 is smaller, and 1 if In2 is smaller. It is not self-timed and has no couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- In1 (0): (-1,1)
- In2 (1): (-1,1)

### Output
- Out (0): -1 or 1
		
MaxS
------
### Description
Indicates which is the maximum of two inputs at the output. Outputs -1 if In1 is bigger, and 1 if In2 is bigger. It is not self-timed and has no couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- In1 (0): (-1,1)
- In2 (1): (-1,1)

### Output
- Out (0): -1 or 1
		
Nope
------
#Description
Does nothing. It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
Inv
-----
#Description
Numerically inverts the input (a -> -a). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)

V
---
#Description
Computes a piece-wise linear transfer function with a V shape with (-1->1, 0->0, 1->1). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (0,1)
	
FullV
-------
#Description
Computes a piece-wise linear transfer function with a V shape with (-1->1, 0->-1, 1->1). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
DV
----
#Description
Computes a piece-wise linear transfer function with an inverted V shape with (-1->0, 0->1, 1->0). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (0,1)
	
FullDV
--------
#Description
Computes a piece-wise linear transfer function with an inverted V shape with (-1->-1, 0->1, 1->-1). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
CJ
----
#Description
Computes a piece-wise linear transfer function with shifts the interval (-1,0,1) -> (0,1,-1), creating a center jump condition. This is equivalent to a phase shift of pi. It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
SQ
----
#Description
Computes a piece-wise linear transfer function with creates a square wave response. (-a,0,a) -> (-1,0,1). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
PGate
-------
#Description
Computes a piece-wise linear transfer function which sets all negative input to zero (-a,0,a) -> (0,0,a). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)

NGate
-------
#Description
Computes a piece-wise linear transfer function which sets all positive input to zero (-a,0,a) -> (-a,0,0). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
Pos
-----
#Description
Computes a linear transfer function creating an all-positive output (-1,0,1) -> (0,0.5,1). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
Neg
-----
#Description
Computes a linear transfer function creating an all-negative output (-1,0,1) -> (-1,-0.5,0). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
ZPuls
-------
#Description
Generates a unit pulse when the input is zero (0->1). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)

SLP
-----
#Description
Computes a simple two-point average which acts like a basic low-pass filter. It takes returns the average of the current and the previous sample.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)

PosSqr
--------
#Description
Computes a quadratic transfer function creating an all-positive output (-a,0,a) -> (a^2,0,a^2). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
Oct
-----
#Description
Returns an octave range starting at base frequency Freq.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Freq (1): Frequency (-1,0,1) -> log(.,55,24000)

### Output
- Out (0): (-1,1)

Range
-------
#Description
Computes a linear transfer function creating a linear output (-1,1) -> (bottom,top). It is coupled for input In and output Out.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Bottom (1): (-1,1)
- Top (2): (-1,1)

### Output
- Out (0): (-1,1)
	
Quant
-------
#Description
Quantizes the smooth incoming signals to semi-tone intervals.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Input
- In (0): (-1,1)

### Output
- Out (0): (-1,1)

Gain
------
#Description
Applies a gain factor.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Amp (1): (-1,1)

### Output
- Out (0): (-1,1)


Sample
--------
#Description
Allows to play back a select sample from a list of loaded sample files. This flowbox has an additional method called AddFile(filename) to add files to its sample pool.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Amp (0): (-1,1)
- Rate (1): Rate (-1,0,1) -> (-4,0,4) with 0.25->1 being standard rate
- Pos (2): (-1,1)
- Sample (3): (-1,1)
- Loop (4): (-1,1)

### Output
- Out (0): (-1,1)
	
Looper
--------
#Description
Allows the recording and play back a sample. While the input to Record is 1, it will record into an internal buffer. This will be played back looped when Play is 1.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- In (0): (-1,1)
- Amp (1): (-1,1)
- Rate (2): Rate (-1,0,1) -> (-4,0,4) with 0.25->1 being standard rate
- Record (3): (-1,1)
- Play (4): (-1,1)
- Pos (5): (-1,1)

### Output
- Out (0): (-1,1)

CMap
------
### Description
Generates a circle map wave. A circle map is a non-linear oscillator. If the non-linearity is -1 it will behave like a linear sine oscillator. It is not self-timed and derives its time update from being pulled at the output Out. It provides not couples.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- NonL (1): Generic (-1,1)
- Amp (2): Amplitude (0,1)
- SRate (3): Rate (-1,0,1) -> (-4,0,4) with 0.25->1 being standard rate
- Time (4): Time(-1,1)

### Output
- Out (0): (-1,1)


Plucked
---------
#Description
This is the STK Plucked physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Trigger (0): (-1,1) -> nonzero value for amplitude
- Freq (1): Frequency (-1,0,1) -> log(.,55,24000)

### Output
- Out (0): (-1,1)

ADSR
------
#Description
This is the STK ADSR envelope algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Trigger (0): (-1,1) -> positive value for ATTACK, nonpositive for RELEASE
- Attack (1): (-1,1) -> attack rate
- Decay (2): (-1,1) -> decay rate
- Sustain (3): (-1,1) -> sustain level
- Release (4): (-1,1) -> release rate

### Output
- Out (0): (-1,1)

Asymp
-------
#Description
This is the STK Asymp asymptotic smoothing algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Tau (1): Rate (-1,1) -> (0,1) 

### Output
- Out (0): (-1,1)
	
BiQuad
--------
#Description
This is the STK BiQuad resonant filter algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Reson (1): Frequency (-1,0,1) -> log(.,55,24000)
- Q (2): (-1,1)
- Notch (3): Frequency (-1,0,1) -> log(.,55,24000)
- NQ (3): (-1,1)

### Output
- Out (0): (-1,1)

Blit
------
#Description
This is the STK Blit band-limted impulse train unit generator algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Phase (1): (-1,1)
- Harms (2): (-1,1)

### Output
- Out (0): (-1,1)
	
BlitSaw
---------
#Description
This is the STK BlitSaw band-limted sawtooth unit generator algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Harms (1): (-1,1)

### Output
- Out (0): (-1,1)
	
BlitSq
--------
#Description
This is the STK BlitSq band-limted square wave unit generator algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Phase (1): (-1,1)
- Harms (2): (-1,1)

### Output
- Out (0): (-1,1)

BlowBotl
----------
#Description
This is the STK BlowBotl physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Amp (1): Amplitude (0,1)
- Rate (2) : Rate (-1,1) -> (sample, 4sec)

### Output
- Out (0): (-1,1)
	
BlowHol
---------
#Description
This is the STK BlowHol physical excitation algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- ToneHole (1) : Tonehole state (-1,1) -> (closed, fully open)
- Vent (2) : Vent state (-1,1) -> (closed, fully open)
- Amp (3): Amplitude (0,1)
- Rate (4) : Rate (-1,1) -> (sample, 4sec)

### Output
- Out (0): (-1,1)
	
Bowed
-------
#Description
This is the STK Bowed physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Vibrato (1): Vibrato Gain (-1,1) 
- Amp (2): Amplitude (0,1) 

### Output
- Out (0): (-1,1)

BowTbl
--------
#Description
This is the STK BowTbl physical excitation algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Offset (1): (-1,1)
- Slope (2): (-1,1)

### Output
- Out (0): (-1,1)
	
Brass
-------
#Description
This is the STK Brass physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Amp (1): Amplitude (0,1) 

### Output
- Out (0): (-1,1)

Clarinet
----------
#Description
This is the STK Brass physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Amp (1): Amplitude (0,1) 
- Rate (2) : Rate (-1,1) -> (sample, 4sec)

### Output
- Out (0): (-1,1)

Delay
-------
#Description
This is the STK Delay non-interpolating delay-line algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Delay (1): (-1,1) -> (0, 1sec)

### Output
- Out (0): (-1,1)
	
DelayA
--------
#Description
This is the STK Delay allpass interpolating delay-line algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Delay (1): (-1,1) -> (0, 1sec)

### Output
- Out (0): (-1,1)

DelayL
--------
#Description
This is the STK Delay linear interpolating delay-line algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Delay (1): (-1,1)

### Output
- Out (0): (-1,1)

Echo
------
#Description
This is the STK Echo algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Echo (1): (-1,1)
- Mix (2): (-1,1)

### Output
- Out (0): (-1,1)

Env
-----
#Description
This is the STK Env envelope algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- Target (0): (-1,1)
- Rate (1) : Rate (-1,1) -> (sample, 4sec)

### Output
- Out (0): (-1,1)

Flute
-------
#Description
This is the STK Brass physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- JetRefl (1): (-1,1)
- EndRefl (2): (-1,1)
- JetDelay (3): (-1,1)
- Amp (4): Amplitude (0,1)
- Rate (5) : Rate (-1,1) -> (sample, 4sec)


### Output
- Out (0): (-1,1)
	
JCRev
-------
#Description
This is the STK JCRev John Chowning reverb algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- T60 (1): (-1,1)

### Output
- Out (0): (-1,1)

JetTbl
--------
#Description
This is the STK JetTbl physical excitation algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)

### Output
- Out (0): (-1,1)

Mod
-----
#Description
This is the STK Mod modulation algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- VibFreq (0): Vibrato Frequency (-1,0,1) -> log(.,55,24000)
- VibGain (1): Vibrato Gain(0,1)
- RandGain (2): Random Gain(0,1)

### Output
- Out (0): (-1,1)

OnePole
---------
#Description
This is the STK OnePole filter algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Reson (1): Frequency (-1,0,1) -> log(.,55,24000)

### Output
- Out (0): (-1,1)
	
OneZero
---------
#Description
This is the STK OneZero filter algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Notch (1): Frequency (-1,0,1) -> log(.,55,24000)

### Output
- Out (0): (-1,1)

PitShift
----------
#Description
This is the STK PitShift pitch shifting algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Shift (1): (-1,0,1) -> (0,1,4)

### Output
- Out (0): (-1,1)

AllPass
---------
#Description
This is the STK AllPass filter algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Coeff (1): (-1,1)

### Output
- Out (0): (-1,1)
	
ZeroBlk
---------
#Description
This is the STK ZeroBlk zero blocking filter algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)

### Output
- Out (0): (-1,1)
	
PRCRev
--------
#Description
This is the STK PRCRev Perry Cook reverb algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- T60 (1): (-1,1)

### Output
- Out (0): (-1,1)
	
ReedTbl
---------
#Description
This is the STK ReedTbl physical excitation algorithm.

### Source of Rate
Any change at input In (0) or output Out (0)

### Couples
- In (0) <-> Out (0)

### Inputs
- In (0): (-1,1)
- Offset (1): (-1,1)
- Slope (2): (-1,1)

### Output
- Out (0): (-1,1)
	
Saxofony
----------
#Description
This is the STK Saxofony physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Freq (0): Frequency (-1,0,1) -> log(.,55,24000)
- Pos (1): blowing position (-1,1) -> (0,1)
- Amp (2): Amplitude (0,1)
- Rate (3) : Rate (-1,1) -> (sample, 4sec)
 

### Output
- Out (0): (-1,1)

Sitar
-------
#Description
This is the STK Sitar physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Trigger (0): (-1,1) -> nonzero value for amplitude
- Freq (1): Frequency (-1,0,1) -> log(.,55,24000)

### Output
- Out (0): (-1,1)

StifKarp
----------
#Description
This is the STK StifKarp physical modeling algorithm.

### Source of Rate
Pulling output Out (0)

### Couples
None

### Inputs
- Trigger (0): (-1,1) -> nonzero value for amplitude
- Freq (1): Frequency (-1,0,1) -> log(.,55,24000)
- Stretch (2): (-1,1)
- Pos (3): (-1,1)
- Loop (4): (-1,1)

### Output
- Out (0): (-1,1)


Sinks
=====

Sinks are flowboxes which do not have an output. They are typically driving device actuation, output to the user, or outgoing network traffic derived from the data at its input.

Dac
-----
### Description
Provides hardware speaker playback. This flowbox is self-timed and provides a stable rate. It cannot be instanced.

### Typical native rate
rate = 48000Hz

### Input
- In (0): Audio data (-1,1)

Net
-----
### Description
Provides network data. This flowbox is self-timed but does not provide a stable rate. It must be instanced.

### Typical native rate
rate = variable, depending on incoming network data timing

### Input
- In (0): Outgoing Network data (-1,1)
	
Vis
-----
### Description
Provides pulling data at visual rates. This flowbox is self-timed but does not provide a stable rate, however the target rate is around 60Hz. It cannot be instanced. This flowbox offers a special method called Get() which allows access to the last data point acquired. Get is guaranteed to have been updated before the OnUpdate event fires.

### Typical native rate
rate = variable, depending on computational and graphics load, target is 60Hz

### Input
- In (0): Pulled visual data (-1,1)

Pull
-----
### Description
Provides data pulled by an urMus lua program. This flowbox is self-timed but does not provide a stable rate. It must be instanced. Calling its :Pull(data) method will increase time and send data to its output.

### Typical native rate
rate = variable, depending on the pattern Pull:Pull() is called

### Input
- In (0): Programmatic pushed data (-1,1)
	
