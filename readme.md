# Raspberry Pi WS2812 LED driver

An experimental Raspberry Pi 16-channel WS2812 NeoPixel LED driver.

Based upon the original code by Jeremy P. Bentham at [https://github.com/jbentham/rpi](https://github.com/jbentham/rpi), introduced in [this blog post](https://iosoft.blog/2020/09/29/raspberry-pi-multi-channel-ws2812/), and upon the various forks, PRs, issues, comments and discussions about the original code.

Currently work-in-progress, low-quality code, *but* it kinda works.
* Have tried to isolate the various components of the code in a library-like structure. 
* Have refactored so it runs on 64-bits (no idea if it still runs on 32-bits?).
* Have dealt with e.g. memcpy and memset causing bus errors on e.g. Pi Zero 2.

References
* Raspberry Pi [pinout](https://pinout.xyz/)
* More J. Bentham posts about [SMI](https://iosoft.blog/2020/07/16/raspberry-pi-smi/) or [DMA](https://iosoft.blog/2020/05/25/raspberry-pi-dma-programming/)
* Some metal [infos](https://www.rpi4os.com/part5-framebuffer/) and [infos](https://www.codeembedded.com/blog/)

Still need to investigate the effects of concurrent DMA acceses and whether we should disable some stuff on the Pi such as HDMI or audio. Use at your own risk.

Original code does not seem to have a license. This is available under the MIT license.