# QA
Let's add some unit tests for the basic functionality. In particular, before implementing the following many features. In addition to unit tests, you can create a Markdown with GUI tests which I will carry out for you after you make the following changes (let's do the tests one feature at a time to make sure we're not diverging too much at once from the current version, which works well.)

# Features

## Load input from file

## Different colors for each track
That way we can color code the controller with corresponding play buttons.

## Finalize Viz GUI (move WhAM to middle)
For now, have a narrow rounded rectangle in the middle that says "WhAM", and remove the current WhAM logo from the top. I'll ask Meghan for a nicer long-rectangular Asset.

## Change "Save/load config" to just "Save/load"
Have it also save current input and output tracks, and as much other configuration as is easy to save.

## Add high/low pass filters
Perhaps instead of "Overdub"? These knobs should go in the Tape Control knob section.

## Give the .exe the icon from Assets/wham.png

## Move "autogen" checkbox
A logical place right now is to narrow the Generate button a bit and use the space (on the same horizontal line) to add a checkbox with a "loop" icon that determines whether the Generate button acts as a momentary or a toggle (toggle being the same behavior as "autogen" currently).

## Header redesign
### Top left
Right now it has a button which shows Git info. Instead, put the WhAM icon (roughly the same size as it currently is in Viz). The icon can be clicked on, and it says "Whale Acoustics Model tape looper interface. Early alpha, CETI internal. (Then some boilerplate for strict licensing, namely, this software cannot be used by anyone except for CETI or the WhAM team.)" Then below that you can put the Git info.

### Top middle
Right now it says "tape looper - wham" this can be removed

### Top right
Right now it has info about the input and output. This info will be moved into Settings. See next item.

## Settings small redesign
Have the settings have three tabs: (1) Gradio, (2) MIDI Binds, (3) I/O. Tabs should be implemented as currently done in the "Synths" menu (there there are two tabs: Sampler and Beep.)

(1) Most importantly, as currently, allows to set the Gradio URL. But use the new real-estate to list whatever info you can find from Gradio. In particular, args.conf is a useful parameter because it tells me what checkpoint the model is running. But no worries if it's too much hassle to figure out what Gradio exposes.

(2) The MIDI Binds tab should as currently list all the current binds, but have a button to unbind each. And have a button to Unbind All.

(3) I/O just lists the info which if currently in the top right.

## Rename "MIDI Learn" to "Midi Bind"
IT's just a better name. Do this everywhere. Rename "Clear MIDI Mapping" to "Unbind"

# Bugs
## Mic icon
Right now it is "m..." which is weird. Can we just use an actual microphone icon instead? We can move it next to the Level slider, which is going to be cleared up.

## "record" should be "input" above the "output"
This is referring to each track, at the top right above the tape control knobs.

## Remove "These knobs feed VampNet's advanced controls" text in Model Params
It's redundant.

# Future features (for reference)
## WhAM logo animation on generate
Once we get the asset from Meghan, make it so that once the model is generating, the logo is animated (what I currently have in mind is to have the rubics cube subfaces light up in random ways, then land on a random final state. Concretely, we might just have say 16 PNGs we cycle through randomly.)

## Sync and play all
Right now flow is to click on "Sync all" and then hit play on all tracks simultaneously. This can replace the existing "Sync all" button, which I've never used. So this new button, when held, repeatedly syncs all tracks and plays them (wait until last track is done, then repeat...).

## Interpolate a common tempo / length so that all tracks loop together
Essentially, if there are multiple tracks of different lengths but we want to loop them together, scale all tracks to match the length of the first one.

## What's with the "in" and "out" dropdowns?
I'm not sure what these are supposed to do. Right now there's only one option in each, and that's "all". Instead, we can have a single drop down "in". This has the option of (some name for the normal behavior, i.e. synths, audio in=mic), or one for each other track input and output. So for example, when working with three tracks, Track 1 will have the default (idk what to call it, maybe "Rec"?), then "Track 2 in", "Track 2 out", "Track 3 in" and "Track 3 out". This allows us to merge tracks together on the fly. If this requires significant architectural changes then we can defer this feature and instead, for now, just remove the "in" and "out" dropdowns which seem useless.

## Save track output button
Per track. Allows you to save the track output to file.

# Demo flow (for reference)
For the five-minute demo at ECDD25.