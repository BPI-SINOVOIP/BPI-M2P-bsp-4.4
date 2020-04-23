## **BPI-M2P-bsp-4.4**
Banana Pi BPI-M2P/M2Z bsp (u-boot 2014.7 & Kernel 4.4)

### **Compile**


----------

**Build M2P**

    #./build.sh BPI-M2P-720P 1

**Build M2Z**

    #./build.sh BPI-M2Z-720P 1

Target download packages in SD/ after build. Please check the build.sh and Makefile for detail compile options. If you have any compile environment issue, please try to build with [sinovoip](https://hub.docker.com/r/sinovoip/bpi-build-linux-4.4/) docker image.


### **Update build to bpi image SD card**

----------


Get the image from [bpi](http://wiki.banana-pi.org/Banana_Pi_BPI-M2%2B#Image_Release) and download it to the SD card. After finish, insert the SD card to PC

    # ./build.sh 7

Choose the type, enter the SD dev, and confirm yes, all the build packages will be installed to target SD card.

