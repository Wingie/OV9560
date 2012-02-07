#include <linux/module.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <asm/io.h>
#include <linux/semaphore.h>
#include <linux/gpio.h>
#include <mach/regs-gpio.h>

#include <linux/videodev2.h>
#ifdef CONFIG_VIDEO_V4L1_COMPAT
#include <libv4l1-videodev.h> 
#endif

#include "sccb.h"
#include "s3c2440camif.h"

#define OV9650_SCCB_ADDR	  0x60
#define OV9650_MANUFACT_ID	0x7FA2
#define OV9650_PRODUCT_ID	  0x9650

struct ov9650_reg
{
	u8 subaddr;
	u8 value;
};

#include "ov9650defs.h"
//#include "ov9650_default.h"
#include "ov9650_sxga.h"
#include "ov9650_vga.h"
#include "ov9650_qvga.h"
#include "ov9650_qcif.h"
#include "ov9650_qqvga.h"

static struct ov9650_reg  ov9650_invert[] = {
{0x12, 0x80}};
//{0x3A, 0x88}};
static struct ov9650_reg  ov9650_restore[] = {
{0xFF, 0x10}};
//{0x3A, 0x8C}};

DEFINE_SEMAPHORE(regs_mutex);

enum ov9650_mode { OV9650_SXGA=0,OV9650_VGA, OV9650_QVGA,/*OV9650_QCIF,*/ OV9650_QQVGA};

struct OV9650_mode_settings
{
	enum ov9650_mode mode;
	int width;
	int height;
	int max_exposure; //maximum exposure in lines
	struct ov9650_reg* register_settings;
	int register_settings_len;
};

static struct OV9650_mode_settings ov9650_configurations[] =  
{
	{
		.mode=OV9650_SXGA,
		.width=1280,
		.height=1024,
		.max_exposure=1048,
		.register_settings=ov9650_sxga,//,
		.register_settings_len=ARRAY_SIZE(ov9650_sxga)
	},
	{
		.mode=OV9650_VGA,
		.width=640,
		.height=480,
		.max_exposure=498,
		.register_settings=ov9650_vga,
		.register_settings_len=ARRAY_SIZE(ov9650_vga)
	},
	{
		.mode=OV9650_QVGA,
		.width=320,
		.height=240,
		.max_exposure=248,
		.register_settings=ov9650_qvga,
		.register_settings_len=ARRAY_SIZE(ov9650_qvga)
	},
	/*{
		.mode=OV9650_QCIF,
		.width=176,
		.height=144,
		.register_settings=ov9650_qcif,
		.register_settings_len=ARRAY_SIZE(ov9650_qcif)
	},*/
	{
		.mode=OV9650_QQVGA,
		.width=160,
		.height=120,
		.max_exposure=248,
		.register_settings=ov9650_qqvga,
		.register_settings_len=ARRAY_SIZE(ov9650_qqvga)
	}
};

static int s3c2440_ov9650_get_format(s3c2440camif_sensor_config* cfg);
static int s3c2440_ov9650_get_auto_exposure(void);
static int s3c2440_ov9650_get_auto_gain(void);
static int s3c2440_ov9650_get_auto_wb(void);
static int s3c2440_ov9650_get_exposure(void);
static int s3c2440_ov9650_get_gain(void);
	
static int s3c2440_ov9650_set_auto_exposure(int);
static int s3c2440_ov9650_set_auto_gain(int);
static int s3c2440_ov9650_set_auto_wb(int);
static int s3c2440_ov9650_set_exposure(int);
static int s3c2440_ov9650_set_gain(int);

static int ov9650_mode=-1;
//camera modes
static bool ov9650_aec=1;
static bool ov9650_agc=1;
static bool ov9650_awb=1;
static int  ov9650_exp=0;
static int  ov9650_gain=0;


static int ov9650_poweron(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPG(12), S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(S3C2410_GPG(12), 0);
	mdelay(20);
	return 0;
}

static int ov9650_poweroff(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPG(12), S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(S3C2410_GPG(12), 1);
	mdelay(20);
	return 0;
}

static int __inline__ ov9650_check(void)
{
	u32 mid;

	mid = sccb_read(OV9650_SCCB_ADDR, 0x1c)<<8;
	mid |= sccb_read(OV9650_SCCB_ADDR, 0x1d);
	printk("SCCB address 0x%02X, ** manufacture ID 0x%04X, expect 0x%04X\n", OV9650_SCCB_ADDR, mid, OV9650_MANUFACT_ID);

	return (mid==OV9650_MANUFACT_ID)?1:0;
}

static u32 __inline__ show_ov9650_product_id(void)
{
	u32 pid;

	pid = sccb_read(OV9650_SCCB_ADDR, REG_PID)<<8;
	pid |= sccb_read(OV9650_SCCB_ADDR, REG_VER);
	printk("SCCB address 0x%02X, product ID 0x%04X, expect 0x%04X\n", OV9650_SCCB_ADDR, pid, OV9650_PRODUCT_ID);
	return pid;
}

