#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#ifdef CONFIG_VIDEO_V4L1_COMPAT
#include <libv4l1-videodev.h> 
#endif
#include <linux/interrupt.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <linux/highmem.h>

#include <linux/dma-mapping.h>

#include <asm/io.h>
#include <asm/memory.h>
#include <mach/regs-gpio.h>
#include <mach/regs-gpioj.h>
#include <mach/regs-clock.h>
#include <mach/map.h>
#include <linux/gpio.h>

#include "sccb.h"
#include "s3c2440camif.h"
#include "gpj.h"
/* debug print macro. */

/* hardware & driver name, version etc. */
#define CARD_NAME		"camera"

static unsigned has_ov9650;
unsigned long camif_base_addr;
static int video_nr = -1;


/**********************************************************************
 * Module Parameters
 **********************************************************************/
static int static_allocate = 1;

module_param(static_allocate, int, 0);
MODULE_PARM_DESC(static_allocate, "Memory allocated statically for the biggest frame size (around 10mb)");

/* camera device(s) */
static s3c2440camif_dev camera;
static int s3c2410camif_set_format(s3c2440camif_dev *pcam, u32 format, u32 width, u32 height);

MODULE_DESCRIPTION("v4l2 driver module for s3c2440 camera interface");
MODULE_AUTHOR("Vladimir Fonov [vladimir.fonov@gmail.com]");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("video");

/* List of all V4Lv2 controls supported by the driver */
static struct v4l2_queryctrl s3c2410camif_controls[] = {
 {
		.id             = V4L2_CID_EXPOSURE_AUTO,
		.type           = V4L2_CTRL_TYPE_BOOLEAN,
		.name           = "Auto Exposure",
		.minimum        = 0,
		.maximum        = 1,
		.step           = 1,
		.default_value  = 1,
  }, {
		.id             = V4L2_CID_EXPOSURE,
		.type           = V4L2_CTRL_TYPE_INTEGER,
		.name           = "Exposure",
		.minimum        = 1,
		.maximum        = 0xffff,
		.step           = 1,
		.default_value  = 1,
		.flags          = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id             = V4L2_CID_AUTOGAIN,
		.type           = V4L2_CTRL_TYPE_BOOLEAN,
		.name           = "Automatic Gain",
		.minimum        = 0,
		.maximum        = 1,
		.step           = 1,
		.default_value  = 1,
	},{
		.id             = V4L2_CID_GAIN,
		.type           = V4L2_CTRL_TYPE_INTEGER,
		.name           = "Analog Gain",
		.minimum        = 0,
		.maximum        = 0x3ff,
		.step           = 1,
		.default_value  = 1,
		.flags          = V4L2_CTRL_FLAG_SLIDER,
	},{
		.id             = V4L2_CID_AUTO_WHITE_BALANCE,
		.type           = V4L2_CTRL_TYPE_BOOLEAN,
		.name           = "Auto WB",
		.minimum        = 0,
		.maximum        = 1,
		.step           = 0,
		.default_value  = 1,
	},
	/*TODO: get more controls to work */
};

extern s3c2440camif_sensor_operations s3c2440camif_sensor_if;

/* software reset camera interface. */
static void __inline__ soft_reset_camif(void)
{
	u32 cigctrl;

	cigctrl = (1<<31)|(1<<29);
	iowrite32(cigctrl, S3C244X_CIGCTRL);
	mdelay(10);

	cigctrl = (1<<29);// |(0<<28)|(1<<27)VF: test pattern
	iowrite32(cigctrl, S3C244X_CIGCTRL);
	mdelay(10);
}

/* software reset camera interface. */
static void __inline__ hw_reset_camif(void)
{
	u32 cigctrl;

	cigctrl = (1<<30)|(1<<29);
	iowrite32(cigctrl, S3C244X_CIGCTRL);
	mdelay(10);

	cigctrl = (1<<29);//|(0<<28)|(1<<27)VF: test pattern
	iowrite32(cigctrl, S3C244X_CIGCTRL);
	mdelay(10);

}

/*!
 * Free frame buffers status
 *
 * @param cam    Structure cam_data *
 *
 * @return none
 */
static void s3c2440camif_free_frame_buf(s3c2440camif_dev * cam)
{
	//int i;

	/*for (i = 0; i < S3C2440_FRAME_NUM; i++) {
			cam->frame[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;
	}*/

	INIT_LIST_HEAD(&cam->ready_q);
	INIT_LIST_HEAD(&cam->working_q);
	INIT_LIST_HEAD(&cam->done_q);
}


/*!
 * Deallocate frame buffers
 *
 * @param cam      Structure cam_data *
 *
 * @param count    int number of buffer need to deallocated
 *
 * @return status  -0 Successfully allocated a buffer, -ENOBUFS failed.
 */
static int s3c2440camif_deallocate_frame_buf(s3c2440camif_dev * cam, int count)
{
	int i;

	//printk(KERN_ALERT"s3c2440camif: Deallocating %d buffers\n",count);
	for (i = 0; i < count; i++) {
		if(cam->frame[i].Y_virt_base)
			free_pages((unsigned int)cam->frame[i].Y_virt_base, cam->frame[i].Y_order);
		
		if(cam->frame[i].CB_virt_base)
			free_pages((unsigned int)cam->frame[i].CB_virt_base, cam->frame[i].CBCR_order);
		
		if(cam->frame[i].CR_virt_base)
			free_pages((unsigned int)cam->frame[i].CR_virt_base, cam->frame[i].CBCR_order);
		
			//dma_free_coherent(0,PAGE_ALIGN(cam->v2f.fmt.pix.sizeimage),cam->frame[i].virt_base,cam->frame[i].phy_base);
		cam->frame[i].Y_virt_base=NULL;
		cam->frame[i].CR_virt_base=cam->frame[i].CB_virt_base=NULL;
		
		//cam->frame[i].phy_base=NULL;
	}
	return 0;
}

/*!
 * Allocate frame buffers
 *
 * @param cam      Structure cam_data *
 *
 * @param count    int number of buffer need to allocated
 *
 * @return status  -0 Successfully allocated a buffer, -ENOBUFS failed.
 */
static int s3c2440camif_allocate_frame_buf(s3c2440camif_dev * cam, int count)
{
	int i;
	
  //printk(KERN_ALERT"s3c2440camif: Allocating %d buffers, %d for Y , 2x%d for Cb and Cr\n",count,cam->Ysize,cam->CbCrsize);
	
	for (i = 0; i < count; i++) {
		if(cam->frame[i].Y_virt_base || cam->frame[i].CB_virt_base || cam->frame[i].CR_virt_base)
			printk(KERN_ERR "s3c2440camif:allocate_frame_buf called without freeing old buffers!.\n");
		
		cam->frame[i].Y_order = get_order(cam->Ysize);
		cam->frame[i].CBCR_order = get_order(cam->CbCrsize);
		
		cam->frame[i].Y_virt_base =  (void*)__get_free_pages(GFP_KERNEL|GFP_DMA, cam->frame[i].Y_order);
		cam->frame[i].CB_virt_base = (void*)__get_free_pages(GFP_KERNEL|GFP_DMA, cam->frame[i].CBCR_order);
		cam->frame[i].CR_virt_base = (void*)__get_free_pages(GFP_KERNEL|GFP_DMA, cam->frame[i].CBCR_order);
		
  	if (cam->frame[i].Y_virt_base == 0 || cam->frame[i].CB_virt_base == 0 || cam->frame[i].CR_virt_base == 0 ) {
						printk(KERN_ERR "s3c2440camif:allocate_frame_buf failed.\n");
						s3c2440camif_deallocate_frame_buf(cam,count);
						return -ENOBUFS;
		}
		//cam->frame[i].phy_base = virt_to_bus( cam->frame[i].virt_base );
		
		/*cam->frame[i].buffer.index = i;
		cam->frame[i].buffer.flags = V4L2_BUF_FLAG_MAPPED;
		cam->frame[i].buffer.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cam->frame[i].buffer.length =	PAGE_ALIGN(cam->v2f.fmt.pix.sizeimage);
		cam->frame[i].buffer.memory = V4L2_MEMORY_MMAP;*/
		//cam->frame[i].buffer.m.offset = cam->frame[i].phy_base;
		cam->frame[i].index = i;
	}
	return 0;
}


