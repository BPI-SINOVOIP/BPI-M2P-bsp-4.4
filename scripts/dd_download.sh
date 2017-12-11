#!/bin/sh

# make partition table by fdisk command
# reserve part for fex binaries download 0~204799
# partition1 /dev/sdc1 vfat 204800~327679
# partition2 /dev/sdc2 ext4 327680~end

die() {
        echo "$*" >&2
        exit 1
}

[ $# -eq 1 ] || die "Usage: $0 /dev/sdc"

[ -s "./env.sh" ] || die "please run ./configure first."

. ./env.sh

O=$1
P=../output/$BOARD/pack
P=${TOPDIR}/sunxi-pack/out

LOOP_DEV=$O
sudo dd if=$P/boot0_sdcard.fex	of=${LOOP_DEV} bs=1k seek=8
sudo dd if=$P/boot_package.fex	of=${LOOP_DEV} bs=1k seek=16400
sudo dd if=$P/sunxi_mbr.fex 	of=${LOOP_DEV} bs=1k seek=20480
sudo dd if=$P/boot-resource.fex of=${LOOP_DEV} bs=1k seek=36864
sudo dd if=$P/env.fex 		of=${LOOP_DEV} bs=1k seek=53248
#sudo dd if=$P/boot.fex 	of=${LOOP_DEV} bs=1k seek=69632
sync
eject $O
