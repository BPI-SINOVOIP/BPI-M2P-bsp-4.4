/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "dev_disp.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/memblock.h>

#define VSYNC_NUM 4
struct fb_info_t {
	struct device *dev;

	bool fb_enable[FB_MAX];
	enum disp_fb_mode fb_mode[FB_MAX];
	u32 layer_hdl[FB_MAX][2];	/* channel, layer_id */
	struct fb_info *fbinfo[FB_MAX];
	struct disp_fb_create_info fb_para[FB_MAX];
	u32 pseudo_palette[FB_MAX][16];
	wait_queue_head_t wait[3];
	unsigned long wait_count[3];
	struct task_struct *vsync_task[3];
	spinlock_t slock[DISP_SCREEN_NUM];
	ktime_t vsync_timestamp[DISP_SCREEN_NUM][VSYNC_NUM];
	u32 vsync_timestamp_head[DISP_SCREEN_NUM];
	u32 vsync_timestamp_tail[DISP_SCREEN_NUM];

	int blank[3];
};

static struct fb_info_t g_fbi;
static phys_addr_t bootlogo_addr;
static int bootlogo_sz;

#define FBHANDTOID(handle)  ((handle) - 100)
#define FBIDTOHAND(ID)  ((ID) + 100)

static struct __fb_addr_para g_fb_addr;

s32 sunxi_get_fb_addr_para(struct __fb_addr_para *fb_addr_para)
{
	if (fb_addr_para) {
		fb_addr_para->fb_paddr = g_fb_addr.fb_paddr;
		fb_addr_para->fb_size = g_fb_addr.fb_size;
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL(sunxi_get_fb_addr_para);

#define sys_put_wvalue(addr, data) writel(data, (void __iomem *)addr)
s32 fb_draw_colorbar(char *base, u32 width, u32 height,
		     struct fb_var_screeninfo *var)
{
	u32 i = 0, j = 0;
	u32 color, a, r, g, b;

	if (!base)
		return -1;

	/*do not draw colorbar when boot sync*/
	if (g_disp_drv.para.boot_info.sync == 1)
		return 0;

	a = ((1 << var->transp.length) - 1)
	    << var->transp.offset;
	r = ((1 << var->red.length) - 1)
	    << var->red.offset;
	g = ((1 << var->green.length) - 1)
	    << var->green.offset;
	b = ((1 << var->blue.length) - 1)
	    << var->blue.offset;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width / 4; j++) {
			u32 offset = 0;

			if (var->bits_per_pixel == 32) {
				offset = width * i + j;
				color = a | r;
				sys_put_wvalue(base + offset * 4, color);

				offset = width * i + j + width / 4;
				color = a | g;
				sys_put_wvalue(base + offset * 4, color);

				offset = width * i + j + width / 4 * 2;
				color = a | b;
				sys_put_wvalue(base + offset * 4, color);

				offset = width * i + j + width / 4 * 3;
				color = a | r | g;
				sys_put_wvalue(base + offset * 4, color);
			}
#if 0
			else if (var->bits_per_pixel == 16) {
				offset = width * i + j;
				color = a | r;
				sys_put_hvalue(base + offset * 2, color);

				offset = width * i + j + width / 4;
				color = a | g;
				sys_put_hvalue(base + offset * 2, color);

				offset = width * i + j + width / 4 * 2;
				color = a | b;
				sys_put_hvalue(base + offset * 2, color);

				offset = width * i + j + width / 4 * 3;
				color = a | r | g;
				sys_put_hvalue(base + offset * 2, color);
			}
#endif
		}
	}

	return 0;
}

s32 fb_draw_gray_pictures(char *base, u32 width, u32 height,
			  struct fb_var_screeninfo *var)
{
	u32 time = 0;

	for (time = 0; time < 18; time++) {
		u32 i = 0, j = 0;

		for (i = 0; i < height; i++) {
			for (j = 0; j < width; j++) {
				char *addr = base + (i * width + j) * 4;
				u32 value =
				    (0xff << 24) | ((time * 15) << 16) |
				    ((time * 15) << 8) | (time * 15);

				sys_put_wvalue((void *)addr, value);
			}
		}
	}
	return 0;
}

static int fb_map_video_memory(struct fb_info *info)
{
	info->screen_base =
	    (char __iomem *)disp_malloc(info->fix.smem_len,
					(u32 *) (&info->fix.smem_start));
	if (info->screen_base) {
		__inf("%s(reserve),va=0x%p, pa=0x%p size:0x%x\n", __func__,
		      (void *)info->screen_base,
		      (void *)info->fix.smem_start,
		      (unsigned int)info->fix.smem_len);
		memset((void *__force)info->screen_base, 0x0,
		       info->fix.smem_len);

		g_fb_addr.fb_paddr = (uintptr_t) info->fix.smem_start;
		g_fb_addr.fb_size = info->fix.smem_len;

		return 0;
	}

	__wrn("disp_malloc fail!\n");
	return -ENOMEM;

	return 0;
}

static inline void fb_unmap_video_memory(struct fb_info *info)
{
	if (!info->screen_base) {
		__wrn("%s: screen_base is null\n", __func__);
		return;
	}
	__inf("%s: screen_base=0x%p, smem=0x%p, len=0x%x\n", __func__,
	      (void *)info->screen_base,
	      (void *)info->fix.smem_start, info->fix.smem_len);
	disp_free((void *__force)info->screen_base,
		  (void *)info->fix.smem_start, info->fix.smem_len);
	info->screen_base = 0;
	info->fix.smem_start = 0;
	g_fb_addr.fb_paddr = 0;
	g_fb_addr.fb_size = 0;
}

static void *Fb_map_kernel(unsigned long phys_addr, unsigned long size)
{
	int npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct page *cur_page = phys_to_page(phys_addr);
	pgprot_t pgprot;
	void *vaddr = NULL;
	int i;

	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++)
		*(tmp++) = cur_page++;

	pgprot = pgprot_noncached(PAGE_KERNEL);
	vaddr = vmap(pages, npages, VM_MAP, pgprot);

	vfree(pages);
	return vaddr;
}

static void *Fb_map_kernel_cache(unsigned long phys_addr, unsigned long size)
{
	int npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct page *cur_page = phys_to_page(phys_addr);
	pgprot_t pgprot;
	void *vaddr = NULL;
	int i;

	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++)
		*(tmp++) = cur_page++;

	pgprot = PAGE_KERNEL;
	vaddr = vmap(pages, npages, VM_MAP, pgprot);

	vfree(pages);
	return vaddr;
}

static void Fb_unmap_kernel(void *vaddr)
{
	vunmap(vaddr);
}

static s32 disp_fb_to_var(enum disp_pixel_format format,
			  struct fb_var_screeninfo *var)
{
	switch (format) {
	case DISP_FORMAT_ARGB_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;
		break;
	case DISP_FORMAT_ABGR_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;
		break;
	case DISP_FORMAT_RGBA_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->blue.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		break;
	case DISP_FORMAT_BGRA_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->red.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		break;
	case DISP_FORMAT_RGB_888:
		var->bits_per_pixel = 24;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGR_888:
		var->bits_per_pixel = 24;
		var->transp.length = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_RGB_565:
		var->bits_per_pixel = 16;
		var->transp.length = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGR_565:
		var->bits_per_pixel = 16;
		var->transp.length = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_ARGB_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 4;
		var->blue.length = 4;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;

		break;
	case DISP_FORMAT_ABGR_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 4;
		var->blue.length = 4;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;

		break;
	case DISP_FORMAT_RGBA_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 5;
		var->blue.length = 4;
		var->transp.offset = 0;
		var->blue.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGRA_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 4;
		var->blue.length = 4;
		var->transp.offset = 0;
		var->red.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_ARGB_1555:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;

		break;
	case DISP_FORMAT_ABGR_1555:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;

		break;
	case DISP_FORMAT_RGBA_5551:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->blue.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGRA_5551:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->red.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	default:
		__wrn("[FB]not support format %d\n", format);
	}

