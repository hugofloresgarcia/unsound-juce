- remove customParams from the Text2Sound4All app.
- factor all of the low pass filter logic out of LooperTrackEngine. 
- remove all panner tests. 
- a UGen is an audio processing class with a prepare and process_sample or process_block methods. All of our classes that interact with the audio loop should have the UGen format. update all classes that don't do so to follow that structure. 
    - an example of this is LooperReadHead. LooperReadHead should have a signature called process_sample method where we read the sample and advance the playhead simultaneously, since we always want to read a sample and advance. 
- remove the channel meter logic from MultiTrackLooperEngine, move it to a "MultiChannelLoudnessMeter" UGen class. 
- remove the peak meter logic from LooperTrackEngine, move it to a PeakMeter UGen class. you should be able to query the peak with get_peak() for the last block you processed.
- make the scripts/build_and_release_all_todo.sh script actually build and release all apps. 
- rename the Engine/ folder to LooperEngine/
- in LooperTrackEngine, don't let outside classes acces the read and write heads, instead, expose the methods you have to expose from LooperTrackEngine (set_speed, set_loop_end, etc. etc.)
- all methods, function names and local variables in the flowerjuce lib should be written in snake_case. please use find and replace from the CLI to correct all variable names and method functions. 

please build and compile in between each of these buillet points, to ensure good progress. 



remember the rules: 

- prefer composition over inheritance
- when writing complex math or logic, prefer to write all the main logic encased in stateless functions, and have wrapper classes that rely on stateless function (think torch.nn.functional and the nn Module for torch). 
- place DBG statements to thoroughly document the flow of code, except in Audio threads where timing is critical. In audio threads, DBG every 2s or so. 
- use PascalCase for class names, snake_case for local variables, functions and class methods, m_snake_case for class members.

- add debug statements everytime you write an early return path. 

- **UI Layout Rule**: In `resized()` and similar UI layout functions, declare all sizes, margins, and spacing values as meaningful `const` variables at the top of the function, organized into logical groups (margins, label sizes, spacing, button sizes, etc.). Then use these named constants throughout the layout code instead of magic numbers. This makes the code maintainable and easy to adjust.

** NEW APP: layercake
- create a new app called layercake. note that this app is NOT a looper.
    - we'll create a LayerCakeEngine, which comprises:
        - n_layers: number of buffer layers (default to 6), use std::array
        - each layer corresponds to a TapeLoop, a buffer that we will fill. make them max 10 seconds for now, with an option to grow at compile time. 
        - n_voices: number of playback voices (default to 16)
        - each voice corresponds to a GrainVoice class (UGen), which contains a LooperReadHead along with an ASR enveloope (UGen) class and a Stereo/2DPanner (UGen) class. to handle varying layers, we should be able to quickly swap the TapeLoop& that the LooperReadHead points to. 
        - all GrainVoices are stored in a std::array
        - LayerCakeEngine should have a function called trigger_grain, which should take a GrainState struct as input =>
            float loop_start, float duration_ms, 
            float rate_semitones, 
            float env_atk, float env_release
            bool fwd_or_bw (playback direction),
            int layer. trigger_grain  will schedule a new grain to be triggered for the process_block.
        - LayerCakeEngine should have a LooperWriteHead for recording from an input channel.
        - in LayerCakeEngine, we should have a set_record_layer(int layeridx) that lets you choose which target layer to record into.
        - in LayerCakeEngine, we'll also need a GrainClock that triggers grains. Make a PatternClock UGen 
            - PatternClock will rely on a metronome to either trigger or not trigger a grain at every clock cycle. 
                - It should have a pattern length of up to 128 steps. 
                - we should be able to adjust the length of the pattern, to, for example 16 steps. 
                - should have an rskip control,which determines the probablility of skipping a grain on a given step. 
                - when the PatternClock is activated, we record the grains triggered during the first pattern_length steps. Once we've recorded the ncessary steps, we play them back in a loop at every clock cycle. The data recorded in every pattern step should be 
                - when the PatternClock is in record mode, every pattern step should record the GrainState of the grain that was triggered during that step, in order to recreate the original sequence in the 
            - PatternClock should rely on a  Metro UGen. 
                - Metro UGen should have a set_period_ms to configure the rate of and its process_sample function should return true everytime the period resets. to count the passage of time, use a sample counter in process_sample. we should have a reset() control. It should also have functional utility helpers to convert bpm to period in ms.  
    

    - we'll need a brand new UI display for this. the man UI component of
        - in terms of color palette and visual design, think NES-metroid color palette, iconography and styling
        - the main window should be a 500-by-500 retro style TV window. 
        - it should display all 6 track waveforms on top of each other, with a red [r] indicator in the corner of the track that is currently reocrd enable.
        - the background of the main screen should be pitch black, with the waveforms drawn in a soft warm white.
        - add other theming and decorating animations, like grass or little space invaders randomly roaming. 
        - I want a dynamic animation of the grains being played, overlaid on top of the buffer:
            - when a new grain is triggered:
                - choose a color from the color palette to represent the grain onscreen:
                - make a semi-tranparent "highlight" over the are of the waveform indicating the loop bounds: start to end
                - make a "playhead" indicating the current position of the read head as the grain plays. 
                    - animate the "playhead" as a sine wave squiggle in a fully opaque version of the color we chose
                        - choose a default frequency of this squiggle, that looks good.
                        - map the playback rate of the grain to the frequency of the squiggles sine wave. if the squiggle is playing at -12 semitones, the squiggle should be at 0.5 it's base frequency.

        - main window will also need a master level knob, and a master level meter. 
    
    - on a separate window that opens with the other one, we want to create a preset bank for storing and recalling patterns for the patternclock. make it so that we can optionally also restore the layer buffers themselves. store layers, patterns,and engine presets in ~/Documents/layercake. 
        - We'll want to keep track of the state of the PatternClock and the LayerCakeEngine AND the layer buffers themselves (separately) for storing and recalling patterns from the patternclock and the state of the controls for the layercakeengine. 
         
