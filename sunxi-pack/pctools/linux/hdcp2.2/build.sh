#!/bin/sh

printf 'Building Tx HDCP key ...\n'
./utils/hdcpkeys -b vendor/HDCP2_TX_KEY.bin -t 2
./utils/esm_swap -i dcp_TRANSMITTER.out -o vendor/hdcp_keys.le -s 1616 -t 1

printf 'Building encrypted firmware image ...\n'
./esmtool -t firware -f utils/sample.aic -k vendor/hdcp_keys.le -p vendor/hdcp2pkf.bin -o esm.fex

rm -rf dcp_TRANSMITTER.out

printf 'All finished.\n'