static u32 ov9650_write_exp_mode(void)
{
	sccb_write(OV9650_SCCB_ADDR, REG_COM8, (1<<7)|(1<<6)|(0<<5)|(ov9650_awb<<1)|(ov9650_agc<<2)|(ov9650_aec<<0));
	return 0;
}


static int ov9650_init_regs(struct ov9650_reg* regs, u32 len)
{
	int i;
	
	//if(!down_interruptible(&regs_mutex))
	//{
		down(&regs_mutex);
		for (i=0; i<len; i++)
		{
			if (regs[i].subaddr == 0xff) 
			{
				mdelay(regs[i].value);
				continue;
			}
			sccb_write(OV9650_SCCB_ADDR, regs[i].subaddr, regs[i].value);
		}
		up(&regs_mutex);
		return 0;
	//} //else //failed to acquire lock
	//	return -1;
}

static int _s3c2440_ov9650_set_format(int mode)
{
	if(mode>=sizeof(ov9650_configurations))
	{
		printk(KERN_ERR"OV9650 can't find desired format!\n");
		return -1; //unable to find mode
	}
	if(ov9650_mode==mode)
		return 0; //don't waste time changing parameters
		
	printk(KERN_ERR"OV9650 setting format %d  %dx%d %d parameters!\n",
				 mode,ov9650_configurations[mode].width,ov9650_configurations[mode].height,
				 ov9650_configurations[mode].register_settings_len);
	ov9650_mode=(enum ov9650_mode)mode;
	
	return ov9650_init_regs(ov9650_configurations[mode].register_settings,
													ov9650_configurations[mode].register_settings_len);
}


static int s3c2440_ov9650_init(void)
{
	int ret=0;
	printk(KERN_ALERT"Loading OV9650 driver.........\n");
	sccb_init();//TODO: check error?
	/* power on. */
	ov9650_poweron();
	mdelay(100);

	/* check device. */
	if (ov9650_check() == 0 && ov9650_check() == 0)
	{
		printk(KERN_ERR"No OV9650 found!!!\n");
		return -ENODEV;
	}

	show_ov9650_product_id();
	//ov9650_init_regs();
	//ov9650_init_regs(ov9650_default,ARRAY_SIZE(ov9650_default) );
	
	ret=_s3c2440_ov9650_set_format(OV9650_VGA);//OV9650_SXGA
	ov9650_write_exp_mode();
	
	printk(KERN_ALERT"ov9650 init done!\n");
	return ret;
}

static int s3c2440_ov9650_set_format(s3c2440camif_sensor_config* cfg)
{
	//try to find the best mode
	int i;
	u32 ret;
	
	//save parameters
	int auto_exp= s3c2440_ov9650_get_auto_exposure();
	int auto_gain=s3c2440_ov9650_get_auto_gain();
	int auto_wb=  s3c2440_ov9650_get_auto_wb();
	int exp=      s3c2440_ov9650_get_exposure();
	int gain=     s3c2440_ov9650_get_gain();
	
	
	for(i=0;i<sizeof(ov9650_configurations);i++)
	{
		if(ov9650_configurations[i].width< cfg->width ||
			 ov9650_configurations[i].height<cfg->height)
			break;
	}
	if(i>0) i--; //take previous mode if available
	
	cfg->width=ov9650_configurations[i].width;
	cfg->height=ov9650_configurations[i].height;
	cfg->pixel_fmt=V4L2_PIX_FMT_YUYV;
	
	ret=_s3c2440_ov9650_set_format(i);
	if(!ret) //resotore previous mode 
	{
		//ov9650_write_exp_mode();
	 s3c2440_ov9650_set_auto_exposure(auto_exp);
	 s3c2440_ov9650_set_auto_gain(auto_gain);
	 s3c2440_ov9650_set_auto_wb(auto_wb);
	 //TODO: abjust exposure according to the resolution?
	 s3c2440_ov9650_set_exposure(exp);
	 s3c2440_ov9650_set_gain(gain);
	}
	return ret;
}

static int s3c2440_ov9650_get_format(s3c2440camif_sensor_config* cfg)
{
	cfg->width=ov9650_configurations[ov9650_mode].width;
	cfg->height=ov9650_configurations[ov9650_mode].height;
	cfg->pixel_fmt=V4L2_PIX_FMT_YUYV;
	return 0;
}

static int s3c2440_ov9650_get_largest_format(s3c2440camif_sensor_config* cfg)
{
	cfg->width=ov9650_configurations[OV9650_SXGA].width;
	cfg->height=ov9650_configurations[OV9650_SXGA].height;
	cfg->pixel_fmt=V4L2_PIX_FMT_YUYV;
	return 0;
}

