# JamesDSP for Linux
###### (OpenSource Version)
This is experimental and obviously work in progress.
Currently everything (analog modelling, bass boost, bs2b, VDC/DDCs, limiter, compression, reverbation and the stereo widener), except the convolver is implemented, and the plugin might produce crackling output if the samplerate is set incorrectly.


__This is the repo of the gst-plugin. You might want to visit the [main repository](https://github.com/ThePBone/JDSP4Linux)__

## Workarounds
### Fix crackling/choppy sound
_Set the default samplerate to 48000Hz in pulseaudio's config:_

`sudo nano /etc/pulse/daemon.conf`

Replace this line:
`;  default-sample-rate = 44100`
with this one:
`default-sample-rate = 48000`
## Launch it
You can find more information in the [main repo](https://github.com/ThePBone/JDSP4Linux).
   	
	gst-launch-1.0 -v pulsesrc device=[INPUTSINK].monitor volume=1.0 \
	! audio/x-raw,channels=2,rate=44100,format=F32LE,endianness=1234 \
	! audioconvert \
	! jdspfx enable="true" analogmodelling-enable="true" analogmodelling-tubedrive="6000" \
	! audio/x-raw,channels=2,rate=44100,format=F32LE,endianness=1234 \
	! audioconvert ! pulsesink device=[OUTPUTSINK] &
This wrapper is based on [gst-plugin-viperfx](https://github.com/ThePBone/gst-plugin-viperfx)




