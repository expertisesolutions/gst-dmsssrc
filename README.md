# GST-DMSS
Gstreamer source plugin for Dahua cameras (and incidentally, Intelbras IP cameras).

There are two plugins defined here:
- **dmsssrc**: connect to the ip camera and receive its data.
- **dmssdemux**: demux dmsssrc data into an audio and video sources.

---
## Build and Install
The dependencies here are [gstreamer-1.0](https://gstreamer.freedesktop.org/), [meson](https://mesonbuild.com) and [ninja](https://ninja-build.org).
To build and install:
```
meson build
meson compile -C build
sudo meson install -C build
```

---
## Usage:
Be sure to add the install path to the `GST_PLUGIN_PATH` environment varible (read more [here](https://gstreamer.freedesktop.org/documentation/gstreamer/running.html?gi-language=c)).

### Inspect
To see all parameters and its descriptions, run:

```
GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0 gst-inspect-1.0 dmsssrc
GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0 gst-inspect-1.0 dmssdemux
```

### Basic examples
To connect to a camera with the ip address (**host**) `192.168.1.108`, **port** `377777`, **user** `admin`, **password** `admin` at **channel** `5`:

``` 
GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0 gst-launch-1.0 \
  dmsssrc host=192.168.1.108 port=37777 user=admin password=admin channel=5
```

Now to get the audio and video from **dmsssrc**, **dmssdemux** can be used as shown below:

``` 
GST_PLUGIN_PATH=/usr/local/lib/gstreamer-1.0 gst-launch-1.0 \
  dmsssrc host=192.168.1.108 port=37777 user=admin password=admin channel=5 \
  ! dmssdemux name=d \
    d. ! audioconvert ! audioresample ! autoaudiosink \
    d. ! videoconvert ! autovideosink
```

### More examples
Check out [dahua-babysitter](https://github.com/felipealmeida/dahua-babysitter) to see a gstreamer app based on this plugin.