/*!
 * Valid whether the palette is supported
 *
 * @param palette V4L2_PIX_FMT_YUV422P //TODO: implement others?
 *
 * @return 0 if failed
 */
static inline int valid_mode(u32 palette)
{
        return ((palette == V4L2_PIX_FMT_YUV422P));
}


static void s3c2440camif_reg_dump(void)
{
  printk(KERN_ALERT"s3c2440camif: CIIMGCPT  = %x\n",ioread32(S3C244X_CIIMGCPT));  
  printk(KERN_ALERT"s3c2440camif: CICOSCCTRL= %x\n",ioread32(S3C244X_CICOSCCTRL));  
  printk(KERN_ALERT"s3c2440camif: CIWDOFST  = %x\n",ioread32(S3C244X_CIWDOFST));  
  printk(KERN_ALERT"s3c2440camif: CICOSTATUS     = %x\n",ioread32(S3C244X_CICOSTATUS));  
  printk(KERN_ALERT"s3c2440camif: CICOSCPRERATIO = %x\n",ioread32(S3C244X_CICOSCPRERATIO));  
  printk(KERN_ALERT"s3c2440camif: CICOSCPREDST   = %x\n",ioread32(S3C244X_CICOSCPREDST));  
  printk(KERN_ALERT"s3c2440camif: CICOTAREA      = %x\n",ioread32(S3C244X_CICOTAREA));  
  printk(KERN_ALERT"s3c2440camif: CIWDOFST       = %x\n",ioread32(S3C244X_CIWDOFST));
  printk(KERN_ALERT"s3c2440camif: CIGCTRL        = %x\n",ioread32(S3C244X_CIGCTRL));  
}


/* switch camif from codec path to preview path. */
static void __inline__ camif_c2p(s3c2440camif_dev * pdev)
{
	/* 1. stop codec. */
	{
		u32 cicoscctrl;
		cicoscctrl = ioread32(S3C244X_CICOSCCTRL);
		cicoscctrl &= ~(1<<15);	// stop preview scaler.
		iowrite32(cicoscctrl, S3C244X_CICOSCCTRL);
	}

	/* 2. soft-reset camif. */
	soft_reset_camif();

	/* 3. clear all overflow. */
	{
		u32 ciwdofst;
		ciwdofst = ioread32(S3C244X_CIWDOFST);
		ciwdofst |= (1<<30)|(1<<15)|(1<<14)|(1<<13)|(1<<12);
		iowrite32(ciwdofst, S3C244X_CIWDOFST);

		ciwdofst &= ~((1<<30)|(1<<15)|(1<<14)|(1<<13)|(1<<12));
		iowrite32(ciwdofst, S3C244X_CIWDOFST);
	}
}

/* calculate main burst size and remained burst size. */
static void __inline__ calc_burst_size(u32 pixperword,u32 hSize, u32 *mainBurstSize, u32 *remainedBurstSize)
{
	u32 tmp;

	tmp = (hSize/pixperword)%16;

	switch(tmp)
	{
	case 0:
		*mainBurstSize = 16;
		*remainedBurstSize = 16;
		break;
	case 4:
		*mainBurstSize = 16;
		*remainedBurstSize = 4;
		break;
	case 8:
		*mainBurstSize=16;
		*remainedBurstSize = 8;
		break;
	default: 
		tmp=(hSize/pixperword)%8;
		switch(tmp)
		{
		case 0:
			*mainBurstSize = 8;
			*remainedBurstSize = 8;
			break;
		case 4:
			*mainBurstSize = 8;
			*remainedBurstSize = 4;
		default:
			*mainBurstSize = 4;
			tmp = (hSize/pixperword)%4;
			*remainedBurstSize = (tmp)?tmp:4;
			break;
		}
		break;
	}
}

/* calculate prescaler ratio and shift. */
static void __inline__ calc_prescaler_ratio_shift(u32 SrcSize, u32 DstSize, u32 *ratio, u32 *shift)
{
	if(SrcSize>=32*DstSize)
	{
		*ratio=32;
		*shift=5;
	}
	else if(SrcSize>=16*DstSize)
	{
		*ratio=16;
		*shift=4;
	}
	else if(SrcSize>=8*DstSize)
	{
		*ratio=8;
		*shift=3;
	}
	else if(SrcSize>=4*DstSize)
	{
		*ratio=4;
		*shift=2;
	}
	else if(SrcSize>=2*DstSize)
	{
		*ratio=2;
		*shift=1;
	}
	else
	{
		*ratio=1;
		*shift=0;
	}    	
}

/* update CISRCFMT only. */
static void __inline__ update_source_fmt_regs(s3c2440camif_dev * pdev)
{
	u32 cisrcfmt;

	cisrcfmt = (1<<31)					// ITU-R BT.601 YCbCr 8-bit mode
				|(0<<30)				// CB,Cr value offset cntrol for YCbCr
				|(pdev->sensor.width<<16)	// source image width
//				|(0<<14)				// input order is YCbYCr
//				|(1<<14)				// input order is YCrYCb
				|(2<<14)				// input order is CbYCrY
//				|(3<<14)				// input order is CrYCbY
				|(pdev->sensor.height<<0);	// source image height

	iowrite32(cisrcfmt, S3C244X_CISRCFMT);
}

/* update registers:
 *	PREVIEW path:
 *		CIPRCLRSA1 ~ CIPRCLRSA4
 *		CIPRTRGFMT
 *		CIPRCTRL
 *		CIPRSCCTRL
 *		CIPRTAREA
 *	CODEC path:
 *		CICOYSA1 ~ CICOYSA4
 *		CICOCBSA1 ~ CICOCBSA4
 *		CICOCRSA1 ~ CICOCRSA4
 *		CICOTRGFMT
 *		CICOCTRL
 *		CICOTAREA
 */
