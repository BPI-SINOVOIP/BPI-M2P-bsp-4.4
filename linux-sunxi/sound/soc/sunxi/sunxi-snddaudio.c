/*
 * sound\soc\sunxi\sunxi_snddaudio.c
 * (C) Copyright 2014-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Wolfgang Huang <huangjinhui@allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/io.h>
#include <linux/of.h>

static int cnt;

static int sunxi_snddaudio_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	unsigned int freq, clk_div;
	int ret;

	switch (params_rate(params)) {
	case 8000:
	case 16000:
	case 32000:
	case 64000:
	case 128000:
	case 24000:
	case 48000:
	case 96000:
	case 192000:
		freq = 24576000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
	case 176400:
		freq = 22579200;
		break;
	default:
		dev_err(card->dev, "unsupport params rate\n");
		return -EINVAL;
	}

	/* set platform clk source freq and set the mode as daudio or pcm */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, freq, 0);
	if (ret < 0)
		return ret;

	/* set codec clk source freq and set the mode as daudio or pcm */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, freq, 0);
	if (ret < 0)
		dev_warn(card->dev, "codec_dai set sysclk failed\n");

	/* set codec dai fmt */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		dev_warn(card->dev, "codec dai set fmt failed\n");

	/* set system clk div */
	clk_div = freq / params_rate(params);
	ret = snd_soc_dai_set_clkdiv(cpu_dai, 0, clk_div);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, 0, clk_div);
	if (ret < 0)
		dev_warn(card->dev, "codec_dai set clkdiv failed\n");
	return 0;
}

/* sunxi card initialization */
static int sunxi_snddaudio_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct snd_soc_ops sunxi_snddaudio_ops = {
	.hw_params      = sunxi_snddaudio_hw_params,
};

static struct snd_soc_dai_link sunxi_snddaudio_dai_link = {
	.name           = "sysvoice",
	.stream_name    = "SUNXI-AUDIO",
	.cpu_dai_name   = "sunxi-daudio",
	.platform_name  = "sunxi-daudio",
	.codec_dai_name = "snd-soc-dummy-dai",
	.codec_name     = "snd-soc-dummy",
	.init           = sunxi_snddaudio_init,
	.ops            = &sunxi_snddaudio_ops,
};

static struct snd_soc_card snd_soc_sunxi_snddaudio = {
	.name           = "snddaudio",
	.owner          = THIS_MODULE,
	.dai_link       = &sunxi_snddaudio_dai_link,
	.num_links      = 1,
};

static int sunxi_snddaudio_dev_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_card *card = &snd_soc_sunxi_snddaudio;
	int ret;
	char name[30] = "snddaudio";

	card->dev = &pdev->dev;
	sunxi_snddaudio_dai_link.cpu_dai_name = NULL;
	sunxi_snddaudio_dai_link.cpu_of_node = of_parse_phandle(np,
				"sunxi,daudio0-controller", 0);
	if (sunxi_snddaudio_dai_link.cpu_of_node)
		goto cpu_node_find;

	sunxi_snddaudio_dai_link.cpu_of_node = of_parse_phandle(np,
			"sunxi,daudio1-controller", 0);
	if (sunxi_snddaudio_dai_link.cpu_of_node)
		goto cpu_node_find;

	sunxi_snddaudio_dai_link.cpu_of_node = of_parse_phandle(np,
			"sunxi,daudio-controller", 0);
	if (sunxi_snddaudio_dai_link.cpu_of_node)
		goto cpu_node_find;

	dev_err(card->dev, "Perperty 'sunxi,daudio-controller' missing\n");
	return -EINVAL;

cpu_node_find:
	sunxi_snddaudio_dai_link.platform_name = NULL;
	sunxi_snddaudio_dai_link.platform_of_node =
				sunxi_snddaudio_dai_link.cpu_of_node;

	ret = of_property_read_string(np, "sunxi,snddaudio-codec",
			&sunxi_snddaudio_dai_link.codec_name);
	/*
	 * As we setting codec & codec_dai in dtb, we just setting the
	 * codec & codec_dai in the dai_link. But if we just not setting,
	 * we then using the snd-soc-dummy as the codec & codec_dai to
	 * construct the snd-card for audio playback & capture.
	 */
	if (!ret) {
		ret = of_property_read_string(np, "sunxi,snddaudio-codec-dai",
				&sunxi_snddaudio_dai_link.codec_dai_name);
		if (ret < 0) {
			dev_err(card->dev, "codec_dai name invaild in dtb\n");
			return -EINVAL;
		}
		sprintf(name+3, "%s", sunxi_snddaudio_dai_link.codec_name);
	} else {
		sprintf(name+9, "%d", cnt++);
	}

	card->name = name;
	dev_info(card->dev, "codec: %s, codec_dai: %s.\n",
			sunxi_snddaudio_dai_link.codec_name,
			sunxi_snddaudio_dai_link.codec_dai_name);

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(card->dev, "snd_soc_register_card failed\n");

	return ret;
}

static int sunxi_snddaudio_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id sunxi_snddaudio_of_match[] = {
	{ .compatible = "allwinner,sunxi-daudio0-machine", },
	{ .compatible = "allwinner,sunxi-daudio1-machine", },
	{ .compatible = "allwinner,sunxi-daudio2-machine", },
	{ .compatible = "allwinner,sunxi-daudio3-machine", },
	{},
};

static struct platform_driver sunxi_snddaudio_driver = {
	.driver = {
		.name   = "snddaudio",
		.owner  = THIS_MODULE,
		.pm     = &snd_soc_pm_ops,
		.of_match_table = sunxi_snddaudio_of_match,
	},
	.probe  = sunxi_snddaudio_dev_probe,
	.remove = sunxi_snddaudio_dev_remove,
};

module_platform_driver(sunxi_snddaudio_driver);

MODULE_AUTHOR("Wolfgang Huang");
MODULE_DESCRIPTION("SUNXI snddaudio ALSA SoC Audio Card Driver");
MODULE_LICENSE("GPL");