	__inf
	    ("fmt%d para: %dbpp, a(%d,%d),r(%d,%d),g(%d,%d),b(%d,%d)\n",
	     (int)format, (int)var->bits_per_pixel, (int)var->transp.offset,
	     (int)var->transp.length, (int)var->red.offset,
	     (int)var->red.length, (int)var->green.offset,
	     (int)var->green.length, (int)var->blue.offset,
	     (int)var->blue.length);

	return 0;
}

static s32 var_to_disp_fb(struct disp_fb_info *fb,
			  struct fb_var_screeninfo *var,
			  struct fb_fix_screeninfo *fix)
{
	if (var->nonstd == 0) {
		/* argb */
		switch (var->bits_per_pixel) {
		case 32:
			if (var->red.offset == 16 && var->green.offset == 8
			    && var->blue.offset == 0)
				fb->format = DISP_FORMAT_ARGB_8888;
			else if (var->blue.offset == 24
				 && var->green.offset == 16
				 && var->red.offset == 8)
				fb->format = DISP_FORMAT_BGRA_8888;
			else if (var->blue.offset == 16
				 && var->green.offset == 8
				 && var->red.offset == 0)
				fb->format = DISP_FORMAT_ABGR_8888;
			else if (var->red.offset == 24
				 && var->green.offset == 16
				 && var->blue.offset == 8)
				fb->format = DISP_FORMAT_RGBA_8888;
			else
				__wrn
				    ("invalid fmt,off<a:%d,r:%d,g:%d,b:%d>\n",
				     var->transp.offset, var->red.offset,
				     var->green.offset, var->blue.offset);

			break;
		case 24:
			if ((var->red.offset == 16)
			 && (var->green.offset == 8)
			 && (var->blue.offset == 0)) {	/* rgb */
				fb->format = DISP_FORMAT_RGB_888;
			} else if ((var->blue.offset == 16)
			       && (var->green.offset == 8)
			       && (var->red.offset == 0)) {	/* bgr */
				fb->format = DISP_FORMAT_BGR_888;
			} else {
				__wrn
				    ("invalid fmt,off<a:%d,r:%d,g:%d,b:%d>\n",
				     var->transp.offset, var->red.offset,
				     var->green.offset, var->blue.offset);
			}

			break;
		case 16:
			if (var->red.offset == 11 && var->green.offset == 5
			    && var->blue.offset == 0) {
				fb->format = DISP_FORMAT_RGB_565;
			} else if (var->blue.offset == 11
				   && var->green.offset == 5
				   && var->red.offset == 0) {
				fb->format = DISP_FORMAT_BGR_565;
			} else if (var->transp.offset == 12
				   && var->red.offset == 8
				   && var->green.offset == 4
				   && var->blue.offset == 0) {
				fb->format = DISP_FORMAT_ARGB_4444;
			} else if (var->transp.offset == 12
				   && var->blue.offset == 8
				   && var->green.offset == 4
				   && var->red.offset == 0) {
				fb->format = DISP_FORMAT_ABGR_4444;
			} else if (var->red.offset == 12
				   && var->green.offset == 8
				   && var->blue.offset == 4
				   && var->transp.offset == 0) {
				fb->format = DISP_FORMAT_RGBA_4444;
			} else if (var->blue.offset == 12
				   && var->green.offset == 8
				   && var->red.offset == 4
				   && var->transp.offset == 0) {
				fb->format = DISP_FORMAT_BGRA_4444;
			} else if (var->transp.offset == 15
				   && var->red.offset == 10
				   && var->green.offset == 5
				   && var->blue.offset == 0) {
				fb->format = DISP_FORMAT_ARGB_1555;
			} else if (var->transp.offset == 15
				   && var->blue.offset == 10
				   && var->green.offset == 5
				   && var->red.offset == 0) {
				fb->format = DISP_FORMAT_ABGR_1555;
			} else if (var->red.offset == 11
				   && var->green.offset == 6
				   && var->blue.offset == 1
				   && var->transp.offset == 0) {
				fb->format = DISP_FORMAT_RGBA_5551;
			} else if (var->blue.offset == 11
				   && var->green.offset == 6
				   && var->red.offset == 1
				   && var->transp.offset == 0) {
				fb->format = DISP_FORMAT_BGRA_5551;
			} else {
				__wrn
				    ("invalid fmt,off<a:%d,r:%d,g:%d,b:%d>\n",
				     var->transp.offset, var->red.offset,
				     var->green.offset, var->blue.offset);
			}

			break;

		default:
			__wrn("invalid bits_per_pixel :%d\n",
			      var->bits_per_pixel);
			return -EINVAL;
		}
	}
	__inf
	    ("format%d,para:%dbpp,a(%d,%d),r(%d,%d),g(%d,%d),b(%d,%d)\n",
	     (int)fb->format, (int)var->bits_per_pixel, (int)var->transp.offset,
	     (int)var->transp.length, (int)var->red.offset,
	     (int)var->red.length, (int)var->green.offset,
	     (int)var->green.length, (int)var->blue.offset,
	     (int)var->blue.length);

	fb->size[0].width = var->xres_virtual;
	fb->size[1].width = var->xres_virtual;
	fb->size[2].width = var->xres_virtual;

	fix->line_length = (var->xres_virtual * var->bits_per_pixel) / 8;

	return 0;
}

static int sunxi_fb_open(struct fb_info *info, int user)
{
	return 0;
}
static int sunxi_fb_release(struct fb_info *info, int user)
{
	return 0;
}