static void __inline__ update_target_fmt_regs(s3c2440camif_dev * pdev)
{
	u32 ciprtrgfmt;
	u32 ciprctrl;
	u32 ciprscctrl;

  u32 cicrtrgfmt;
  u32 cicrctrl;
  u32 cicrscctrl;
  u32 cicoscctrl;
  u32 ciccoctrl;
  
	u32 YmainBurstSize, YremainedBurstSize;
  u32 CbCrmainBurstSize, CbCrremainedBurstSize;

  /* CIPRCLRSA1 ~ CIPRCLRSA4. */
  #if 0
  iowrite32(img_buff[0].phy_base, S3C244X_CIPRCLRSA1);
  iowrite32(img_buff[1].phy_base, S3C244X_CIPRCLRSA2);
  iowrite32(img_buff[2].phy_base, S3C244X_CIPRCLRSA3);
  iowrite32(img_buff[3].phy_base, S3C244X_CIPRCLRSA4);
  #else 
  
  
	
  //TODO: convert to proper use of video4linux buffers
  iowrite32(virt_to_bus(pdev->frame[0].Y_virt_base), S3C244X_CICOYSA1);
  iowrite32(virt_to_bus(pdev->frame[1].Y_virt_base), S3C244X_CICOYSA2);
  iowrite32(virt_to_bus(pdev->frame[2].Y_virt_base), S3C244X_CICOYSA3);
  iowrite32(virt_to_bus(pdev->frame[3].Y_virt_base), S3C244X_CICOYSA4);

  iowrite32(virt_to_bus(pdev->frame[0].CB_virt_base), S3C244X_CICOCBSA1);
  iowrite32(virt_to_bus(pdev->frame[1].CB_virt_base), S3C244X_CICOCBSA2);
  iowrite32(virt_to_bus(pdev->frame[2].CB_virt_base), S3C244X_CICOCBSA3);
  iowrite32(virt_to_bus(pdev->frame[3].CB_virt_base), S3C244X_CICOCBSA4);

  iowrite32(virt_to_bus(pdev->frame[0].CR_virt_base), S3C244X_CICOCRSA1);
  iowrite32(virt_to_bus(pdev->frame[1].CR_virt_base), S3C244X_CICOCRSA2);
  iowrite32(virt_to_bus(pdev->frame[2].CR_virt_base), S3C244X_CICOCRSA3);
  iowrite32(virt_to_bus(pdev->frame[3].CR_virt_base), S3C244X_CICOCRSA4);
	
	/*
	iowrite32(0, S3C244X_CICOCBSA1);
	iowrite32(0, S3C244X_CICOCBSA2);
  iowrite32(0, S3C244X_CICOCBSA3);
  iowrite32(0, S3C244X_CICOCBSA4);

  iowrite32(0, S3C244X_CICOCRSA1);
  iowrite32(0, S3C244X_CICOCRSA2);
  iowrite32(0, S3C244X_CICOCRSA3);
  iowrite32(0, S3C244X_CICOCRSA4);	*/
  #endif
  
  #if 0
  /* CIPRTRGFMT. */
  ciprtrgfmt = (pdev->preTargetHsize<<16)		// horizontal pixel number of target image
              |(0<<14)						// don't mirror or rotation.
              |(pdev->preTargetVsize<<0);	// vertical pixel number of target image
  iowrite32(ciprtrgfmt, S3C244X_CIPRTRGFMT);

  /* CIPRCTRL. */
  calc_burst_size(2, pdev->preTargetHsize, &mainBurstSize, &remainedBurstSize);
  ciprctrl = (mainBurstSize<<19)|(remainedBurstSize<<14);
  iowrite32(ciprctrl, S3C244X_CIPRCTRL);

  /* CIPRSCCTRL. */
  ciprscctrl = ioread32(S3C244X_CIPRSCCTRL);
  ciprscctrl &= 1<<15;	// clear all other info except 'preview scaler start'.
  ciprscctrl |= 0<<30;	// 16-bits RGB
  iowrite32(ciprscctrl, S3C244X_CIPRSCCTRL);	// 16-bit RGB

  /* CIPRTAREA. */
  iowrite32(pdev->preTargetHsize * pdev->preTargetVsize, S3C244X_CIPRTAREA);
  #else 
  /* CICOTRGFMT. */
  cicrtrgfmt =  (1<<31) //use input YCbCr:422 mode
               |(1<<30) //use output YCbCr:422 mode
               |(pdev->v2f.fmt.pix.width<<16)   // horizontal pixel number of target image
               |(0<<14)            // don't mirror or rotation.
               |(pdev->v2f.fmt.pix.height<<0); // vertical pixel number of target image
               
  iowrite32(cicrtrgfmt, S3C244X_CICOTRGFMT);

  /* CICOCTRL. */
  //YCrCb420 
  calc_burst_size(4, pdev->v2f.fmt.pix.width,   &YmainBurstSize,    &YremainedBurstSize);
  calc_burst_size(4, pdev->v2f.fmt.pix.width/2, &CbCrmainBurstSize, &CbCrremainedBurstSize);
  
  ciccoctrl = (YmainBurstSize<<19)  |(YremainedBurstSize<<14)|
              (CbCrmainBurstSize<<9)|(CbCrremainedBurstSize<<4);
  iowrite32(ciccoctrl, S3C244X_CICOCTRL);

  /* CICOSCCTRL. */
  //cicoscctrl = ioread32(S3C244X_CICOSCCTRL);
  //cicoscctrl &= 1<<15;  // clear all other info except 'codec scaler start ?'.
  
  /*cicoscctrl |=|(1<<30)
               |(1<<29)
               |(1<<16)   //horizontal 1:1 scale
               |(1<<0 ) ; //vertical 1:1 scale ?
               
  iowrite32(cicoscctrl, S3C244X_CICOSCCTRL);  */

  /* CICOTAREA. */
  iowrite32(pdev->v2f.fmt.pix.width*pdev->v2f.fmt.pix.height, S3C244X_CICOTAREA);
  
  
  #endif
}

/* update CIWDOFST only. */
static void __inline__ update_target_wnd_regs(s3c2440camif_dev * pdev)
{
	u32 ciwdofst;
	u32 winHorOfst, winVerOfst;

	//TODO: add support for window
	//winHorOfst = (pdev->sensor.width - pdev->wndHsize)>>1;
	//winVerOfst = (pdev->sensor.height - pdev->wndVsize)>>1;
	winHorOfst=0;
	winVerOfst=0;

	winHorOfst &= 0xFFFFFFF8;
	winVerOfst &= 0xFFFFFFF8;
  
	if ((winHorOfst == 0)&&(winVerOfst == 0))
	{
		ciwdofst =  (1<<30)      // clear the overflow ind flag of input CODEC FIFO Y
          |(1<<15)      // clear the overflow ind flag of input CODEC FIFO Cb
          |(1<<14)      // clear the overflow ind flag of input CODEC FIFO Cr
          |(1<<13)      // clear the overflow ind flag of input PREVIEW FIFO Cb
          |(1<<12);      // clear the overflow ind flag of input PREVIEW FIFO Cr
          
          //printk(KERN_ALERT"s3c2440camif: no window offset\n");           
	} else {
		ciwdofst = (1<<31)				// window offset enable
					|(1<<30)			// clear the overflow ind flag of input CODEC FIFO Y
					|(winHorOfst<<16)	// windows horizontal offset
					|(1<<15)			// clear the overflow ind flag of input CODEC FIFO Cb
					|(1<<14)			// clear the overflow ind flag of input CODEC FIFO Cr
					|(1<<13)			// clear the overflow ind flag of input PREVIEW FIFO Cb
					|(1<<12)			// clear the overflow ind flag of input PREVIEW FIFO Cr
					|(winVerOfst<<0);	// window vertical offset
		//printk(KERN_ALERT"s3c2440camif:  window offset:%d x %d\n",winHorOfst,winVerOfst);           
	}

	iowrite32(ciwdofst, S3C244X_CIWDOFST);
}

/* update registers:
 *	PREVIEW path:
 *		CIPRSCPRERATIO
 *		CIPRSCPREDST
 *		CIPRSCCTRL
 *	CODEC path:
 *		CICOSCPRERATIO
 *		CICOSCPREDST
 *		CICOSCCTRL
 */
