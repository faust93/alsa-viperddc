# alsa-plugin-viperddc
Open-source VDC engine for ALSA

This is a standalone VDC engine for use with ALSA
It does not require any extra dependencies

This plugin is a port of [gst-plugin-viperddc](https://github.com/Audio4Linux/gst-plugin-viperddc.git)

### Build from sources
Clone repository
```bash
git clone https://github.com/faust93/alsa-viperddc
```

##### Dependencies required:
- ALSA Development headers and alsa-lib

Build the shared library
```bash
make
```

You should end up with `libasound_module_pcm_ddc.so`
Now you need to copy the file into one of alsa lib directories, for ex. `/usr/lib/alsa-lib`

```bash
sudo cp libasound_module_pcm_ddc.so /usr/lib/alsa-lib/
```
or

```bash
sudo make install
```

### Usage
VDC plugin operates in FLOAT32 sample format, so the audio data must be pumped through a `plug` to change the format to float under the hood:
```
pcm.<name_pcm_plug>{
    type plug;
    slave.pcm <name_pcm>;
}
```

Modify your local `.asoundrc` or `/etc/asound.conf` alsa configuration file, adding something like this:

```
pcm.viperddc {
    type ddc
#    slave.pcm "plug:dmixer"
    slave.pcm "plughw:0"
    ddc_file "/<some_folder_change_it>/V4ARISE.vdc"
}
pcm.ddc {
    type plug
    slave.pcm viperddc
}

```

You can try to produce some sound now by addressing the plugin by name, e.g.:
```bash
mpg123 -a ddc kalinka_malinka.mp3
```

