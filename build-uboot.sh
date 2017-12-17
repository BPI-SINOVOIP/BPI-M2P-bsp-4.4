#!/bin/bash
# b.sh
#
# (c) Copyright 2013
# Allwinner Technology Co., Ltd. <www.allwinnertech.com>
# wangwei <wangwei@allwinnertech.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.


BUILD_CHIP="sun50iw2p1"
BUILD_CHIP="sun8iw7p1"
BUILD_UBOOT="u-boot-sunxi"
BUILD_CONFIG_FILE=".config"
set -e

build_select_chip()
{

	local count=0
	printf "All valid Sunxi chip:\n"
	for chip in $( find "$BUILD_UBOOT/arch/arm/cpu/armv7/" -mindepth 1 -maxdepth 1 -type d -name "sun[0-9]*" |sort); do
		chips[$count]=`basename $chip`
		printf "$count. ${chips[$count]}\n"
		let count=$count+1
	done

	while true; do
		read -p "Please select a chip:"
		RES=`expr match $REPLY "[0-9][0-9]*$"`
		if [ "$RES" -le 0 ]; then
			echo "please use index number"
			continue
		fi
		if [ "$REPLY" -ge $count ]; then
			echo "too big"
			continue
		fi
		if [ "$REPLY" -lt "0" ]; then
			echo "too small"
			continue
		fi
		break
	done
    BUILD_CHIP=${chips[$REPLY]}
}


build_select_build_uboot()
{
	local count=0
	local length=0

#	build_uboot=(u-boot-2011.09 u-boot-2014.07)
	build_uboot=(u-boot-sunxi)
	build_uboot=(u-boot-2011.09 u-boot-sunxi)
	printf "All valid u-boot version:\n"

	length=`expr ${#build_uboot[@]} - 1`
	for count in `seq 0 $length`; do
		printf "$count. ${build_uboot[$count]}\n"
	done

	let count=$count+1

	while true; do
		read -p "Please select a build type:"
		RES=`expr match $REPLY "[0-9][0-9]*$"`
		if [ "$RES" -le 0 ]; then
			echo "please use index number"
			continue
		fi
		if [ "$REPLY" -ge $count ]; then
			echo "too big"
			continue
		fi
		if [ "$REPLY" -lt "0" ]; then
			echo "too small"
			continue
		fi
		break
	done

	BUILD_UBOOT=${build_uboot[$REPLY]}

}

build_get_config_form_user()
{
	build_select_build_uboot
	build_select_chip
}

build_write_config_to_file()
{
	rm -rf $BUILD_CONFIG_FILE
	echo "BUILD_CHIP       :${BUILD_CHIP}" >> $BUILD_CONFIG_FILE
	echo "BUILD_UBOOT        :$BUILD_UBOOT" >> $BUILD_CONFIG_FILE
}

build_get_config_from_file()
{
	BUILD_CHIP=`cat $BUILD_CONFIG_FILE | awk -F"[:|=]" '(NF&&$1~/^[[:space:]]*BUILD_CHIP/) {printf "%s",$2}'`
	BUILD_UBOOT=`cat $BUILD_CONFIG_FILE | awk -F"[:|=]" '(NF&&$1~/^[[:space:]]*BUILD_UBOOT/) {printf "%s",$2}'`
}

build_show_config()
{
	printf "\nconfig information is:\n"
	echo -e '\033[0;31;36m'
	printf "BUILD_CHIP       : ${BUILD_CHIP}\n"
	printf "BUILD_UBOOT      : ${BUILD_UBOOT}\n"
	echo -e '\033[0m'
}

build_show_help()
{
printf "
(c) Copyright 2016
Allwinner Technology Co., Ltd. <www.allwinnertech.com>
wangwei <wangwei@allwinnertech.com>

NAME
	build - The top level build script to build Sunxi platform bootloader

OPTIONS
	-h             display help message
	config         config the platform which we want to build
	clean          clean the tmp file
	distclean      clean the tmp file and configure file
	showconfig     show the current compile config
	uboot          build uboot
	boot0          build boot0
	fes            build fes
	sboot          build secure boot
	spl            build boot0 fes sboot in one time
	pack           pack img
	pack_debug     pack img,switch uart to card0
	pack_secure    pack secure img

"

}


if [ "$1" == "-h" ]; then
	build_show_help
	exit
fi


if [[ "$1" == config ]]; then
	if [[ -z "$2" ]]; then
		build_get_config_form_user
	else
		BUILD_CHIP=$2
	fi
	build_write_config_to_file
	cd $BUILD_UBOOT
	make distclean

	if [[ "$BUILD_UBOOT" == "u-boot-2011.09" ]]; then
		make ${BUILD_CHIP}p1_config
	else
		make ${BUILD_CHIP}_config
	fi

	cd ..
	exit
fi

if [ -f $BUILD_CONFIG_FILE ]; then
	build_get_config_from_file
else
	echo -e '\033[0;31;36m'
	echo "you should run ./build-uboot.sh config at first"
	echo -e '\033[0m'
	exit
fi

#
# Build the u-boot SunxiPlatform code
#

#build_show_config

for arg in "$@"
do
	if [[ $arg == clean ]]; then
		echo "clean the build..."
		cd $BUILD_UBOOT; make clean; cd ..
		exit
	elif [[ $arg == distclean ]]; then
		echo "distclean the build..."
		cd $BUILD_UBOOT; make distclean; cd ..
		rm -rf $BUILD_CONFIG_FILE
		exit
	elif [[ $arg == showconfig ]]; then
		build_show_config
		exit
	elif [[ $arg == uboot ]]; then
		cd $BUILD_UBOOT
		make -j
		cd ..
		exit
	elif [[ $arg == boot0 ]]; then
		cd $BUILD_UBOOT
		make boot0
		cd ..
		exit
	elif [[ $arg == fes ]]; then
		cd $BUILD_UBOOT
		make fes
		cd ..
		exit
	elif [[ $arg == sboot ]]; then
		cd $BUILD_UBOOT
		make sboot
		cd ..
		exit
	elif [[ $arg == spl ]]; then
		cd $BUILD_UBOOT
		make spl
		cd ..
		exit
	elif [[ $arg == pack ]]; then
		cd ..
		./build-uboot.sh pack
		cd brandy
		exit
	elif [[ $arg == pack_secure ]]; then
		cd ..
		./build-uboot.sh pack_secure
		cd brandy
		exit
	elif [[ $arg == pack_debug ]]; then
		cd ..
		./build-uboot.sh pack_debug
		cd brandy
		exit
	else
		echo "invalid paramters"
		exit
	fi
done


#
#if the input paramters is null, just build all.
#
cd $BUILD_UBOOT
#make -j && make spl
make -j
cd ..