static void __inline__ update_target_zoom_regs(s3c2440camif_dev * pdev)
{
    u32 preHratio, preVratio;
    u32 coHratio, coVratio;
    u32 Hshift, Vshift;
    u32 shfactor;
    
    u32 preDstWidth, preDstHeight;
    u32 coDstWidth, coDstHeight;
    u32 Hscale, Vscale;
    u32 mainHratio, mainVratio;

    u32 ciprscpreratio;
    u32 ciprscpredst;
    u32 ciprscctrl;

    u32 cicoscpreratio;
    u32 cicoscpredst;
    u32 cicoscctrl;


    #if 0
    /* CIPRSCPRERATIO. */
    calc_prescaler_ratio_shift(pdev->wndHsize, pdev->preTargetHsize,
                  &preHratio, &Hshift);
                  
    calc_prescaler_ratio_shift(pdev->wndVsize, pdev->preTargetVsize,
                  &preVratio, &Vshift);

    shfactor = 10 - (Hshift + Vshift);

    ciprscpreratio = (shfactor<<28)   // shift factor for preview pre-scaler
             |(preHratio<<16) // horizontal ratio of preview pre-scaler
             |(preVratio<<0); // vertical ratio of preview pre-scaler
             
    iowrite32(ciprscpreratio, S3C244X_CIPRSCPRERATIO);
    
		/* CIPRSCPREDST. */
		preDstWidth = pdev->wndHsize / preHratio;
		preDstHeight = pdev->wndVsize / preVratio;
		ciprscpredst = (preDstWidth<<16)	// destination width for preview pre-scaler
					   |(preDstHeight<<0);	// destination height for preview pre-scaler
		iowrite32(ciprscpredst, S3C244X_CIPRSCPREDST);

		/* CIPRSCCTRL. */
		Hscale = (pdev->wndHsize >= pdev->preTargetHsize)?0:1;
		Vscale = (pdev->wndVsize >= pdev->preTargetVsize)?0:1;
		mainHratio = (pdev->wndHsize<<8)/(pdev->preTargetHsize<<Hshift);
		mainVratio = (pdev->wndVsize<<8)/(pdev->preTargetVsize<<Vshift);
		ciprscctrl = ioread32(S3C244X_CIPRSCCTRL);
		ciprscctrl &= (1<<30)|(1<<15);	// keep preview image format (RGB565 or RGB24), and preview scaler start state.
		ciprscctrl |= (1<<31)	// this bit should be always 1.
					  |(Hscale<<29)		// ???, horizontal scale up/down.
					  |(Vscale<<28)		// ???, vertical scale up/down.
					  |(mainHratio<<16)	// horizontal scale ratio for preview main-scaler
					  |(mainVratio<<0);	// vertical scale ratio for preview main-scaler
            
		iowrite32(ciprscctrl, S3C244X_CIPRSCCTRL);
   #else 
	 
	  pdev->bypass_mode=(pdev->v2f.fmt.pix.width==pdev->sensor.width && pdev->v2f.fmt.pix.height==pdev->sensor.width );
    /* CIPRSCPRERATIO. */
    calc_prescaler_ratio_shift(pdev->wndHsize, pdev->v2f.fmt.pix.width,
                                &coHratio, &Hshift);
                  
    calc_prescaler_ratio_shift(pdev->wndVsize, pdev->v2f.fmt.pix.height,
                                &coVratio, &Vshift);

    shfactor = 10 - (Hshift + Vshift);

    ciprscpreratio = (shfactor<<28)   // shift factor for codec pre-scaler
                    |(coHratio<<16) // horizontal ratio of codec pre-scaler
                    |(coVratio<<0); // vertical ratio of codec pre-scaler
             
    iowrite32(ciprscpreratio, S3C244X_CICOSCPRERATIO);
    
    /* CIPRSCPREDST. */
    coDstWidth =  pdev->wndHsize / coHratio;
    coDstHeight = pdev->wndVsize / coVratio;
    
    cicoscpredst = (coDstWidth<<16)  // destination width for codec pre-scaler
                  |(coDstHeight<<0);  // destination height for codec pre-scaler
                  
    iowrite32(cicoscpredst, S3C244X_CICOSCPREDST  );

    /* CIPRSCCTRL. */
    Hscale = (pdev->wndHsize > pdev->v2f.fmt.pix.width)?0:1;
    Vscale = (pdev->wndVsize > pdev->v2f.fmt.pix.height)?0:1;
    
    mainHratio = (pdev->wndHsize<<8)/(pdev->v2f.fmt.pix.width<<Hshift);
    mainVratio = (pdev->wndVsize<<8)/(pdev->v2f.fmt.pix.height<<Vshift);
    
    //printk(KERN_ALERT"mainHratio=%d mainVratio=%d :\n",mainHratio,mainVratio);
    
    cicoscctrl = ioread32(S3C244X_CICOSCCTRL);
    cicoscctrl &= (1<<31)|(1<<15);  // keep bypass mode and start /stop.
    
    cicoscctrl |= ((pdev->bypass_mode?1:0)<<31) //bypass
            |(Hscale<<29)   // ???, horizontal scale up/down.
            |(Vscale<<30)   // ???, vertical scale up/down.
            |(mainHratio<<16) // horizontal scale ratio for codec main-scaler
            |(mainVratio<<0); // vertical scale ratio for codec main-scaler
            
    iowrite32(cicoscctrl, S3C244X_CICOSCCTRL);
    
    //printk(KERN_ALERT"update zoom:\n");
    //s3c2440camif_reg_dump();
   #endif
}

/* update camif registers, called only when camif ready, or ISR. */
static void __inline__ update_camif_regs(s3c2440camif_dev * pdev)
{
	if (!in_irq())
	{
		while(1)	// wait until VSYNC is 'L'
		{
			barrier();
			if ((ioread32(S3C244X_CICOSTATUS)&(1<<28)) == 0)
				break;
		}
	}

	/* WARNING: don't change the statement sort below!!! */
	update_source_fmt_regs(pdev);
	update_target_wnd_regs(pdev);
	update_target_fmt_regs(pdev);
	update_target_zoom_regs(pdev);
}

/* start image capture.
 *
 * param 'stream' means capture pictures streamly or capture only one picture.
 */
static int start_capture(s3c2440camif_dev * pdev, int stream)
{
	int ret;

	u32 ciwdofst;
	u32 ciprscctrl;
	u32 ciimgcpt;
  u32 cicoscctrl;
  u32 cigctrl;
  
  //printk(KERN_ALERT"s3c2440camif:  start capture\n");

	ciwdofst = ioread32(S3C244X_CIWDOFST);
	ciwdofst |=  (1<<30)		// Clear the overflow indication flag of input CODEC FIFO Y
              |(1<<15)	// Clear the overflow indication flag of input CODEC FIFO Cb
              |(1<<14)	// Clear the overflow indication flag of input CODEC FIFO Cr
              |(1<<13)	// Clear the overflow indication flag of input PREVIEW FIFO Cb
              |(1<<12);	// Clear the overflow indication flag of input PREVIEW FIFO Cr
        
	iowrite32(ciwdofst, S3C244X_CIWDOFST);

  #if 0
  ciprscctrl = ioread32(S3C244X_CIPRSCCTRL);
  ciprscctrl |= 1<<15;	// preview scaler start
  iowrite32(ciprscctrl, S3C244X_CIPRSCCTRL);

  ciimgcpt = (1<<31)    // camera interface global capture enable
             |(1<<29); // capture enable for preview scaler.
  iowrite32(ciimgcpt, S3C244X_CIIMGCPT);
  
  #else 
  
  //s3c2440camif_reg_dump();
  cicoscctrl = ioread32(S3C244X_CICOSCCTRL);
  cicoscctrl |= (1<<15) ; //bypass mode disable
  iowrite32(cicoscctrl,S3C244X_CICOSCCTRL);
  
  cigctrl = (1<<31)|(1<<30);
  iowrite32(cigctrl,S3C244X_CIIMGCPT); //enable global capture
  #endif
  
  pdev->state = CAMIF_STATE_CODECING;
  
  
	ret = 0;
	if (stream == 0)
	{
		pdev->cmdcode = CAMIF_CMD_STOP;
		ret = wait_event_interruptible(pdev->cmdqueue, pdev->cmdcode == CAMIF_CMD_NONE);
		
	} else {
    pdev->cmdcode = CAMIF_CMD_NONE;
  }
	return ret;
}

/* stop image capture, always called in ISR.
 *	P-path regs:
 *		CIPRSCCTRL
 *		CIPRCTRL
 *	C-path regs:
 *		CICOSCCTRL.
 *		CICOCTRL
 *	Global regs:
 *		CIIMGCPT
 */
static void stop_capture(s3c2440camif_dev * pdev)
{
	u32 ciprscctrl;
	u32 ciprctrl;

	u32 cicoscctrl;
	u32 cicoctrl;

  //printk(KERN_ALERT"s3c2440camif:  stop capture\n");

	switch(pdev->state)
	{
	case CAMIF_STATE_PREVIEWING:
		/* CIPRCTRL. */
		ciprctrl = ioread32(S3C244X_CIPRCTRL);
		ciprctrl |= 1<<2;		// enable last IRQ at the end of frame capture.
		iowrite32(ciprctrl, S3C244X_CIPRCTRL);

		/* CIPRSCCTRL. */
		ciprscctrl = ioread32(S3C244X_CIPRSCCTRL);
		ciprscctrl &= ~(1<<15);		// clear preview scaler start bit.
		iowrite32(ciprscctrl, S3C244X_CIPRSCCTRL);

		/* CIIMGCPT. */
		iowrite32(0, S3C244X_CIIMGCPT);
		pdev->state = CAMIF_STATE_READY;

		break;

	case CAMIF_STATE_CODECING:
		/* CICOCTRL. */
		cicoctrl = ioread32(S3C244X_CICOCTRL);
		cicoctrl |= 1<<2;		// enable last IRQ at the end of frame capture.
		iowrite32(cicoctrl, S3C244X_CICOCTRL);

		/* CICOSCCTRL. */
		cicoscctrl = ioread32(S3C244X_CICOSCCTRL);
		cicoscctrl &= ~(1<<15);		// clear codec scaler start bit.
		iowrite32(cicoscctrl, S3C244X_CICOSCCTRL);

		/* CIIMGCPT. */
		iowrite32(0, S3C244X_CIIMGCPT);
		pdev->state = CAMIF_STATE_READY;

		break;

	}
}

