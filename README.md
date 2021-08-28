# JamesDSP for Linux

### This repository is outdated. Please check https://github.com/Audio4Linux/JDSP4Linux for updates. The GStreamer plugin for libjamesdsp is now backed into the GUI.

###### (OpenSource Version)

>Maintained by [@ThePBone](https://t.me/ThePBone)

### Technical Data
Supported sample formats:
* 32-bit float (LE)
* 32-bit int (LE)

Supported samplerates:
* 44100
* 48000
### Effects
Pretty much everything from the opensource version is implemented:
* Analog modelling (12AX7)
* BS2B
* ViPER DDCs
* Limiter
* Compression
* Convolver
* Reverbation (Progenitor2)
* Bass boost
* Stereo widener (Mid/Side) 

Instead of being tied to presets; stereo widener, bs2b and the reverbation engine can be fully customized.



__This is the repo of the gst-plugin. You might want to visit the [main repository](https://github.com/Audio4Linux/JDSP4Linux)__

## AUR package

yochananmarqos made an [AUR package](https://aur.archlinux.org/packages/gst-plugin-jamesdsp-git/) for this repo:
```bash
yay -S gst-plugin-jamesdsp-git
```
Note: This package alone does not install JDSP for you. Installation instructions can be found in the main repository!

![AUR version](https://img.shields.io/aur/version/gst-plugin-jamesdsp-git?label=aur)

## Launch it
You can find more information in the [main repo](https://github.com/Audio4Linux/JDSP4Linux).
   
   	gst-launch-1.0 -v pulsesrc device=[INPUTSINK].monitor volume=1.0 \
	! jdspfx enable="true" analogmodelling-enable="true" analogmodelling-tubedrive="6000" \
	! pulsesink device=[OUTPUTSINK] &	
	
Use this pipeline if the input sink is using an unsupported format:

	gst-launch-1.0 -v pulsesrc device=[INPUTSINK].monitor volume=1.0 \
	! audio/x-raw,channels=2,rate=44100,format=F32LE,endianness=1234 \
	! audioconvert \
	! jdspfx enable="true" analogmodelling-enable="true" analogmodelling-tubedrive="6000" \
	! pulsesink device=[OUTPUTSINK] &
This wrapper is based on [gst-plugin-viperfx](https://github.com/ThePBone/gst-plugin-viperfx)




