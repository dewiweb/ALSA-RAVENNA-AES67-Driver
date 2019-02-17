# ALSA RAVENNA/AES67 Driver note #

## License ##

Although the Kernel part of this software is licensed under [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.en.html), the User land part (Butler) is divided into two licenses according to the following application :

**A. Integration into commercial products**
Please contact us through [here](https://www.merging.com/company/general-inquiries)

**B. Personal use**
Please find the license [here](https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm/src/master/Butler/LICENSE.md)

## Network ##
### Switches ###
We recommend to use a switch supporting multicast traffic (RFC 1112), multicast forwarding, IGMPv2 (RFC 2236), IGMP snooping (RFC 4541).
A list of tested switch is available [here](https://confluence.merging.com/display/PUBLICDOC/Network+Switches+for+RAVENNA+-+AES67)
### Throughput ###
1Gb/s switch and NIC is recommended (all Merging products can only be connected to a 1Gb/s port, except MERGING+PLAYER that support 100Mb/s).
Note : In case of 100 Mb/s link, the bandwidth allows for up to 2 channels at 384kHz or 8 channels at 192kHz

## Architecture ##
The RAVENNA ALSA implementation is splitted into 2 parts:

1. A Linux kernel module (LKM) : MergingRavennaALSA.ko
2. A a user land binary call the Daemon : Merging_RAVENNA_Daemon

### The kernel part is responsible of ###
* Registering as an ALSA driver
* Generating and receiving RTP audio packets
* PTP driven interrupt loop
* Netlink communication between user and kernel

### The Butler  part is responsible of ###
* Communication and configuration of the LKM
* High level RAVENNA/AES67 protocol implementation
  * mDNS discovery
  * SAP discovery
* RAVENNA devices sample rate arbitration
* Web server
* Remote volume control

Note : The Butler cannot be launched if the LKM has not been previously inserted. The LKM cannot be removed as long as the Butler is running

### ALSA Features ###
* 1FS to 8FS support
* PCM up to 384kHz
* Native DSD support (64/128/256) in playback only (DOP not supported)
* Interleaved and non-interleaved\** 16/24/32 bit integer formats
* Up to 64\* I/O
* Volume control

\* OEM build only. Public build is limited to 8 I/O

\** Not available in capture mode

### mDNS implementation ###

The RAVENNA protocol uses mDNS. Depending on the platform/distribution the Daemon will use Bonjour or Avahi libraries.
If Avahi is present in the system, the Daemon has to use that library. If Avahi is not present in the system, a built-in Bonjour implementation will be used instead (available only for integrators builds).

## Prerequisite ##
### Kernel ###
* GCC >= 4.9
* Linux kernel > 2.4 (3.18 for DSD support)
* The Linux kernel headers for the target.

#### Kernel Config ####
* NETFILTER
* HIGH_RES_TIMERS
* NETLINK
* Kernel > 2.4 (3.18 for DSD)
* For optimal performance in a real time environment consider changing CONFIG_HZ=1000 when building the kernel. More [here](https://www.kernel.org/doc/Documentation/timers/NO_HZ.txt)

### ALSA ###
ALSA lib superior or equal to 1.0.29 for DSD support (DSD is not yet supported in capture/receiver direction)
A [Zeroconf](https://en.wikipedia.org/wiki/Zero-configuration_networking) discovering service running on the system (Avahi only for public build).

### Architecture ###
While the Butler OEM build may be build for multiple architecture (ARM, ARM64, x86, x86_64), the public Butler is built for the x86_64 architecture only

## Setup ##
### Driver (Linux Kernel Module) ###
Pull and compile the driver
```
git clone https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm.git
cd ravenna-alsa-lkm/driver
make
```
Once you have successfully built the driver, you are able to install (reboot survive) or insert it.

* Install

```
sudo su
cp MergingRavennaALSA.ko /lib/modules/$(uname -r)/kernel/drivers
depmod
```

* Insert

```
sudo insmod MergingRavennaALSA.ko
```

### Butler ###
#### Public build run condition ####

The public Butler runs under the following conditions:

* Avahi library must be installed (custom OEM build may work with Bonjour)
* The processor architecture is limited to amd64 (x86_64)
* Tested with Ubuntu 16.04
* GLIBC >= 2.17  is required

#### Launch ####
```
cd ravenna-alsa-lkm/Butler
chmod u+x Merging_RAVENNA_Daemon
./Merging_RAVENNA_Daemon
```
Use the -d option to run it in a daemon mode.

#### Config file ####
Next to the Butler binary, you will find the merging_ravenna_daemon.conf file providing the following options :

* interface_name : Network interface name used by RAVENNA/AES67 network. e.g eth0, eth2, enc0, br1...
* device_name : By default the name is "ALSA (on <hostname\>". This can be changed but the name has to be unique on the network (used by Zeroconf) and white spaces are not supported
* web_app_port : Port number on which the RAVENNA/AES67 webserver will listen to
* web_app_path : Path of the webapp folder provided in the package. Should terminate by webapp/advanced
* tic_frame_size_at_1fs : Frame size in sample at 1Fs (44.1 / 48 kHz). e.g 48 for AES67
* config_pathname : Path where the config file will be saved e.g streamer and receiver
* max_tic_frame_size : in case of a high value of tic_frame_size_at_1fs, this have to be set to 8192
* source_name_prefix : the name of the source that will be automatically used. Useful in the high-end world

## Audio networking ##
A general presentation on Audio networking can be found on : [here](https://www.merging.com/highlights/audio-networking#audio-networking)

### Firewall ###
To ensure a correct operation of the driver, check that the following port are open :

* web server : port set in the config file TCP (default is 9090)
* mdns : 5353 UDP
* AES67 discovery : 9875 UDP

### Configuration ###
Once the Butler is successfully launched, the web page configuration web page can be accessed at the port defined in the config file (default is 9090)

<local IP of interface_name\>:9090

The web page documentation is available [here](https://confluence.merging.com/pages/viewpage.action?pageId=33260125)

### Hardware Requirement ###
* A PTP master device running on the network (this driver does not act as a PTP master). E.g. [Horus/Hapi](https://www.merging.com/products/interfaces)
* A switch supporting multicast traffic (RFC 1112), multicast forwarding, IGMPv2 (RFC 2236), IGMP snooping (RFC 4541)
* An AES67 or RAVENNA device. E.g. [Horus/Hapi](https://www.merging.com/products/interfaces)

### AES67 knowledge base ###
* [Merging and AES67 devices](http://www.merging.com/uploads/assets/Installers/ravenna/Configure%20Merging%20and%20AES67%20Devices.pdf)
* [Configure Merging and Dante devices in AES67](https://confluence.merging.com/display/DSI/Configure+Merging+and+Dante+devices+in+AES67+mode+UPDATE)

## Test ##
### ALSA check ###

The following ALSA tools can be used to check capture and playback:
* White noise on L/R channels:
```
speaker-test -D plughw:RAVENNA -r 48000 -c 2
```
* Playback of wavfile
```
aplay -D hw:1,0 -t wav -r 48000 *.wav
```
* Record
```
arecord -D hw:1,0 -f dat -d 1 -t wav filename.wav
```
* ALSA Loop
```
alsaloop -P hw:1,0 -C hw:1,0 -r 48000 -f S16_LE -l 1 -S 0
```
note: "hw:1,0" has to be changed according to your installed ALSA audio devices

### Troubleshooting ###

Sources are not discovered: ensure that mutlicast traffic is not filtered by the firewall

## Further docs ##
* [Real-time Audio on Embedded Linux](https://elinux.org/images/8/82/Elc2011_lorriaux.pdf)
* [Tools and technics for audio debugging](http://www.ti.com/lit/an/sprac10/sprac10.pdf)