/* update camera interface with the new config. */
static void update_camif_config ( s3c2440camif_dev	* pdev, u32 cmdcode)
{

	switch (pdev->state)
	{
	case CAMIF_STATE_READY:
		update_camif_regs(pdev);		// config the regs directly.
		break;

	case CAMIF_STATE_PREVIEWING:

		/* camif is previewing image. */

		disable_irq(IRQ_S3C2440_CAM_P);		// disable cam-preview irq.

		/* source image format. */
		if (cmdcode & CAMIF_CMD_SFMT)
		{
			// ignore it, nothing to do now.
		}

		/* target image format. */
		if (cmdcode & CAMIF_CMD_TFMT)
		{
				/* change target image format only. */
				pdev->cmdcode |= CAMIF_CMD_TFMT;
		}

		/* target image window offset. */
		if (cmdcode & CAMIF_CMD_WND)
		{
			pdev->cmdcode |= CAMIF_CMD_WND;
		}

		/* target image zoomi & zoomout. */
		if (cmdcode & CAMIF_CMD_ZOOM)
		{
			pdev->cmdcode |= CAMIF_CMD_ZOOM;
		}

		/* stop previewing. */
		if (cmdcode & CAMIF_CMD_STOP)
		{
			pdev->cmdcode |= CAMIF_CMD_STOP;
		}
		enable_irq(IRQ_S3C2440_CAM_P);	// enable cam-preview irq.

		wait_event(pdev->cmdqueue, (pdev->cmdcode==CAMIF_CMD_NONE));	// wait until the ISR completes command.
		break;

	case CAMIF_STATE_CODECING:

		/* camif is previewing image. */

		disable_irq(IRQ_S3C2440_CAM_C);		// disable cam-codec irq.

		/* source image format. */
		if (cmdcode & CAMIF_CMD_SFMT)
		{
			// ignore it, nothing to do now.
		}

		/* target image format. */
		if (cmdcode & CAMIF_CMD_TFMT)
		{
				/* change target image format only. */
				pdev->cmdcode |= CAMIF_CMD_TFMT;
		}

		/* target image window offset. */
		if (cmdcode & CAMIF_CMD_WND)
		{
			pdev->cmdcode |= CAMIF_CMD_WND;
		}

		/* target image zoomi & zoomout. */
		if (cmdcode & CAMIF_CMD_ZOOM)
		{
			pdev->cmdcode |= CAMIF_CMD_ZOOM;
		}

		/* stop previewing. */
		if (cmdcode & CAMIF_CMD_STOP)
		{
			pdev->cmdcode |= CAMIF_CMD_STOP;
		}
		enable_irq(IRQ_S3C2440_CAM_C);	// enable cam-codec irq.
		wait_event(pdev->cmdqueue, (pdev->cmdcode==CAMIF_CMD_NONE));	// wait until the ISR completes command.
		break;

	default:
		break;
	}
}

/* config camif when master-open camera.*/
static void init_camif_config(s3c2440camif_dev	* pdev)
{
	update_camif_config(pdev, CAMIF_CMD_STOP);
}

/*
 * ISR: service for C-path interrupt.
 */
static irqreturn_t on_camif_irq_c(int irq, void * dev)
{
	u32 cicostatus;
	u32 frame;
  int oflow_y,oflow_cb,oflow_cr;
	s3c2440camif_dev * pdev;
  

	cicostatus = ioread32(S3C244X_CICOSTATUS);
	if ((cicostatus & (1<<21))== 0)
	{
		return IRQ_RETVAL(IRQ_NONE);
	}
  

	pdev = (s3c2440camif_dev *)dev;

	/* valid img_buff[x] just DMAed. */
	frame = (cicostatus&(3<<26))>>26;
	//frame = (frame+4-1)%4;
  
  oflow_y=(cicostatus&(1<<31))>>31;
  oflow_cb=(cicostatus&(1<<30))>>30; 
  oflow_cr=(cicostatus&(1<<29))>>29; 
  
  //img_buff[frame].state = CAMIF_BUFF_YCbCr420;
  pdev->frame[frame].state = CAMIF_BUFF_YCbCr422;
	
  //printk(KERN_ALERT"%d %d %d %d\n",img_buff[0].state,img_buff[1].state,img_buff[2].state,img_buff[3].state);

  pdev->last_frame=frame;
  //printk(KERN_ALERT"on_camif_irq_c %d %d %d %d\n",frame,oflow_y,oflow_cb,oflow_cr);

	if (pdev->cmdcode & CAMIF_CMD_STOP)
	{
		stop_capture(pdev);

		pdev->state = CAMIF_STATE_READY;
	}
	else
	{
		if (pdev->cmdcode & CAMIF_CMD_C2P)
		{
			camif_c2p(pdev);
		}

		if (pdev->cmdcode & CAMIF_CMD_WND)
		{
			update_target_wnd_regs(pdev);
		}

		if (pdev->cmdcode & CAMIF_CMD_TFMT)
		{
			update_target_fmt_regs(pdev);
		}

		if (pdev->cmdcode & CAMIF_CMD_ZOOM)
		{
			update_target_zoom_regs(pdev);
		}

		//invalid_image_buffer();
	}
	pdev->cmdcode = CAMIF_CMD_NONE;
	wake_up(&pdev->cmdqueue);
	
  //wake_up(&pdev->cap_queue);


	return IRQ_RETVAL(IRQ_HANDLED);
}

/*
 * ISR: service for P-path interrupt.
 */
static irqreturn_t on_camif_irq_p(int irq, void * dev)
{
	u32 ciprstatus;

	u32 frame;
	s3c2440camif_dev * pdev;

  //printk(KERN_ALERT"on_camif_irq_p\n");
	ciprstatus = ioread32(S3C244X_CIPRSTATUS);
  
	if ((ciprstatus & (1<<21))== 0)
	{
		return IRQ_RETVAL(IRQ_NONE);
	}

	pdev = (s3c2440camif_dev *)dev;

	/* valid img_buff[x] just DMAed. */
	frame = (ciprstatus&(3<<26))>>26;
	frame = (frame+4-1)%4;
  
  //printk(KERN_ALERT"on_camif_irq_p %d\n",frame);
  
  pdev->last_frame=frame;

  pdev->frame[frame].state = CAMIF_BUFF_RGB565;

	if (pdev->cmdcode & CAMIF_CMD_STOP)
	{
		stop_capture(pdev);

		pdev->state = CAMIF_STATE_READY;
	}
	else
	{
		if (pdev->cmdcode & CAMIF_CMD_P2C)
		{
			camif_c2p(pdev);
		}

		if (pdev->cmdcode & CAMIF_CMD_WND)
		{
			update_target_wnd_regs(pdev);
		}

		if (pdev->cmdcode & CAMIF_CMD_TFMT)
		{
			update_target_fmt_regs(pdev);
		}

		if (pdev->cmdcode & CAMIF_CMD_ZOOM)
		{
			update_target_zoom_regs(pdev);
		}
		//TODO signal that all frames are invalid
	}
	pdev->cmdcode = CAMIF_CMD_NONE;
	wake_up(&pdev->cmdqueue);
  //wake_up(&pdev->cap_queue);

	return IRQ_RETVAL(IRQ_HANDLED);
}

/*
 * camif_open()
 */
static int camif_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	s3c2440camif_dev *pcam = (s3c2440camif_dev *)video_get_drvdata(vdev);
	
	int ret;

	if (!has_ov9650) {
		return -ENODEV;
	}

  file->private_data = vdev;

  pcam->state = CAMIF_STATE_READY;

	init_camif_config(pcam);
	
  
  pcam->last_frame=-1;
  
  if((ret=request_irq(IRQ_S3C2440_CAM_C, on_camif_irq_c, IRQF_DISABLED, "CAM_C", pcam))<0)	// setup ISRs
    goto error2;

  if((ret=request_irq(IRQ_S3C2440_CAM_P, on_camif_irq_p, IRQF_DISABLED, "CAM_P", pcam))<0)
    goto error3;

  clk_enable(pcam->clk);		// and enable camif clock.

  //soft_reset_camif();
	//update_camif_config(pcam, 0);
	//make sure frames are up-to date
	if((ret=s3c2410camif_set_format(pcam, pcam->v2f.fmt.pix.pixelformat, pcam->v2f.fmt.pix.width, pcam->v2f.fmt.pix.height))<0)
		goto error1;
	

  //start streaming
  if (start_capture(pcam, 1) != 0)
    goto error3;
  
	return 0;

error3:
	free_irq(IRQ_S3C2440_CAM_C, pcam);
error2:
	s3c2440camif_deallocate_frame_buf(pcam,S3C2440_FRAME_NUM);
error1:
	return ret;
}

