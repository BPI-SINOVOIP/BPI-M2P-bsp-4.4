#!/bin/bash

die() {
        echo "$*" >&2
        exit 1
}

[ -s "./env.sh" ] || die "please run ./configure first."

set -e

. ./env.sh

PACK_ROOT="$TOPDIR/sunxi-pack"
#PLATFORM="linux"
PLATFORM="dragonboard"

pack_bootloader()
{
  BOARD=$1
  (
  cd $PACK_ROOT
  echo "pack -c $MACH -p $PLATFORM -b $BOARD"
  ./pack -c $MACH -p $PLATFORM -b $BOARD 
  )
  $TOPDIR/scripts/bootloader.sh $BOARD
}

echo "pack $BOARD"
pack_bootloader $BOARD