static int fb_wait_for_vsync(struct fb_info *info)
{
	unsigned long count;
	u32 sel = 0;
	int ret;
	int num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	for (sel = 0; sel < num_screens; sel++) {
		if (sel == g_fbi.fb_mode[info->node]) {
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			if (!mgr || !mgr->device
			    || (mgr->device->is_enabled == NULL))
				return 0;

			if (mgr->device->is_enabled(mgr->device) == 0)
				return 0;

			count = g_fbi.wait_count[sel];
			ret =
			    wait_event_interruptible_timeout(g_fbi.wait[sel],
							     count !=
							     g_fbi.
							     wait_count[sel],
							     msecs_to_jiffies
							     (50));
			if (ret == 0) {
				__inf("timeout\n");
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

#if 0
static void print_layer_config_info(char *str, struct disp_layer_config *config)
{
	printk("%s:\n", str);
	printk("enable = %u\n", config->enable);
	printk("channel = %u\n", config->channel);
	printk("layer_id = %u\n", config->layer_id);
	printk("info:\n");
	printk("	mode = %u\n", config->info.mode);
	printk("	screen_win = [%d %d %u %u]\n", config->info.screen_win.x, config->info.screen_win.y,
		config->info.screen_win.width, config->info.screen_win.height);
	printk("	fb:\n");
	printk("		addr[0] = 0x%08llx\n", config->info.fb.addr[0]);
	printk("		addr[1] = 0x%08llx\n", config->info.fb.addr[1]);
	printk("		addr[2] = 0x%08llx\n", config->info.fb.addr[2]);
	printk("		size[0] = [%u %u]\n", config->info.fb.size[0].width, config->info.fb.size[0].height);
	printk("		size[1] = [%u %u]\n", config->info.fb.size[1].width, config->info.fb.size[1].height);
	printk("		size[2] = [%u %u]\n", config->info.fb.size[2].width, config->info.fb.size[2].height);
	printk("		crop[0] = [%lld %lld %lld %lld]\n", (config->info.fb.crop.x >> 32), (config->info.fb.crop.y >> 32),
		(config->info.fb.crop.width >> 32), (config->info.fb.crop.height >> 32));
	printk("		format = %u\n", config->info.fb.format);
}
#else
static void print_layer_config_info(char *str, struct disp_layer_config *config) {}
#endif

static int sunxi_fb_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	u32 sel = 0;
	u32 num_screens;
	int need_wait_vsync = 1;
	s32 type_switch_flag = 0;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("fb %d, pos=%d,%d\n", info->node, var->xoffset, var->yoffset);

	for (sel = 0; sel < num_screens; sel++) {
		if (sel == g_fbi.fb_mode[info->node]) {
			u32 buffer_num = 1;
			u32 y_offset = 0;
			s32 chan = g_fbi.layer_hdl[info->node][0];
			s32 layer_id = g_fbi.layer_hdl[info->node][1];
			struct disp_rect dirty_rect;
			struct disp_layer_config config;
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			memset(&config, 0, sizeof(struct disp_layer_config));
			if (mgr && mgr->get_layer_config
			    && mgr->set_layer_config) {
				config.channel = chan;
				config.layer_id = layer_id;
				if (mgr->get_layer_config(mgr, &config, 1)
				    != 0) {
					__wrn
					    ("fb%d,get_lyr_cfg(%d,%d,%d)fail\n",
					     info->node, sel, chan, layer_id);
					return -1;
				}

				if (mgr->rot_sw) {
					if (0 != var->reserved[2] && 0 != var->reserved[3]) {
						dirty_rect.x = var->reserved[0];
						dirty_rect.y = var->reserved[1];
						dirty_rect.width = var->reserved[2];
						dirty_rect.height = var->reserved[3];
					} else {
						dirty_rect.x = 0;
						dirty_rect.y = 0;
						dirty_rect.width = 0;
						dirty_rect.height = 0;
						return 0;
					}

					if (mgr->rot_sw->checkout) {
						type_switch_flag = mgr->rot_sw->checkout(mgr->rot_sw, &config);
						if (type_switch_flag == 1) {
							/* update the total buffer if disp device switch */
							dirty_rect.x = 0;
							dirty_rect.y = 0;
							dirty_rect.width = config.info.fb.size[0].width;
							dirty_rect.height = config.info.fb.size[0].height;
						}
					}
				}
				config.info.fb.crop.x =
				    ((long long)var->xoffset) << 32;
				config.info.fb.crop.y =
				    ((unsigned long long)(var->yoffset +
							  y_offset)) << 32;
				config.info.fb.crop.width =
				    ((long long)var->xres) << 32;
				config.info.fb.crop.height =
				    ((long long)(var->yres / buffer_num)) << 32;

				if (mgr->rot_sw && mgr->rot_sw->apply) {
					if (0 != mgr->rot_sw->apply(mgr->rot_sw, &config, dirty_rect)) {
						__inf("rot sw need not apply, then return\n");
						return 0;
					}
					need_wait_vsync = 0;
				}

				print_layer_config_info("set_layer_config: ", &config);
				if (mgr->set_layer_config(mgr, &config, 1)
				    != 0) {
					__wrn
					    ("fb%d,set_lyr_cfg(%d,%d,%d)fail\n",
					     info->node, sel, chan, layer_id);
					return -1;
				}
			}
		}
	}

	if (1 == need_wait_vsync)
		fb_wait_for_vsync(info);
	return 0;
}

static int sunxi_fb_check_var(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct disp_fb_info disp_fb;
	struct fb_fix_screeninfo fix;

	if (var_to_disp_fb(&disp_fb, var, &fix) != 0) {
		switch (var->bits_per_pixel) {
		case 8:
		case 4:
		case 2:
		case 1:
			disp_fb_to_var(DISP_FORMAT_ARGB_8888, var);
			break;

		case 19:
		case 18:
		case 16:
			disp_fb_to_var(DISP_FORMAT_RGB_565, var);
			break;

		case 32:
		case 28:
		case 25:
		case 24:
			disp_fb_to_var(DISP_FORMAT_ARGB_8888, var);
			break;

		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int sunxi_fb_set_par(struct fb_info *info)
{
	u32 sel = 0;
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("sunxi_fb_set_par\n");

	for (sel = 0; sel < num_screens; sel++) {
		if (sel == g_fbi.fb_mode[info->node]) {
			struct fb_var_screeninfo *var = &info->var;
			struct fb_fix_screeninfo *fix = &info->fix;
			u32 buffer_num = 1;
			u32 y_offset = 0;
			s32 chan = g_fbi.layer_hdl[info->node][0];
			s32 layer_id = g_fbi.layer_hdl[info->node][1];
			struct disp_layer_config config;
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			if (mgr && mgr->get_layer_config
			    && mgr->set_layer_config) {
				config.channel = chan;
				config.layer_id = layer_id;
				mgr->get_layer_config(mgr, &config, 1);
			}

			var_to_disp_fb(&(config.info.fb), var, fix);
			config.info.fb.crop.x = var->xoffset;
			config.info.fb.crop.y = var->yoffset + y_offset;
			config.info.fb.crop.width = var->xres;
			config.info.fb.crop.height = var->yres / buffer_num;
			if (mgr && mgr->get_layer_config
			    && mgr->set_layer_config)
				mgr->set_layer_config(mgr, &config, 1);
		}
	}
	return 0;
}

static int sunxi_fb_blank(int blank_mode, struct fb_info *info)
{
	u32 sel = 0;
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("sunxi_fb_blank,mode:%d\n", blank_mode);

	for (sel = 0; sel < num_screens; sel++) {
		if (sel == g_fbi.fb_mode[info->node]) {
			s32 chan = g_fbi.layer_hdl[info->node][0];
			s32 layer_id = g_fbi.layer_hdl[info->node][1];
			struct disp_layer_config config;
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			if (blank_mode == FB_BLANK_POWERDOWN) {
				if (mgr && mgr->get_layer_config
				    && mgr->set_layer_config) {
					config.channel = chan;
					config.layer_id = layer_id;
					mgr->get_layer_config(mgr, &config, 1);
					config.enable = 0;
					mgr->set_layer_config(mgr, &config, 1);
				}
			} else {
				if (mgr && mgr->get_layer_config
				    && mgr->set_layer_config) {
					config.channel = chan;
					config.layer_id = layer_id;
					mgr->get_layer_config(mgr, &config, 1);
					config.enable = 1;
					mgr->set_layer_config(mgr, &config, 1);
				}
			}
		}
	}
	return 0;
}

static int sunxi_fb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	__inf("sunxi_fb_cursor\n");

	return 0;
}

static int sunxi_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long mmio_pgoff;
	unsigned long start;
	u32 len;

	start = info->fix.smem_start;
	if (start == 0)
		return -EINVAL;
	len = info->fix.smem_len;
	mmio_pgoff = PAGE_ALIGN((start & ~PAGE_MASK) + len) >> PAGE_SHIFT;

	if (vma->vm_pgoff >= mmio_pgoff) {
		if (info->var.accel_flags)
			return -EINVAL;
		vma->vm_pgoff -= mmio_pgoff;
		start = info->fix.mmio_start;
		len = info->fix.mmio_len;
	}
	vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return vm_iomap_memory(vma, start, len);
}

/**
 * drv_disp_vsync_event - wakeup vsync thread
 * @sel: the index of display manager
 *
 * Get the current time, push it into the cirular queue,
 * and then wakeup the vsync thread.
 */
s32 drv_disp_vsync_event(u32 sel)
{
	unsigned long flags;
	ktime_t now;
	unsigned int head, tail, next;
	bool full = false;

	now = ktime_get();
	spin_lock_irqsave(&g_fbi.slock[sel], flags);
	head = g_fbi.vsync_timestamp_head[sel];
	tail = g_fbi.vsync_timestamp_tail[sel];
	next = tail + 1;
	next = (next >= VSYNC_NUM) ? 0 : next;
	if (next == head)
		full = true;

	if (!full) {
		g_fbi.vsync_timestamp[sel][tail] = now;
		g_fbi.vsync_timestamp_tail[sel] = next;
	}
	spin_unlock_irqrestore(&g_fbi.slock[sel], flags);

	if (g_fbi.vsync_task[sel])
		wake_up_process(g_fbi.vsync_task[sel]);

	if (full)
		pr_warn("%s(): disp %d's vsync timestamp list is full!\n",
			__func__, sel);

	return 0;
}

/**
 * vsync_proc - sends vsync message
 * @disp: the index of display manager
 *
 * Get the timestamp from the circular queue,
 * And send it widthin vsync message to the userland.
 */
static int vsync_proc(u32 disp)
{
	char buf[64];
	char *envp[2];
	unsigned long flags;
	unsigned int head, tail, next;
	ktime_t time;

	spin_lock_irqsave(&g_fbi.slock[disp], flags);
	head = g_fbi.vsync_timestamp_head[disp];
	tail = g_fbi.vsync_timestamp_tail[disp];
	while (head != tail) {
		time = g_fbi.vsync_timestamp[disp][head];
		next = head + 1;
		next = (next >= VSYNC_NUM) ? 0 : next;
		g_fbi.vsync_timestamp_head[disp] = next;
		spin_unlock_irqrestore(&g_fbi.slock[disp], flags);

		snprintf(buf, sizeof(buf), "VSYNC%d=%llu", disp,
			 ktime_to_ns(time));
		envp[0] = buf;
		envp[1] = NULL;
		kobject_uevent_env(&g_fbi.dev->kobj, KOBJ_CHANGE, envp);

		spin_lock_irqsave(&g_fbi.slock[disp], flags);
		head = g_fbi.vsync_timestamp_head[disp];
		tail = g_fbi.vsync_timestamp_tail[disp];
	}

	spin_unlock_irqrestore(&g_fbi.slock[disp], flags);

	return 0;
}

static int vsync_thread(void *parg)
{
	unsigned long disp = (unsigned long)parg;

	while (1) {

		vsync_proc(disp);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		if (kthread_should_stop())
			break;
		set_current_state(TASK_RUNNING);
	}

	return 0;
}

void DRV_disp_int_process(u32 sel)
{
	g_fbi.wait_count[sel]++;
	wake_up_interruptible(&g_fbi.wait[sel]);
}

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	u32 mask = ((1 << bf->length) - 1) << bf->offset;

	return (val << bf->offset) & mask;
}

static int sunxi_fb_setcolreg(unsigned regno, unsigned red, unsigned green,
			      unsigned blue, unsigned transp,
			      struct fb_info *info)
{
	u32 val;
	u32 ret = 0;

	switch (info->fix.visual) {
	case FB_VISUAL_PSEUDOCOLOR:
		ret = -EINVAL;
		break;
	case FB_VISUAL_TRUECOLOR:
		if (regno < 16) {
			val = convert_bitfield(transp, &info->var.transp) |
			    convert_bitfield(red, &info->var.red) |
			    convert_bitfield(green, &info->var.green) |
			    convert_bitfield(blue, &info->var.blue);
			__inf("regno=%2d,a=%2X,r=%2X,g=%2X,b=%2X,result=%08X\n",
			      regno, transp, red, green, blue,
			      val);
			((u32 *) info->pseudo_palette)[regno] = val;
		} else {
			ret = 0;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sunxi_fb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	unsigned int j, r = 0;
	unsigned char hred, hgreen, hblue, htransp = 0xff;
	unsigned short *red, *green, *blue, *transp;

	__inf("Fb_setcmap, cmap start:%d len:%d, %dbpp\n", cmap->start,
	      cmap->len, info->var.bits_per_pixel);

	red = cmap->red;
	green = cmap->green;
	blue = cmap->blue;
	transp = cmap->transp;

	for (j = 0; j < cmap->len; j++) {
		hred = *red++;
		hgreen = *green++;
		hblue = *blue++;
		if (transp)
			htransp = (*transp++) & 0xff;
		else
			htransp = 0xff;

		r = sunxi_fb_setcolreg(cmap->start + j, hred, hgreen, hblue,
				       htransp, info);
		if (r)
			return r;
	}

	return 0;
}

static int sunxi_fb_ioctl(struct fb_info *info, unsigned int cmd,
			  unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
#if 0
	case FBIOGET_VBLANK:
		{
			struct fb_vblank vblank;
			struct disp_video_timings tt;
			u32 line = 0;
			u32 sel;

			sel =
			    (g_fbi.fb_mode[info->node] ==
			     FB_MODE_SCREEN1) ? 1 : 0;
			line = bsp_disp_get_cur_line(sel);
			bsp_disp_get_timming(sel, &tt);

			memset(&vblank, 0, sizeof(struct fb_vblank));
			vblank.flags |= FB_VBLANK_HAVE_VBLANK;
			vblank.flags |= FB_VBLANK_HAVE_VSYNC;
			if (line <= (tt.ver_total_time - tt.y_res))
				vblank.flags |= FB_VBLANK_VBLANKING;
			if ((line > tt.ver_front_porch)
			    && (line <
				(tt.ver_front_porch + tt.ver_sync_time))) {
				vblank.flags |= FB_VBLANK_VSYNCING;
			}

			if (copy_to_user
			    ((void __user *)arg, &vblank,
			     sizeof(struct fb_vblank)))
				ret = -EFAULT;

			break;
		}
#endif

	case FBIO_WAITFORVSYNC:
		{
			/* ret = fb_wait_for_vsync(info); */
			break;
		}

	case FBIO_FREE:
		{
			int fb_id = 0;	/* fb0 */
			struct fb_info *fbinfo = g_fbi.fbinfo[fb_id];

			if ((!g_fbi.fb_enable[fb_id])
			    || (fbinfo != info)) {
				__wrn("%s, fb%d already release ? or fb_info mismatch: fbinfo=0x%p, info=0x%p\n",
				      __func__, fb_id, fbinfo, info);
				return -1;
			}

			__inf("### FBIO_FREE ###\n");

			fb_unmap_video_memory(fbinfo);

			/* unbound fb0 from layer(1,0)  */
			g_fbi.layer_hdl[fb_id][0] = 0;
			g_fbi.layer_hdl[fb_id][1] = 0;
			g_fbi.fb_mode[fb_id] = 0;
			g_fbi.fb_enable[fb_id] = 0;
			break;
		}

	case FBIO_ALLOC:
		{
			int fb_id = 0;	/* fb0 */
			struct fb_info *fbinfo = g_fbi.fbinfo[fb_id];

			if (g_fbi.fb_enable[fb_id]
			    || (fbinfo != info)) {
				__wrn("%s, fb%d already enable ? or fb_info mismatch: fbinfo=0x%p, info=0x%p\n",
				      __func__, fb_id, fbinfo, info);
				return -1;
			}

			__inf("### FBIO_ALLOC ###\n");

			/* fb0 bound to layer(1,0)  */
			g_fbi.layer_hdl[fb_id][0] = 1;
			g_fbi.layer_hdl[fb_id][1] = 0;

			fb_map_video_memory(fbinfo);

			g_fbi.fb_mode[fb_id] = g_fbi.fb_para[fb_id].fb_mode;
			g_fbi.fb_enable[fb_id] = 1;
			break;
		}

	default:
		break;
	}
	return ret;
}

static struct fb_ops dispfb_ops = {
	.owner = THIS_MODULE,
	.fb_open = sunxi_fb_open,
	.fb_release = sunxi_fb_release,
	.fb_pan_display = sunxi_fb_pan_display,
	.fb_ioctl = sunxi_fb_ioctl,
	.fb_check_var = sunxi_fb_check_var,
	.fb_set_par = sunxi_fb_set_par,
	.fb_blank = sunxi_fb_blank,
	.fb_cursor = sunxi_fb_cursor,
	.fb_mmap = sunxi_fb_mmap,
#if defined(CONFIG_FB_CONSOLE_SUNXI)
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
#endif
	.fb_setcmap = sunxi_fb_setcmap,
	.fb_setcolreg = sunxi_fb_setcolreg,

};

static int Fb_copy_boot_fb(u32 sel, struct fb_info *info)
{
	enum {
		BOOT_FB_ADDR = 0,
		BOOT_FB_WIDTH,
		BOOT_FB_HEIGHT,
		BOOT_FB_BPP,
		BOOT_FB_STRIDE,
		BOOT_FB_CROP_L,
		BOOT_FB_CROP_T,
		BOOT_FB_CROP_R,
		BOOT_FB_CROP_B,
	};

	char *boot_fb_str = NULL;
	char *src_phy_addr = NULL;
	char *src_addr = NULL;
	char *src_addr_b = NULL;
	char *src_addr_e = NULL;
	int src_width = 0;
	int src_height = 0;
	int src_bpp = 0;
	int src_stride = 0;
	int src_cp_btyes = 0;
	int src_crop_l = 0;
	int src_crop_t = 0;
	int src_crop_r = 0;
	int src_crop_b = 0;

	char *dst_addr = NULL;
	int dst_width = 0;
	int dst_height = 0;
	int dst_bpp = 0;
	int dst_stride = 0;
	int ret;

	unsigned long map_offset;

	if (info == NULL) {
		__wrn("%s,%d: null pointer\n", __func__, __LINE__);
		return -1;
	}

	boot_fb_str = (char *)disp_boot_para_parse_str("boot_fb0");
	if (boot_fb_str != NULL) {
		int i = 0;
		char boot_fb[128] = { 0 };
		int len = strlen(boot_fb_str);

		if (sizeof(boot_fb) - 1 < len) {
			__wrn("need bigger array size[%d] for boot_fb\n", len);
			return -1;
		}
		memcpy((void *)boot_fb, (void *)boot_fb_str, len);
		boot_fb[len] = '\0';
		boot_fb_str = boot_fb;
		for (i = 0;; ++i) {
			char *p = strstr(boot_fb_str, ",");

			if (p != NULL)
				*p = '\0';
			if (i == BOOT_FB_ADDR) {
				ret = kstrtoul(boot_fb_str, 16,
				    (unsigned long *)&src_phy_addr);
				if (ret)
					pr_warn("parse src_phy_addr fail!\n");
			} else if (i == BOOT_FB_WIDTH) {
				ret = kstrtou32(boot_fb_str, 16, &src_width);
				if (ret)
					pr_warn("parse src_width fail!\n");
			} else if (i == BOOT_FB_HEIGHT) {
				ret = kstrtou32(boot_fb_str, 16, &src_height);
				if (ret)
					pr_warn("parse src_height fail!\n");
			} else if (i == BOOT_FB_BPP) {
				ret = kstrtou32(boot_fb_str, 16, &src_bpp);
				if (ret)
					pr_warn("parse src_bpp fail!\n");
			} else if (i == BOOT_FB_STRIDE) {
				ret = kstrtou32(boot_fb_str, 16, &src_stride);
				if (ret)
					pr_warn("parse src_stride fail!\n");
			} else if (i == BOOT_FB_CROP_L) {
				ret = kstrtou32(boot_fb_str, 16, &src_crop_l);
				if (ret)
					pr_warn("parse src_crop_l fail!\n");
			} else if (i == BOOT_FB_CROP_T) {
				ret = kstrtou32(boot_fb_str, 16, &src_crop_t);
				if (ret)
					pr_warn("parse src_crop_t fail!\n");
			} else if (i == BOOT_FB_CROP_R) {
				ret = kstrtou32(boot_fb_str, 16, &src_crop_r);
				if (ret)
					pr_warn("parse src_crop_r fail!\n");
			} else if (i == BOOT_FB_CROP_B) {
				ret = kstrtou32(boot_fb_str, 16, &src_crop_b);
				if (ret)
					pr_warn("parse src_crop_b fail!\n");
			} else {
				break;
			}

			if (p == NULL)
				break;
			boot_fb_str = p + 1;
		}
	} else {
		__wrn("no boot_fb0\n");
		return -1;
	}

	dst_addr = (char *)(info->screen_base);
	dst_width = info->var.xres;
	dst_height = info->var.yres;
	dst_bpp = info->var.bits_per_pixel;
	dst_stride = info->fix.line_length;

	if ((src_phy_addr == NULL)
	    || (src_width <= 0)
	    || (src_height <= 0)
	    || (src_stride <= 0)
	    || (src_bpp <= 0)
	    || (dst_addr == NULL)
	    || (dst_width <= 0)
	    || (dst_height <= 0)
	    || (dst_stride <= 0)
	    || (dst_bpp <= 0)
	    || (src_bpp != dst_bpp)) {
		__wrn
		    ("wrong para: src[phy_addr=%p,w=%d,h=%d,bpp=%d,stride=%d], dst[addr=%p,w=%d,h=%d,bpp=%d,stride=%d]\n",
		     src_phy_addr,
		     src_width, src_height, src_bpp, src_stride, dst_addr,
		     dst_width, dst_height, dst_bpp, dst_stride);
		return -1;
	}

	map_offset = (unsigned long)src_phy_addr + PAGE_SIZE
	    - PAGE_ALIGN((unsigned long)src_phy_addr + 1);
	src_addr = (char *)Fb_map_kernel_cache((unsigned long)src_phy_addr -
					       map_offset,
					       src_stride * src_height +
					       map_offset);
	if (src_addr == NULL) {
		__wrn("Fb_map_kernel_cache for src_addr failed\n");
		return -1;
	}

	src_addr_b = src_addr + map_offset;
	if ((src_crop_b > src_crop_t) &&
	    (src_height > src_crop_b - src_crop_t) &&
	    (src_crop_t >= 0) &&
	    (src_height >= src_crop_b)) {
		src_height = src_crop_b - src_crop_t;
		src_addr_b += (src_stride * src_crop_t);
	}
	if ((src_crop_r > src_crop_l)
	    && (src_width > src_crop_r - src_crop_l)
	    && (src_crop_l >= 0)
	    && (src_width >= src_crop_r)) {
		src_width = src_crop_r - src_crop_l;
		src_addr_b += (src_crop_l * src_bpp >> 3);
	}
	if (src_height < dst_height) {
		int dst_crop_t = (dst_height - src_height) >> 1;

		dst_addr += (dst_stride * dst_crop_t);
	} else if (src_height > dst_height) {
		__wrn("src_height(%d) > dst_height(%d),please cut the height\n",
		      src_height,
		      dst_height);
		Fb_unmap_kernel(src_addr);
		return -1;
	}
	if (src_width < dst_width) {
		int dst_crop_l = (dst_width - src_width) >> 1;

		dst_addr += (dst_crop_l * dst_bpp >> 3);
	} else if (src_width > dst_width) {
		__wrn("src_width(%d) > dst_width(%d),please cut the width!\n",
		      src_width,
		      dst_width);
		Fb_unmap_kernel(src_addr);
		return -1;
	}

	src_cp_btyes = src_width * src_bpp >> 3;
	src_addr_e = src_addr_b + src_stride * src_height;
	for (; src_addr_b != src_addr_e; src_addr_b += src_stride) {
		memcpy((void *)dst_addr, (void *)src_addr_b, src_cp_btyes);
		dst_addr += dst_stride;
	}

	Fb_unmap_kernel(src_addr);

	return 0;
}

static int Fb_map_kernel_logo(u32 sel, struct fb_info *info)
{
	void *vaddr = NULL;
	uintptr_t paddr = 0;
	void *screen_offset = NULL, *image_offset = NULL;
	char *tmp_buffer = NULL;
	char *bmp_data = NULL;
	struct sunxi_bmp_store s_bmp_info;
	struct sunxi_bmp_store *bmp_info = &s_bmp_info;
	struct bmp_pad_header bmp_pad_header;
	struct bmp_header *bmp_header;
	int zero_num = 0;
	unsigned int x, y, bmp_bpix, fb_width, fb_height;
	unsigned int effective_width, effective_height;
	uintptr_t offset;
	int i = 0;

	paddr = bootlogo_addr;
	if (paddr == 0) {
		__inf("Fb_map_kernel_logo failed!");
		return Fb_copy_boot_fb(sel, info);
	}

	/* parser bmp header */
	offset = paddr & ~PAGE_MASK;
	vaddr = (void *)Fb_map_kernel(paddr, sizeof(struct bmp_header));
	if (vaddr == NULL) {
		__wrn("fb_map_kernel failed, paddr=0x%p,size=0x%x\n",
		      (void *)paddr, (unsigned int)sizeof(struct bmp_header));
		return -1;
	}

	memcpy(&bmp_pad_header.signature[0], vaddr + offset,
	       sizeof(struct bmp_header));
	bmp_header = (struct bmp_header *) &bmp_pad_header.signature[0];
	if ((bmp_header->signature[0] != 'B')
	    || (bmp_header->signature[1] != 'M')) {
		__wrn("this is not a bmp picture.\n");
		return -1;
	}

	bmp_bpix = bmp_header->bit_count / 8;

	if ((bmp_bpix != 3) && (bmp_bpix != 4))
		return -1;

	if (bmp_bpix == 3)
		zero_num = (4 - ((3 * bmp_header->width) % 4)) & 3;

	x = bmp_header->width;
	y = (bmp_header->height & 0x80000000) ? (-bmp_header->
						 height) : (bmp_header->height);
	fb_width = info->var.xres;
	fb_height = info->var.yres;
	if ((paddr <= 0) || x <= 1 || y <= 1) {
		__wrn("kernel logo para error!\n");
		return -EINVAL;
	}

	bmp_info->x = x;
	bmp_info->y = y;
	bmp_info->bit = bmp_header->bit_count;
	bmp_info->buffer = (void *__force)(info->screen_base);

	if (bmp_bpix == 3)
		info->var.bits_per_pixel = 24;
	else if (bmp_bpix == 4)
		info->var.bits_per_pixel = 32;
	else
		info->var.bits_per_pixel = 32;

	Fb_unmap_kernel(vaddr);

	/* map the total bmp buffer */
	vaddr =
	    (void *)Fb_map_kernel(paddr,
				  x * y * bmp_bpix + sizeof(struct bmp_header));
	if (vaddr == NULL) {
		__wrn("fb_map_kernel failed, paddr=0x%p,size=0x%x\n",
		      (void *)paddr,
		      (unsigned int)(x * y * bmp_bpix +
		       sizeof(struct bmp_header)));
		return -1;
	}

	tmp_buffer = (char *)bmp_info->buffer;
	screen_offset = (void *)bmp_info->buffer;
	bmp_data = (char *)(vaddr + bmp_header->data_offset);
	image_offset = (void *)bmp_data;
	effective_width = (fb_width < x) ? fb_width : x;
	effective_height = (fb_height < y) ? fb_height : y;

	if (bmp_header->height & 0x80000000) {

		screen_offset =
		    (void *)((void *__force)info->screen_base +
			     (fb_width * (abs(fb_height - y) / 2)
			      + abs(fb_width - x) / 2)
			      * (info->var.bits_per_pixel >> 3));

		for (i = 0; i < effective_height; i++) {
			memcpy((void *)screen_offset, image_offset,
			       effective_width *
			       (info->var.bits_per_pixel >> 3));
			screen_offset =
			    (void *)(screen_offset + fb_width *
				     (info->var.bits_per_pixel >> 3));
			image_offset =
			    (void *)image_offset +
			    x * (info->var.bits_per_pixel >> 3);
		}
	} else {

		screen_offset =
		    (void *)((void *__force)info->screen_base +
			     (fb_width * (abs(fb_height - y) / 2)
			      + abs(fb_width - x) / 2)
			      * (info->var.bits_per_pixel >> 3));
		image_offset =
		    (void *)(image_offset + (x * (abs(y - fb_height) / 2)
					     + abs(x - fb_width) / 2) *
			     (info->var.bits_per_pixel >> 3));

		image_offset =
		    (void *)bmp_data + (effective_height -
					1) * x *
		    (info->var.bits_per_pixel >> 3);
		for (i = effective_height - 1; i >= 0; i--) {
			memcpy((void *)screen_offset, image_offset,
			       effective_width *
			       (info->var.bits_per_pixel >> 3));
			screen_offset =
			    (void *)(screen_offset +
				     fb_width *
				     (info->var.bits_per_pixel >> 3));
			image_offset =
			    (void *)bmp_data +
			    i * x * (info->var.bits_per_pixel >> 3);
		}
	}

	Fb_unmap_kernel(vaddr);
	return 0;
}

static s32 display_fb_request(u32 fb_id, struct disp_fb_create_info *fb_para)
{
	struct fb_info *info = NULL;
	struct disp_layer_config config;
	u32 sel;
	u32 xres, yres;
	u32 num_screens;

#if defined(CONFIG_ARCH_SUN8IW12)
	/* fb bound to layer(2,0)  */
	g_fbi.layer_hdl[fb_id][0] = 2;
	g_fbi.layer_hdl[fb_id][1] = 0;
#else
	/* fb bound to layer(1,0)  */
	g_fbi.layer_hdl[fb_id][0] = 1;
	g_fbi.layer_hdl[fb_id][1] = 0;
#endif


	num_screens = bsp_disp_feat_get_num_screens();

	__inf("%s,fb_id:%d\n", __func__, fb_id);

	if (g_fbi.fb_enable[fb_id]) {
		__wrn("%s, fb%d is already requested!\n", __func__, fb_id);
		return -1;
	}
	info = g_fbi.fbinfo[fb_id];

	xres = fb_para->width;
	yres = fb_para->height;
	if ((xres == 0) || (yres == 0) || (info->var.bits_per_pixel == 0)) {
		__wrn("invalid paras xres(%d), yres(%d) bpp(%d)\n", xres, yres,
		      info->var.bits_per_pixel);
		return -1;
	}

	info->var.xoffset = 0;
	info->var.yoffset = 0;
	info->var.xres = xres;
	info->var.yres = yres;
	info->var.xres_virtual = xres;
	info->fix.line_length =
	    (fb_para->width * info->var.bits_per_pixel) >> 3;
	info->fix.smem_len =
	    info->fix.line_length * fb_para->height * fb_para->buffer_num;
	if (info->fix.line_length != 0)
		info->var.yres_virtual =
		    info->fix.smem_len / info->fix.line_length;
	fb_map_video_memory(info);

	for (sel = 0; sel < num_screens; sel++) {
		if (sel == fb_para->fb_mode) {
			u32 y_offset = 0, src_width = xres, src_height = yres;
			struct disp_video_timings tt;
			struct disp_manager *mgr = NULL;

			mgr = g_disp_drv.mgr[sel];
			if (mgr && mgr->device && mgr->device->get_timings) {
				mgr->device->get_timings(mgr->device, &tt);
				if (tt.pixel_clk != 0)
					g_fbi.fbinfo[fb_id]->var.pixclock =
					    1000000000 / tt.pixel_clk;
				g_fbi.fbinfo[fb_id]->var.left_margin =
				    tt.hor_back_porch;
				g_fbi.fbinfo[fb_id]->var.right_margin =
				    tt.hor_front_porch;
				g_fbi.fbinfo[fb_id]->var.upper_margin =
				    tt.ver_back_porch;
				g_fbi.fbinfo[fb_id]->var.lower_margin =
				    tt.ver_front_porch;
				g_fbi.fbinfo[fb_id]->var.hsync_len =
				    tt.hor_sync_time;
				g_fbi.fbinfo[fb_id]->var.vsync_len =
				    tt.ver_sync_time;
			}
			info->var.width =
			    bsp_disp_get_screen_physical_width(sel);
			info->var.height =
			    bsp_disp_get_screen_physical_height(sel);

			memset(&config, 0, sizeof(struct disp_layer_config));

			config.channel = g_fbi.layer_hdl[fb_id][0];
			config.layer_id = g_fbi.layer_hdl[fb_id][1];
			config.enable = 1;
			Fb_map_kernel_logo(sel, info);
			if (g_disp_drv.para.boot_info.sync == 1) {
				if ((sel == g_disp_drv.para.boot_info.disp) &&
				    (g_disp_drv.para.boot_info.type !=
				     DISP_OUTPUT_TYPE_NONE)) {
					bsp_disp_get_display_size(sel,
					      &fb_para->output_width,
					      &fb_para->output_height);
				}
			}

#if defined(CONFIG_DISP2_SUNXI_BOOT_COLORBAR)
			fb_draw_colorbar((char *__force)g_fbi.fbinfo[0]->screen_base,
				fb_para->width, fb_para->height * fb_para->buffer_num, &(g_fbi.fbinfo[0]->var));
#endif

			if (mgr && mgr->rot_sw &&
				(ROTATION_SW_90 == mgr->rot_sw->degree ||
				ROTATION_SW_270 == mgr->rot_sw->degree)) {
					config.info.screen_win.height = (0 == fb_para->output_width) ? src_width:fb_para->output_width;
					config.info.screen_win.width = (0 == fb_para->output_height) ? src_width:fb_para->output_height;
					printk("rot_degree =%d, w=%d, h=%d\n", mgr->rot_sw->degree,
					config.info.screen_win.width, config.info.screen_win.height);
			} else {
				config.info.screen_win.width = (0 == fb_para->output_width) ? src_width:fb_para->output_width;
				config.info.screen_win.height = (0 == fb_para->output_height) ? src_width:fb_para->output_height;
			}

			config.info.mode = LAYER_MODE_BUFFER;
			config.info.alpha_mode = 1;
			config.info.alpha_value = 0xff;
			config.info.fb.crop.x = ((long long)0) << 32;
			config.info.fb.crop.y = ((long long)y_offset) << 32;
			config.info.fb.crop.width =
			    ((long long)src_width) << 32;
			config.info.fb.crop.height =
			    ((long long)src_height) << 32;
			config.info.screen_win.x = 0;
			config.info.screen_win.y = 0;
			var_to_disp_fb(&(config.info.fb), &(info->var),
				       &(info->fix));
			config.info.fb.addr[0] =
			    (unsigned long long)info->fix.smem_start;
			config.info.fb.addr[1] = 0;
			config.info.fb.addr[2] = 0;
			config.info.fb.flags = DISP_BF_NORMAL;
			config.info.fb.scan = DISP_SCAN_PROGRESSIVE;
			config.info.fb.size[0].width = fb_para->width;
			config.info.fb.size[0].height = fb_para->height;
			config.info.fb.size[1].width = fb_para->width;
			config.info.fb.size[1].height = fb_para->height;
			config.info.fb.size[2].width = fb_para->width;
			config.info.fb.size[2].height = fb_para->height;
			config.info.fb.color_space = DISP_BT601;

			if (mgr && mgr->rot_sw && mgr->rot_sw->apply) {
				struct disp_rect dirty_rect;
				dirty_rect.x = config.info.fb.crop.x >> 32;
				dirty_rect.y = config.info.fb.crop.y >> 32;
				dirty_rect.width = config.info.fb.crop.width >> 32;
				dirty_rect.height = config.info.fb.crop.height >> 32;

				mgr->rot_sw->apply(mgr->rot_sw, &config, dirty_rect);
			}

#if !defined(CONFIG_EINK_PANEL_USED)
			if (mgr && mgr->set_layer_config) {
				mgr->set_layer_config(mgr, &config, 1);
			}
#endif
		}
	}

	g_fbi.fb_enable[fb_id] = 1;
	g_fbi.fb_mode[fb_id] = fb_para->fb_mode;
	memcpy(&g_fbi.fb_para[fb_id], fb_para,
	       sizeof(struct disp_fb_create_info));

	return 0;
}

static s32 display_fb_release(u32 fb_id)
{
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("%s, fb_id:%d\n", __func__, fb_id);

	if (g_fbi.fb_enable[fb_id]) {
		u32 sel = 0;
		struct fb_info *info = g_fbi.fbinfo[fb_id];

		for (sel = 0; sel < num_screens; sel++) {
			if (sel == g_fbi.fb_mode[fb_id]) {
				struct disp_manager *mgr = NULL;
				struct disp_layer_config config;

				mgr = g_disp_drv.mgr[sel];
				memset(&config, 0,
				       sizeof(struct disp_layer_config));
				config.channel = g_fbi.layer_hdl[fb_id][0];
				config.layer_id = g_fbi.layer_hdl[fb_id][1];
				if (mgr && mgr->set_layer_config)
					mgr->set_layer_config(mgr, &config, 1);
			}
		}
		g_fbi.layer_hdl[fb_id][0] = 0;
		g_fbi.layer_hdl[fb_id][1] = 0;
		g_fbi.fb_mode[fb_id] = FB_MODE_SCREEN0;
		memset(&g_fbi.fb_para[fb_id], 0,
		       sizeof(struct disp_fb_create_info));
		g_fbi.fb_enable[fb_id] = 0;
#if defined(CONFIG_FB_CONSOLE_SUNXI)
		fb_dealloc_cmap(&info->cmap);
#endif
		fb_unmap_video_memory(info);

		return 0;
	}

	__wrn("invalid paras fb_id:%d in %s\n", fb_id, __func__);
	return -1;
}

s32 Display_set_fb_timming(u32 sel)
{
	u8 fb_id = 0;

	for (fb_id = 0; fb_id < FB_MAX; fb_id++) {
		if (g_fbi.fb_enable[fb_id]) {
			if (sel == g_fbi.fb_mode[fb_id]) {
				struct disp_video_timings tt;
				struct disp_manager *mgr = g_disp_drv.mgr[sel];

				if (mgr && mgr->device
				    && mgr->device->get_timings) {
					mgr->device->get_timings(mgr->device,
								 &tt);
					if (tt.pixel_clk != 0)
						g_fbi.fbinfo[fb_id]->var.
						    pixclock =
						    1000000000 / tt.pixel_clk;
					g_fbi.fbinfo[fb_id]->var.left_margin =
					    tt.hor_back_porch;
					g_fbi.fbinfo[fb_id]->var.right_margin =
					    tt.hor_front_porch;
					g_fbi.fbinfo[fb_id]->var.upper_margin =
					    tt.ver_back_porch;
					g_fbi.fbinfo[fb_id]->var.lower_margin =
					    tt.ver_front_porch;
					g_fbi.fbinfo[fb_id]->var.hsync_len =
					    tt.hor_sync_time;
					g_fbi.fbinfo[fb_id]->var.vsync_len =
					    tt.ver_sync_time;
				}
			}
		}
	}

	return 0;
}

static s32 fb_parse_bootlogo_base(phys_addr_t *fb_base, int *fb_size)
{
	*fb_base = (phys_addr_t) disp_boot_para_parse("fb_base");

	return 0;
}

unsigned long fb_get_address_info(u32 fb_id, u32 phy_virt_flag)
{
	struct fb_info *info = NULL;
	unsigned long phy_addr = 0;
	unsigned long virt_addr = 0;

	if (fb_id >= FB_MAX)
		return 0;

	info = g_fbi.fbinfo[fb_id];
	phy_addr = info->fix.smem_start;
	virt_addr = (unsigned long)info->screen_base;

	if (phy_virt_flag == 0)
		/* get virtual address */
		return virt_addr;

	/* get phy address */
	return phy_addr;
}

s32 fb_init(struct platform_device *pdev)
{
	struct disp_fb_create_info fb_para;
	unsigned long i;
	u32 num_screens;
	/* struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 }; */

	g_fbi.dev = &pdev->dev;
	num_screens = bsp_disp_feat_get_num_screens();

	__inf("[DISP] %s\n", __func__);

	fb_parse_bootlogo_base(&bootlogo_addr, &bootlogo_sz);

	for (i = 0; i < num_screens; i++) {
		char task_name[25];

		spin_lock_init(&g_fbi.slock[i]);
		sprintf(task_name, "vsync proc %ld", i);
		g_fbi.vsync_task[i] =
		    kthread_create(vsync_thread, (void *)i, task_name);
		if (IS_ERR(g_fbi.vsync_task[i])) {
			s32 err = 0;

			__wrn("Unable to start kernel thread %s.\n",
			      "hdmi proc");
			err = PTR_ERR(g_fbi.vsync_task[i]);
			g_fbi.vsync_task[i] = NULL;
		} else {
			wake_up_process(g_fbi.vsync_task[i]);
		}
	}
	init_waitqueue_head(&g_fbi.wait[0]);
	init_waitqueue_head(&g_fbi.wait[1]);
	init_waitqueue_head(&g_fbi.wait[2]);
	disp_register_sync_finish_proc(DRV_disp_int_process);

	for (i = 0; i < 8; i++) {
		g_fbi.fbinfo[i] = framebuffer_alloc(0, g_fbi.dev);
		g_fbi.fbinfo[i]->fbops = &dispfb_ops;
		g_fbi.fbinfo[i]->flags = 0;
		g_fbi.fbinfo[i]->device = g_fbi.dev;
		g_fbi.fbinfo[i]->par = &g_fbi;
		g_fbi.fbinfo[i]->var.xoffset = 0;
		g_fbi.fbinfo[i]->var.yoffset = 0;
		g_fbi.fbinfo[i]->var.xres = 800;
		g_fbi.fbinfo[i]->var.yres = 480;
		g_fbi.fbinfo[i]->var.xres_virtual = 800;
		g_fbi.fbinfo[i]->var.yres_virtual = 480 * 2;
		g_fbi.fbinfo[i]->var.nonstd = 0;
		g_fbi.fbinfo[i]->var.bits_per_pixel = 32;
		g_fbi.fbinfo[i]->var.transp.length = 8;
		g_fbi.fbinfo[i]->var.red.length = 8;
		g_fbi.fbinfo[i]->var.green.length = 8;
		g_fbi.fbinfo[i]->var.blue.length = 8;
		g_fbi.fbinfo[i]->var.transp.offset = 24;
		g_fbi.fbinfo[i]->var.red.offset = 16;
		g_fbi.fbinfo[i]->var.green.offset = 8;
		g_fbi.fbinfo[i]->var.blue.offset = 0;
		g_fbi.fbinfo[i]->var.activate = FB_ACTIVATE_FORCE;
		g_fbi.fbinfo[i]->fix.type = FB_TYPE_PACKED_PIXELS;
		g_fbi.fbinfo[i]->fix.type_aux = 0;
		g_fbi.fbinfo[i]->fix.visual = FB_VISUAL_TRUECOLOR;
		g_fbi.fbinfo[i]->fix.xpanstep = 1;
		g_fbi.fbinfo[i]->fix.ypanstep = 1;
		g_fbi.fbinfo[i]->fix.ywrapstep = 0;
		g_fbi.fbinfo[i]->fix.accel = FB_ACCEL_NONE;
		g_fbi.fbinfo[i]->fix.line_length =
		    g_fbi.fbinfo[i]->var.xres_virtual * 4;
		g_fbi.fbinfo[i]->fix.smem_len =
		    g_fbi.fbinfo[i]->fix.line_length *
		    g_fbi.fbinfo[i]->var.yres_virtual * 2;
		g_fbi.fbinfo[i]->screen_base = NULL;
		g_fbi.fbinfo[i]->pseudo_palette = g_fbi.pseudo_palette[i];
		g_fbi.fbinfo[i]->fix.smem_start = 0x0;
		g_fbi.fbinfo[i]->fix.mmio_start = 0;
		g_fbi.fbinfo[i]->fix.mmio_len = 0;

		if (fb_alloc_cmap(&g_fbi.fbinfo[i]->cmap, 256, 1) < 0)
			return -ENOMEM;
	}

	if (g_disp_drv.disp_init.b_init) {
		u32 fb_num = 0;
		struct disp_init_para *disp_init = &g_disp_drv.disp_init;

		fb_num = 1;
		for (i = 0; i < fb_num; i++) {
			u32 screen_id = g_disp_drv.disp_init.disp_mode;

			if (g_disp_drv.para.boot_info.sync)
				screen_id = g_disp_drv.para.boot_info.disp;

			disp_fb_to_var(disp_init->format[i],
				       &(g_fbi.fbinfo[i]->var));
			fb_para.buffer_num = g_disp_drv.disp_init.buffer_num[i];
			if ((disp_init->fb_width[i] == 0)
			    || (disp_init->fb_height[i] == 0)) {
				fb_para.width =
				    bsp_disp_get_screen_width_from_output_type
				    (screen_id,
				     disp_init->output_type[screen_id],
				     disp_init->output_mode[screen_id]);
				fb_para.height =
				    bsp_disp_get_screen_height_from_output_type
				    (screen_id,
				     disp_init->output_type[screen_id],
				     disp_init->output_mode[screen_id]);
			} else {
				fb_para.width = disp_init->fb_width[i];
				fb_para.height = disp_init->fb_height[i];
			}

			fb_para.output_width =
			    bsp_disp_get_screen_width_from_output_type
			    (screen_id,
			     disp_init->output_type[screen_id],
			     disp_init->output_mode[screen_id]);
			fb_para.output_height =
			    bsp_disp_get_screen_height_from_output_type
			    (screen_id,
			     disp_init->output_type[screen_id],
			     disp_init->output_mode[screen_id]);
			fb_para.fb_mode = screen_id;

#if defined(SUPPORT_EINK) && defined(CONFIG_EINK_PANEL_USED)
			fb_para.output_width = fb_para.width;
			fb_para.output_height = fb_para.height;
#endif
			display_fb_request(i, &fb_para);
#if defined(CONFIG_DISP2_SUNXI_BOOT_COLORBAR)
#if 0
			fb_draw_colorbar((char *__force)g_fbi.fbinfo[i]->
					 screen_base, fb_para.width,
					 fb_para.height * fb_para.buffer_num,
					 &(g_fbi.fbinfo[i]->var));
#endif
#endif
		}
		for (i = 0; i < 8; i++)
			register_framebuffer(g_fbi.fbinfo[i]);
	}

	return 0;
}

s32 fb_exit(void)
{
	unsigned int fb_id = 0;
	unsigned int i;
	unsigned int num_screens;

	num_screens = bsp_disp_feat_get_num_screens();
	pr_warn("%s, num_screens=%d\n", __func__, num_screens);
	disp_unregister_sync_finish_proc(DRV_disp_int_process);
	for (i = 0; i < num_screens; i++) {
		if (g_fbi.vsync_task[i] && !IS_ERR(g_fbi.vsync_task[i])) {
			kthread_stop(g_fbi.vsync_task[i]);
			g_fbi.vsync_task[i] = NULL;
		}
	}

	for (fb_id = 0; fb_id < 8; fb_id++) {
		if (g_fbi.fbinfo[fb_id]) {
			fb_dealloc_cmap(&g_fbi.fbinfo[fb_id]->cmap);
			display_fb_release(fb_id);
			unregister_framebuffer(g_fbi.fbinfo[fb_id]);
			framebuffer_release(g_fbi.fbinfo[fb_id]);
			g_fbi.fbinfo[fb_id] = NULL;
		}
	}

	return 0;
}
