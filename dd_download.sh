sudo gunzip -c SD/bpi-m2z/100MB/BPI-M2Z-720P-linux4.4-8k.img.gz | dd of=$1 bs=1024 seek=8
sync
cd SD/bpi-m2z
sudo bpi-update -d $1
