# JamesDSP for Linux
This is very experimental and obviously work in progress.
Currently only analog modelling, bass boost, reverbation and the stereo widener is implemented, and the plugin might produce crackling output if the samplerate is set incorrectly.

## Workarounds
### Fix crackling/choppy sound
_Set the default samplerate to 48000Hz in pulseaudio's config:_

`sudo nano /etc/pulse/daemon.conf`

Replace this line:
`;  default-sample-rate = 44100`
with this one:
`default-sample-rate = 48000`
## Launch it
I will provide more details and update this readme later...
	
	gst-launch-1.0 -v pulsesrc device=[INPUTSINK].monitor volume=1.0 \
	! audio/x-raw,channels=2,rate=44100,format=F32LE,endianness=1234 \
	! audioconvert \
	! jdspfx enable="true" analogmodelling-enable="true" analogmodelling-tubedrive="6000" \
	! audio/x-raw,channels=2,rate=44100,format=F32LE,endianness=1234 \
	! audioconvert ! pulsesink device=[OUTPUTSINK] > $logfile &
This wrapper is based on [gst-plugin-viperfx](https://github.com/ThePBone/gst-plugin-viperfx)




