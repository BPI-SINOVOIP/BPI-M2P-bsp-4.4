::-------------����rootfs.iso
call fsbuild.bat

::-------------����rootfs.az
..\..\..\..\eStudio\Softwares\az\az e rootfs.iso setup\rootfs.az -fastmode
del rootfs.iso

::-------------����zdisk.img
pushd .
cd setup
call makezdisk.bat
popd

::-------------����zdisk.img��У��ֵ
..\..\..\..\eStudio\Softwares\eDragonEx200\FileAddSum.exe  setup\zdisk.img  ..\eFex\verify.fex

::-------------����ePDKv100.img
del ePDKv100.img
..\..\..\..\eStudio\Softwares\eDragonEx200\dragon image.cfg  > image.txt
del image.bin

pushd .
cd setup
del zdisk.img
del rootfs.az
popd

pushd .
cd ..\efex
del verify.fex
popd

pause
