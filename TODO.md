layercake: 

- implement drag and drop for audio files into individual layercake layer. dragging an audio file on top of a layer should load the audio file into the layer's buffer, trimmed up to the max duration of the layer tape buffer. 


in a bottom row below the main window and knobs, place four LFO widgets side by side. You'll also need to make an LFO UGen. 
the widget should have
- small knobs control for speed and depth
- modes: sin, tri, square, random, smooth random
- a small window visualizing the LFO wave
- I should be able to drag the LFO wave into any knob(s), and it should tie the knob to the LFO. add a separate, complementary colored sweep indicator showing the value affected by the LFO
- right clicking on the knob should show an option to remove the LFO from the knob. 

add a knob sweep recorder/looper -- right clicking on a knob should open an option to record a sweep... if user clicks on that option, indicate a blinking state on the knob that it's ready to record -- record the gesture from when mouse clicks to when mouse releases, then immediately loop it back until user clears it by touching the knob again. for situations where we are interacting via midi, add a separate momentary button for emulating the mouse click for defining the bounds upon which we should record... label the button [kr] (knob record)