/*
 * camif_read()
 */
static ssize_t camif_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
	struct video_device *vdev=file->private_data;
	s3c2440camif_dev *pcam = (s3c2440camif_dev *)video_get_drvdata(vdev);
	int ret=0;
  int last_frame;
	size_t _count;

	//disable_irq(IRQ_S3C2440_CAM_C);
	//disable_irq(IRQ_S3C2440_CAM_P);
 
  last_frame= pcam->last_frame;//atomic operation?
 //try to find a valid frame
  //if( last_frame != -1)
   
	//printk(KERN_ALERT"read: %d %d %d %d\n",pcam->frame[0].state,pcam->frame[1].state,pcam->frame[2].state,pcam->frame[3].state);
	if((last_frame==-1) || (pcam->frame[last_frame].state == CAMIF_BUFF_INVALID))
	{
		//let's sleep ?
		wait_event_interruptible(pcam->cmdqueue, (last_frame!=pcam->last_frame));
		last_frame=pcam->last_frame;
	}
	
	if(count> pcam->v2f.fmt.pix.sizeimage)
		count=pcam->v2f.fmt.pix.sizeimage;
	ret=count;
	
	_count=count>pcam->Ysize?pcam->Ysize:count;
	copy_to_user(data, (void *)pcam->frame[last_frame].Y_virt_base, _count);
	count-=_count;
	data+=_count;
	
	if(count>0)
	{
		_count=count>pcam->CbCrsize?pcam->CbCrsize:count;
		copy_to_user(data, (void *)pcam->frame[last_frame].CB_virt_base, _count);
		count-=_count;
		data+=_count;
		
		if(count>0)
		{
			_count=count>pcam->CbCrsize?pcam->CbCrsize:count;
			copy_to_user(data, (void *)pcam->frame[last_frame].CR_virt_base, _count);
		}
	}
	
	pcam->frame[last_frame].state=CAMIF_BUFF_INVALID; //mark as invalid
	
	//printk(KERN_ALERT"+\n");
	return ret;
	 
  //printk(KERN_ALERT"-\n");
	//enable_irq(IRQ_S3C2440_CAM_P);
	//enable_irq(IRQ_S3C2440_CAM_C);
	//return 0;
}

/*
 * camif_release()
 */
static int camif_release(struct file *file)
{
	struct video_device *vdev=file->private_data;
	s3c2440camif_dev *pcam = (s3c2440camif_dev *)video_get_drvdata(vdev);
	int ret=0;

	if(pcam->state == CAMIF_STATE_PREVIEWING || pcam->state == CAMIF_STATE_CODECING)
	{
		pcam->cmdcode = CAMIF_CMD_STOP;
		ret = wait_event_interruptible(pcam->cmdqueue, pcam->cmdcode == CAMIF_CMD_NONE);
	}

  clk_disable(pcam->clk);				// stop camif clock

  free_irq(IRQ_S3C2440_CAM_P, pcam);	// free camif IRQs

  free_irq(IRQ_S3C2440_CAM_C, pcam);

	if(!static_allocate)
		s3c2440camif_deallocate_frame_buf(pcam,S3C2440_FRAME_NUM);

	return ret;
}



/*!
 * V4L interface - poll function
 *
 * @param file       structure file *
 *
 * @param wait       structure poll_table *
 *
 * @return  status   POLLIN | POLLRDNORM
 */
static unsigned int camif_poll(struct file *file, poll_table * wait)
{
	struct video_device *dev = video_devdata(file);
	s3c2440camif_dev *pcam = video_get_drvdata(dev);
	unsigned int ret = 0;
	wait_queue_head_t *queue = NULL;
	
	/*
	int res = POLLIN | POLLRDNORM;
	if (down_interruptible(&pcam->busy_lock))
		return -EINTR;*/
	if (pcam->last_frame>-1 && pcam->frame[pcam->last_frame].state!=CAMIF_BUFF_INVALID) ret = POLLIN | POLLRDNORM;

	queue = &pcam->cmdqueue;
	poll_wait(file, queue, wait);

	//up(&cam->busy_lock);
	return ret;
}

static int s3c2410camif_querycap(struct file *file, void  *priv, struct v4l2_capability *cap)
{
	struct video_device *dev = video_devdata(file);
	s3c2440camif_dev *pcam = video_get_drvdata(dev);


	//WARN_ON(priv != file->private_data);

	strcpy(cap->driver,"s3c2440camera" );
	strcpy(cap->card, "s3c2410camif");
	cap->version =  KERNEL_VERSION(0,0,1);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE|V4L2_CAP_READWRITE; //TODO: move to mmap someday
	return 0;
}

	

static s3c2410camif_fmt formats[] = {
	{
		.name     = "4:2:2, planar, YUYV",
		.fourcc   = V4L2_PIX_FMT_YUV422P,
		.depth    = 16,
	},
	{ //TODO: this is another format that hardware may produce
		.name     = "4:2:0, planar, YUYV",
		.fourcc   = V4L2_PIX_FMT_YVU420,
		.depth    = 12, // 8+8+8+8Y + 8U + 8V /4
	},
	{ //TODO: this is format that Preview module can generate
		.name     = "RGB565 (LE)",
		.fourcc   = V4L2_PIX_FMT_RGB565, /* gggbbbbb rrrrrggg */
		.depth    = 16,
	},
	{//TODO: this is also format that Preview module can generate
		.name     = "RGB24",
		.fourcc   = V4L2_PIX_FMT_RGB24, 
		.depth    = 24,
	}
};


static int s3c2410camif_enum_fmt_vid_cap(struct file *file, void  *priv, struct v4l2_fmtdesc *f)
{
	struct video_device *dev = video_devdata(file);
	//s3c2440camif_dev *pcam = video_get_drvdata(dev);
	s3c2410camif_fmt *fmt;
	
	if(f->index >= 1) //TODO: add support for other formats
		return -EINVAL;
	//FOR now we oly support YCbCr422p format
	fmt=&formats[f->index];
	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int s3c2410camif_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	struct video_device *dev = video_devdata(file);
	s3c2440camif_dev *pcam = video_get_drvdata(dev);
	
	*f=pcam->v2f;
	return (0);
}

static int s3c2410camif_set_format(s3c2440camif_dev *pcam,u32 format,u32 width,u32 height)
{
	int ret=0;
	u32 scH,scV;
  int last_state=pcam->state;
	
	if(pcam->state == CAMIF_STATE_PREVIEWING || pcam->state == CAMIF_STATE_CODECING)
	{
		pcam->cmdcode = CAMIF_CMD_STOP;
		ret = wait_event_interruptible(pcam->cmdqueue, pcam->cmdcode == CAMIF_CMD_NONE);
	}
	
	disable_irq(IRQ_S3C2440_CAM_C);
	disable_irq(IRQ_S3C2440_CAM_P);

	//deallocate old buffers
	if(!static_allocate)
		s3c2440camif_deallocate_frame_buf(pcam,S3C2440_FRAME_NUM);
	
	//try to get something close from the sensor
	pcam->sensor.width=width;
	pcam->sensor.height=height;
	pcam->sensor.pixel_fmt=V4L2_PIX_FMT_YUYV;
	pcam->sensor_op->set_format(&pcam->sensor);
	//copy data back
	pcam->wndHsize = pcam->sensor.width;
	pcam->wndVsize = pcam->sensor.height;
	
	if(width >pcam->sensor.width) width=pcam->sensor.width;
	if(height>pcam->sensor.height) height=pcam->sensor.height;
	
	//round up to an integer scaling factor
	scH=pcam->sensor.width/width;
	scV=pcam->sensor.height/height;
	
	width=pcam->sensor.width/scH;
	height=pcam->sensor.height/scV;
	
	//TODO: add support for other formats
	pcam->v2f.fmt.pix.pixelformat=V4L2_PIX_FMT_YUV422P;
	
	pcam->v2f.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	pcam->v2f.fmt.pix.width=width;
	pcam->v2f.fmt.pix.height=height;
	pcam->v2f.fmt.pix.bytesperline=width;
	pcam->v2f.fmt.pix.field=V4L2_FIELD_NONE;
	pcam->v2f.fmt.pix.bytesperline=width;
	
	pcam->v2f.fmt.pix.sizeimage=width*height*2; 
	
  //setup for YCrCb422p mode
  //TODO: add switch for other modes
  pcam->Ysize=   pcam->v2f.fmt.pix.width*pcam->v2f.fmt.pix.height;
  pcam->CbCrsize=pcam->v2f.fmt.pix.width*pcam->v2f.fmt.pix.height/2;
	
	if(!static_allocate)
	{
		if((ret=s3c2440camif_allocate_frame_buf(pcam,S3C2440_FRAME_NUM))<0) 
			return ret;
	}
	
	//update_camif_config(pcam,0);
	
	init_camif_config(pcam);
	enable_irq(IRQ_S3C2440_CAM_C);
	enable_irq(IRQ_S3C2440_CAM_P);
	
	pcam->last_frame=-1;//wait for fresh frame
	
	if(last_state==CAMIF_STATE_CODECING || last_state== CAMIF_STATE_PREVIEWING)
		return start_capture(pcam,1);	
	else 
		return 0;
}

