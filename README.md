## **BPI-M2P-bsp-4.4**
Banana Pi BPI-M2P/M2Z bsp (u-boot 2014.7 & Kernel 4.4)

### **Compile**


----------


    #./build.sh 1

Target download packages in SD/ after build. Please check the build.sh and Makefile for detail compile options. If you have any compile environment issue, please try to build with [sinovoip](https://hub.docker.com/r/sinovoip/bpi-build-linux-4.4/) docker image.


### **Update build to bpi image SD card**

----------


Get the image from [bpi](http://wiki.banana-pi.org/Banana_Pi_BPI-M2%2B#Image_Release) and download it to the SD card. After finish, insert the SD card to PC

    # ./build.sh 7

Choose the type, enter the SD dev, and confirm yes, all the build packages will be installed to target SD card.

![Install](https://raw.githubusercontent.com/Dangku/readme/master/m2p_4.4/Screenshot%20from%202018-09-25%2011-30-13.png)



