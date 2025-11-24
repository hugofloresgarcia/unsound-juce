# text2sound4all

![text2sound4all](/assets/text2sound4all.png)

text2sound4all is a multitrack looper that lets you generate sounds from text prompts using stable audio open small. it is an open source version of [unsound objects](https://hugofloresgarcia.art/interfaces.md#unsound-objects-2025), an instrument/spatial improvisation instrument/piece where a performer co-creates acoustic environments with a generative neural network.  

i presented text2sound4all (and the accompanying )

## download

you can download the latest release (v0.0.1) from the releases page: [download v0.0.1](https://github.com/hugofloresgarcia/unsound-juce/releases/tag/v0.0.1)

**note**: currently, we only have pre-built binaries for macOS (apple silicon). if you're on intel mac, windows, or linux, you'll need to build from source. check out the [main readme](../../README.md) for build instructions.

## setting up the backend

text2sound4all relies on a huggingface space to generate audio. by default, it connects to the public `hugggof/saos` space.

however, the public space runs on a cpu and is **very slow**. for a reasonable speed, you'll want to duplicate the space and run it on a gpu.

1. go to the [saos huggingface space](https://huggingface.co/spaces/hugggof/saos).
2. click the "duplicate this space" button in the top right corner.
3. select a gpu hardware (e.g. T4 small or A10G).
4. once your space is running, copy the url (e.g. `https://huggingface.co/spaces/{your-username}/saos`).
5. paste this url into the configuration dialog when you launch text2sound4all.

## bugs?

if you find any bugs or issues, please [open an issue](https://github.com/hugofloresgarcia/unsound-juce/issues) on the github repo!
