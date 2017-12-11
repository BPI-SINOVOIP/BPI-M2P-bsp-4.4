
# esmtool Usage

* Generate HDCP2.2 firware: esm.img
  > ./esmtool -t firware -f utils/sample.aic -k vendor/hdcp_keys.le -p vendor/hdcp2pkf.bin -o esm.fex

# Configuration

All parameters specified in the configuration file(utils/sample.aic).
Change it parameters specified in the configuration file,
But please DO NOT modify unless you are sure what you do.

	Name          Type           Description
	PKf           Byte string    Platform fuse key (16 bytes)
	IK            Byte string    Image key (16 bytes)
	IVc           Byte string    Code IV (12 bytes)
	BBRlen        Integer        BBR length (total)
	CLL           Integer        Cache line length
	MAClen        Integer        MAC length
	Koff          Integer        If set, the IK value is written into the image at the specified offset in the ROM data.


