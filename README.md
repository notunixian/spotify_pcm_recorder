# spotify_pcm_recorder
a small project i wrote in about an hour to capture raw PCM data from spotify, allows you to capture spotify lossless (although this is very ineffecient for any true downloading/ripping purposes)

## usage
- copy dbghelp.dll, symsrv.dll and symsrv.yes to the folder where Spotify.exe is from Windows Debugging Kit (look at C:\Windows Kits\N\Debuggers\[x86 or x64]\)
- close spotify if it was already open
- start spotify
- inject pcm_recorder.dll into the spotify process with no command line args (it also owns the main window titled 'Spotify Premium')
- play a track (you won't hear anything as this dll nulls out the buffer that gets sent to the audio device)
- wait for the track to play through fully (or press the pause button if you want to stop it early)
- find raw pcm dump written in dumps folder where Spotify.exe is located

## using the dumped pcm
- open audacity, import -> raw data
- encoding: 32-bit float, 2 channels, 44100 hz
- export however you wish (if you want to maintain bit perfectness, export as wav w/ stero, 44100 khz, signed 32bit PCM)

## projects used
[safetyhook](https://github.com/cursey/safetyhook)\
[HoShiMin's SymParser](https://gist.github.com/HoShiMin/779d1c5e96e50a653ca43511b7bcb69a)
