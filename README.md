# JamesDSP for Linux
###### (OpenSource Version)
This is experimental and obviously work in progress.
Currently everything (analog modelling, bass boost, bs2b, VDC/DDCs, limiter, compression, reverbation and the stereo widener), except the convolver is implemented.


__This is the repo of the gst-plugin. You might want to visit the [main repository](https://github.com/ThePBone/JDSP4Linux)__

## AUR package

yochananmarqos made an [AUR package](https://aur.archlinux.org/packages/gst-plugin-jamesdsp-git/) for this repo:
```bash
yay -S gst-plugin-jamesdsp-git
```
Note: This package alone does not install JDSP for you. Installation instructions can be found in the main repository!

![AUR version](https://img.shields.io/aur/version/gst-plugin-jamesdsp-git?label=aur)

## Launch it
You can find more information in the [main repo](https://github.com/ThePBone/JDSP4Linux).
   
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




