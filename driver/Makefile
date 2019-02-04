#####################################################################################################
# Makefile for the Linux Kernel Module for Merging Ravenna ALSA driver
# This will build MergingRavennaALSA.ko
#####################################################################################################

obj-m := MergingRavennaALSA.o
MergingRavennaALSA-objs := c_wrapper_lib.o audio_driver.o manager.o module_main.o PTP.o module_interface.o module_netlink.o module_timer.o EtherTubeNetfilter.o RTP_streams_manager.o RTP_audio_stream.o RTP_stream.o RTP_stream_info.o MTAL_EthUtils.o MTAL_LKernelAPI.o MTConvert.o

MAKE = make
KSRC = /lib/modules/$(shell uname -r)/build/
SRC = $(shell pwd)

modules:
	$(MAKE) -C $(KSRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KSRC) M=$(SRC) modules_install

clean:
	$(MAKE) -C $(KSRC) M=$(SRC) clean