static int s3c2410camif_s_fmt_vid_cap(struct file *file, void *priv,struct v4l2_format *f)
{
	struct video_device *dev = video_devdata(file);
	s3c2440camif_dev *pcam = video_get_drvdata(dev);

	int ret=0;
	
	//TODO: stop capture
	ret=s3c2410camif_set_format(pcam,f->fmt.pix.pixelformat,f->fmt.pix.width,f->fmt.pix.height);
	*f=pcam->v2f;
	
	//TODO: start capture
	return ret;
}


static int s3c2410camif_vidioc_queryctrl(struct file *file,
                void *priv, struct v4l2_queryctrl *c)
{
	int i;
	int nbr;
	nbr = ARRAY_SIZE(s3c2410camif_controls);

	for (i = 0; i < nbr; i++) {
		if (s3c2410camif_controls[i].id == c->id) {
			memcpy(c, &s3c2410camif_controls[i],	sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}
	
	return -EINVAL;
}

static int s3c2410camif_vidioc_enum_input(struct file *file,
                void *priv, struct v4l2_input *input)
{
	if (input->index != 0)
		return -EINVAL;

	strcpy(input->name, "S3C2410 Camera");
	input->type = V4L2_INPUT_TYPE_CAMERA;
	return 0;
}

static int s3c2410camif_vidioc_g_ctrl(struct file *file,
                void *priv, struct v4l2_control *c)
{
	struct video_device *dev = video_devdata(file);
	s3c2440camif_dev *pcam = video_get_drvdata(dev);

	switch (c->id) {
		case V4L2_CID_EXPOSURE:
			c->value=pcam->sensor_op->get_exposure();
			return 0;
		case V4L2_CID_AUTOGAIN:
			c->value=pcam->sensor_op->get_auto_gain();
			return 0;
		case V4L2_CID_GAIN:
			c->value=pcam->sensor_op->get_gain();
			return 0;
		case V4L2_CID_EXPOSURE_AUTO:
			c->value=pcam->sensor_op->get_auto_exposure();
			return 0;
		case V4L2_CID_AUTO_WHITE_BALANCE:
			c->value=pcam->sensor_op->get_auto_wb();
			return 0;
		default:
			return -EINVAL;
	}
	return 0;
}

static int s3c2410camif_vidioc_s_ctrl(struct file *file,
                void *priv, struct v4l2_control *c)
{
	struct video_device *dev = video_devdata(file);
	s3c2440camif_dev *pcam = video_get_drvdata(dev);
	
	switch (c->id) {
		case V4L2_CID_AUTOGAIN:
			return pcam->sensor_op->set_auto_gain(c->value);
		case V4L2_CID_GAIN:
			return pcam->sensor_op->set_gain(c->value);
		case V4L2_CID_EXPOSURE_AUTO:
			return pcam->sensor_op->set_auto_exposure(c->value);
		case V4L2_CID_EXPOSURE:
			return pcam->sensor_op->set_exposure(c->value);
		case V4L2_CID_AUTO_WHITE_BALANCE:
			return pcam->sensor_op->set_auto_wb(c->value);
		default:
			return -EINVAL;
	}
	return 0;
}


static struct v4l2_file_operations s3c2410camif_fops = {
	.owner = THIS_MODULE,
	.open = camif_open,
	.release = camif_release,
	.read = camif_read,
	.ioctl = video_ioctl2,
	.mmap = NULL,//TODO: implement this camif_mmap
	.poll = camif_poll,
};

static const struct v4l2_ioctl_ops s3c2410camif_ioctl_ops = {
	.vidioc_querycap	       = s3c2410camif_querycap,
	.vidioc_enum_fmt_vid_cap = s3c2410camif_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap    = s3c2410camif_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap    = s3c2410camif_s_fmt_vid_cap,
	.vidioc_g_ctrl           = s3c2410camif_vidioc_g_ctrl,
	.vidioc_s_ctrl           = s3c2410camif_vidioc_s_ctrl,	
	.vidioc_enum_input       = s3c2410camif_vidioc_enum_input,
	.vidioc_queryctrl        = s3c2410camif_vidioc_queryctrl,
};

static struct video_device s3c2410camif_v4l_template = {
        .name = "s3c2410camif Camera",
        .fops = &s3c2410camif_fops,
				.ioctl_ops = & s3c2410camif_ioctl_ops,
        .release = video_device_release,
};

static void camera_platform_release(struct device *device)
{
}

static struct platform_device s3c2410camif_v4l2_devices = {
        .name = "s3c2410camif_v4l2",
        .dev = {
                .release = camera_platform_release,
                },
        .id = 0,
};


static int init_s3c2410camif_struct(s3c2440camif_dev * cam)
{
	int i;
	/* Default everything to 0 */
	memset(cam, 0, sizeof(s3c2440camif_dev));
	
  cam->video_dev = video_device_alloc();
	if (cam->video_dev == NULL)
					return -ENOMEM;
	
	 *(cam->video_dev) = s3c2410camif_v4l_template;
	
	video_set_drvdata(cam->video_dev, cam);
	dev_set_drvdata(&s3c2410camif_v4l2_devices.dev, (void *)cam);
	cam->video_dev->minor = -1;
	
	for (i = 0; i < S3C2440_FRAME_NUM; i++) {
		cam->frame[i].Y_virt_base =0;
		cam->frame[i].CB_virt_base=0;
		cam->frame[i].CR_virt_base=0;
	}
	
	cam->sensor_op = &s3c2440camif_sensor_if;
	
	/* init reference counter and its mutex. */
	mutex_init(&cam->rcmutex);
	cam->rc = 0;

	/* init image input source. */
	cam->input = 0;
	
	/* bypass is on by default */
	cam->bypass_mode = 1;

	/* init camif state and its lock. */
	cam->state = CAMIF_STATE_FREE;

	/* init command code, command lock and the command wait queue. */
	cam->cmdcode = CAMIF_CMD_NONE;
	
	init_waitqueue_head(&cam->cmdqueue);
	//init_waitqueue_head(&cam->cap_queue);
  
  cam->last_frame=-1;
	
	//cam->streamparm.parm.capture.capturemode = 0;
	
	/*
	cam->standard.index = 0;
	cam->standard.id = V4L2_STD_UNKNOWN;
	cam->standard.frameperiod.denominator = 4;
	cam->standard.frameperiod.numerator = 1;
	cam->standard.framelines = HEIGHT;
	cam->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	cam->streamparm.parm.capture.timeperframe = cam->standard.frameperiod;
	cam->streamparm.parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
	*/
	
/*	cam->overlay_on = false;
	cam->capture_on = false;
	cam->skip_frame = 0;
	cam->v4l2_fb.capability = V4L2_FBUF_CAP_EXTERNOVERLAY;
	cam->v4l2_fb.flags = V4L2_FBUF_FLAG_PRIMARY;*/

	cam->input = 0;	      // FIXME, the default input image format, see inputs[] for detail.

	/* the source image size (input from external camera). */
	cam->sensor.width = 1280;	// FIXME, the OV9650's horizontal output pixels.
	cam->sensor.height = 1024;	// FIXME, the OV9650's verical output pixels.

	/* the windowed image size. */
	cam->wndHsize = 1280;
	cam->wndVsize = 1024;

	/* codec-path target(output) image size. */
	cam->coTargetHsize = cam->wndHsize;
	cam->coTargetVsize = cam->wndVsize;

	/* preview-path target(preview) image size. */
	cam->preTargetHsize = 640;
	cam->preTargetVsize = 512;
	//TODO: query this from the sensor later

	//YCbCr422 mode
	
	cam->v2f.fmt.pix.pixelformat =  V4L2_PIX_FMT_YUV422P;
	cam->v2f.fmt.pix.sizeimage =    cam->coTargetHsize*cam->coTargetVsize*2;
	cam->v2f.fmt.pix.bytesperline = cam->coTargetHsize;
	cam->v2f.fmt.pix.width =        cam->coTargetHsize;
	cam->v2f.fmt.pix.height =       cam->coTargetVsize;
	cam->v2f.fmt.pix.field = V4L2_FIELD_NONE;

	return 0;
}

/*!
 * This function is called to put the sensor in a low power state. Refer to the
 * document driver-model/driver.txt in the kernel source tree for more
 * information.
 *
 * @param   pdev  the device structure used to give information on which I2C
 *                to suspend
 * @param   state the power state the device is entering
 *
 * @return  The function returns 0 on success and -1 on failure.
 */
static int s3c2440camif_v4l2_suspend(struct platform_device *pdev, pm_message_t state)
{
	s3c2440camif_dev *cam = platform_get_drvdata(pdev);

	if (cam == NULL) {
		return -1;
	}
	//TODO: implemetn low power mode
	return 0;
}

/*!
 * This function is called to bring the sensor back from a low power state.Refer
 * to the document driver-model/driver.txt in the kernel source tree for more
 * information.
 *
 * @param   pdev   the device structure
 *
 * @return  The function returns 0 on success and -1 on failure
 */
static int s3c2440camif_v4l2_resume(struct platform_device *pdev)
{
	s3c2440camif_dev *cam = platform_get_drvdata(pdev);

	if (cam == NULL) {
					return -1;
	}

	//TODO: implemetn low power mode

	return 0;
}

/*
 * This structure contains pointers to the power management callback functions.
 */
static struct platform_driver s3c2410camif_v4l2_driver = {
        .driver = {
                   .name = "s3c2440camif Platform Driver",
                   },
        .probe = NULL,
        .remove = NULL,
        .suspend = s3c2440camif_v4l2_suspend,
        .resume = s3c2440camif_v4l2_resume,
        .shutdown = NULL,
};



/*
 * camif_init()
 */
static int __init camif_init(void)
{
	
	u8 err = 0;
	int ret;
	s3c2440camif_dev *cam;
	struct clk * camif_upll_clk;
	
	cam=&camera;
  if((ret=init_s3c2410camif_struct(cam))<0)
		return ret;
	
	/* Register the I2C device */
	err = platform_device_register(&s3c2410camif_v4l2_devices);
	if (err != 0) {
		pr_debug("1.camif_init: platform_device_register failed.\n");
		platform_driver_unregister(&s3c2410camif_v4l2_driver);
		return err;
	}	
	
	/* Register the device driver structure. */
	err = platform_driver_register(&s3c2410camif_v4l2_driver);
	if (err != 0) {
		platform_device_unregister(&s3c2410camif_v4l2_devices);
		pr_debug("camif_init: driver_register failed.\n");
		video_device_release(cam->video_dev);
		return err;
	}	
	
  /* register v4l device */
	if (video_register_device(cam->video_dev, VFL_TYPE_GRABBER, video_nr) == -1) 
	{
		platform_driver_unregister(&s3c2410camif_v4l2_driver);
		platform_device_unregister(&s3c2410camif_v4l2_devices);
		video_device_release(cam->video_dev);
		pr_debug("video_register_device failed\n");
		return -1;
	}
	
	/* set gpio-j to camera mode. */
	s3c2410_gpio_cfgpin(S3C2440_GPJ0, S3C2440_GPJ0_CAMDATA0);
	s3c2410_gpio_cfgpin(S3C2440_GPJ1, S3C2440_GPJ1_CAMDATA1);
	s3c2410_gpio_cfgpin(S3C2440_GPJ2, S3C2440_GPJ2_CAMDATA2);
	s3c2410_gpio_cfgpin(S3C2440_GPJ3, S3C2440_GPJ3_CAMDATA3);
	s3c2410_gpio_cfgpin(S3C2440_GPJ4, S3C2440_GPJ4_CAMDATA4);
	s3c2410_gpio_cfgpin(S3C2440_GPJ5, S3C2440_GPJ5_CAMDATA5);
	s3c2410_gpio_cfgpin(S3C2440_GPJ6, S3C2440_GPJ6_CAMDATA6);
	s3c2410_gpio_cfgpin(S3C2440_GPJ7, S3C2440_GPJ7_CAMDATA7);
	s3c2410_gpio_cfgpin(S3C2440_GPJ8, S3C2440_GPJ8_CAMPCLK);
	s3c2410_gpio_cfgpin(S3C2440_GPJ9, S3C2440_GPJ9_CAMVSYNC);
	s3c2410_gpio_cfgpin(S3C2440_GPJ10, S3C2440_GPJ10_CAMHREF);
	s3c2410_gpio_cfgpin(S3C2440_GPJ11, S3C2440_GPJ11_CAMCLKOUT);
	s3c2410_gpio_cfgpin(S3C2440_GPJ12, S3C2440_GPJ12_CAMRESET);

	/* init camera's virtual memory. */
	if (!request_mem_region((unsigned long)S3C2440_PA_CAMIF, S3C2440_SZ_CAMIF, CARD_NAME))
	{
		ret = -EBUSY;
		goto error1;
	}

	/* remap the virtual memory. */
	camif_base_addr = (unsigned long)ioremap_nocache((unsigned long)S3C2440_PA_CAMIF, S3C2440_SZ_CAMIF);
	if (camif_base_addr == (unsigned long)NULL)
	{
		ret = -EBUSY;
		goto error2;
	}

	/* init camera clock. */
	cam->clk = clk_get(NULL, "camif");
	if (IS_ERR(cam->clk))
	{
		ret = -ENOENT;
		goto error3;
	}
	clk_enable(cam->clk);

	camif_upll_clk = clk_get(NULL, "camif-upll");
	clk_set_rate(camif_upll_clk, 24000000);//24000000
	mdelay(100);
	
	//sccb error!
	sccb_init();
	hw_reset_camif();
	
	has_ov9650 = cam->sensor_op->init() >= 0;
	

	if(static_allocate)
	{
		cam->sensor_op->get_largest_format(&cam->sensor);
		cam->wndHsize = cam->sensor.width;
		cam->wndVsize = cam->sensor.height;
		
		cam->Ysize=   cam->wndHsize*cam->wndVsize;
		cam->CbCrsize=cam->wndHsize*cam->wndVsize/2;
	  //allocate memory
		
		if((ret=s3c2440camif_allocate_frame_buf(cam,S3C2440_FRAME_NUM))<0) 
			goto error4;
		
	} else {
		cam->sensor_op->get_format(&cam->sensor);
		cam->wndHsize = cam->sensor.width;
		cam->wndVsize = cam->sensor.height;
	}
	
	s3c2410_gpio_setpin(S3C2410_GPG(4), 1);
	
	return 0;
	
error4:
	clk_put(cam->clk);
error3:
	iounmap((void *)camif_base_addr);
error2:
	release_mem_region((unsigned long)S3C2440_PA_CAMIF, S3C2440_SZ_CAMIF);
	
error1:
	return ret;
}

/*
 * camif_cleanup()
 */
static void __exit camif_cleanup(void)
{
	s3c2440camif_dev *pcam=&camera;

	if(static_allocate)
		s3c2440camif_deallocate_frame_buf(pcam,S3C2440_FRAME_NUM);
	
	video_unregister_device(pcam->video_dev);
	platform_driver_unregister(&s3c2410camif_v4l2_driver);
	platform_device_unregister(&s3c2410camif_v4l2_devices);
				
	//sccb_cleanup();
	s3c2440camif_sensor_if.cleanup();
	sccb_cleanup();

	clk_put(pcam->clk);

	iounmap((void *)camif_base_addr);
	release_mem_region((unsigned long)S3C2440_PA_CAMIF, S3C2440_SZ_CAMIF);

	printk(KERN_ALERT"s3c2440camif: module removed\n");
}


//module_param(video_nr, int, -1);

module_init(camif_init);
module_exit(camif_cleanup);