static int s3c2440_ov9650_cleanup(void)
{
	sccb_cleanup();
	return 0;
}

static int s3c2440_ov9650_get_auto_exposure(void)
{	
	return (sccb_read(OV9650_SCCB_ADDR, REG_COM8)&0x01)==0x01?1:0;
}

static int s3c2440_ov9650_get_auto_gain(void)
{
	return (sccb_read(OV9650_SCCB_ADDR, REG_COM8)&0x04)==0x04?1:0;
}

static int s3c2440_ov9650_get_auto_wb(void)
{
	return (sccb_read(OV9650_SCCB_ADDR, REG_COM8)&0x02)==0x02?1:0;
}

static int s3c2440_ov9650_get_exposure(void)
{
	int p1=sccb_read(OV9650_SCCB_ADDR, REG_COM1)&0x03;
	int p2=sccb_read(OV9650_SCCB_ADDR, REG_AEC);
	int p3=sccb_read(OV9650_SCCB_ADDR, REG_AECHM)&0x3f;
	return p1|(p2<<2)|(p3<<10);
}

static int s3c2440_ov9650_get_gain(void)
{
	int p1=sccb_read(OV9650_SCCB_ADDR, REG_VREF)>>6;
	int p2=sccb_read(OV9650_SCCB_ADDR, REG_GAIN);
	return 	(p1<<8)|p2;
}

static int s3c2440_ov9650_set_auto_exposure(int v)
{
	u8 val=sccb_read(OV9650_SCCB_ADDR, REG_COM8)&(0xff-0x01);
	
	sccb_write(OV9650_SCCB_ADDR, REG_COM8,val|(v?0x01:0));
	return 0;
}

static int s3c2440_ov9650_set_auto_gain(int v)
{
	u8 val=sccb_read(OV9650_SCCB_ADDR, REG_COM8)&(0xff-0x04);
	sccb_write(OV9650_SCCB_ADDR, REG_COM8,val|(v?0x04:0));
	return 0;
}

static int s3c2440_ov9650_set_auto_wb(int v)
{
	u8 val=sccb_read(OV9650_SCCB_ADDR, REG_COM8)&(0xff-0x02);
	sccb_write(OV9650_SCCB_ADDR, REG_COM8,val|(v?0x02:0));
	return 0;
}

static int s3c2440_ov9650_set_exposure(int v)
{
	u8 com1,aechm;
	
	s3c2440_ov9650_set_auto_exposure(0);
	
	com1 =sccb_read(OV9650_SCCB_ADDR, REG_COM1)&0xfc;
	aechm=sccb_read(OV9650_SCCB_ADDR, REG_AECHM)&0xc0;
	
	sccb_write(OV9650_SCCB_ADDR, REG_AECHM,aechm|((v>>10)&0x003f));
	sccb_write(OV9650_SCCB_ADDR, REG_AEC,(v>>2)&0x00ff);
	sccb_write(OV9650_SCCB_ADDR, REG_COM1,com1|(v&0x0003));
	
	return 0;
}

static int s3c2440_ov9650_set_gain(int v)
{
	u8 vref;
	
	vref=(sccb_read(OV9650_SCCB_ADDR, REG_VREF)&0x3f)|((v&(3<<8))>>2);
	sccb_write(OV9650_SCCB_ADDR, REG_GAIN,v&0xff);
	sccb_write(OV9650_SCCB_ADDR, REG_VREF,vref);
	return 0;
}

s3c2440camif_sensor_operations s3c2440camif_sensor_if = 
{
	.init=s3c2440_ov9650_init,
	.cleanup=s3c2440_ov9650_cleanup,
	.set_format=s3c2440_ov9650_set_format,
	.get_format=s3c2440_ov9650_get_format,
	.get_largest_format=s3c2440_ov9650_get_largest_format,
	.poweron=ov9650_poweron,
	.poweroff=ov9650_poweroff,
	
	.get_auto_exposure=s3c2440_ov9650_get_auto_exposure,
	.get_auto_gain=s3c2440_ov9650_get_auto_gain,
	.get_auto_wb=s3c2440_ov9650_get_auto_wb,
	.get_exposure=s3c2440_ov9650_get_exposure,
	.get_gain=s3c2440_ov9650_get_gain,
	
	.set_auto_exposure=s3c2440_ov9650_set_auto_exposure,
	.set_auto_gain=s3c2440_ov9650_set_auto_gain,
	.set_auto_wb=s3c2440_ov9650_set_auto_wb,
	.set_exposure=s3c2440_ov9650_set_exposure,
	.set_gain=s3c2440_ov9650_set_gain,
};

#if 0
module_init(ov9650_init);
module_exit(ov9650_cleanup);
MODULE_LICENSE("GPL");
#endif
