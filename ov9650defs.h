// register definitions for OmniVision OV9650 camera

#ifndef __OV_9550_defs_H__
#define __OV_9550_defs_H__


/*
 * Our nominal (default) frame rate.
 */
#define OV9650_FRAME_RATE 30

/*
 * The 9650 sits on i2c with ID 0x42
 */
#define OV9650_I2C_ADDR 0x60


#define OV9650_MANUFACT_ID	0x7FA2
#define OV9650_PRODUCT_ID	  0x9650

/* Registers */
#define REG_GAIN	0x00	/* Gain lower 8 bits (rest in vref) */
#define REG_BLUE	0x01	/* blue gain */
#define REG_RED		0x02	/* red gain */
#define REG_VREF	0x03	/* Pieces of GAIN, VSTART, VSTOP */
#define REG_COM1	0x04	/* Control 1 */
#define  COM1_CCIR656	  0x40  /* CCIR656 enable */
#define REG_BAVE	0x05	/* U/B Average level */
#define REG_GbAVE	0x06	/* Y/Gb Average level */
#define REG_GrAVE	0x07	/* Cr Average Level */
#define REG_RAVE	0x08	/* V/R Average level */
#define REG_COM2	0x09	/* Control 2 */
#define  COM2_SSLEEP	  0x10	/* Soft sleep mode */
#define REG_PID		0x0a	/* Product ID MSB */
#define REG_VER		0x0b	/* Product ID LSB */
#define REG_COM3	0x0c	/* Output byte swap, signals reassign */
#define  COM3_SWAP	  0x40	  /* Byte swap */
#define  COM3_VARIOPIXEL1 0x04
#define REG_COM4	0x0d	/* Vario Pixels  */
#define  COM4_VARIOPIXEL2 0x80

#define REG_COM5	0x0e	/* System clock options */
#define   COM5_SLAVE_MODE 0x10
#define   COM5_SYSTEMCLOCK48MHZ 0x80

#define REG_COM6	0x0f	/* HREF & ADBLC options */
#define REG_AEC 	0x10	/* More bits of AEC value */
#define REG_CLKRC	0x11	/* Clocl control */
#define   CLK_EXT	    0x40	  /* Use external clock directly */
#define   CLK_SCALE	  0x3f	  /* Mask for internal clock scale */
#define REG_COM7	0x12	/* SCCB reset, output format */
#define   COM7_RESET	  0x80	  /* Register reset */
#define   COM7_FMT_MASK	  0x38
#define   COM7_FMT_VGA	  0x00
#define	  COM7_FMT_CIF	  0x20	  /* CIF format */
#define   COM7_FMT_QVGA	  0x10	  /* QVGA format */
#define   COM7_FMT_QCIF	  0x08	  /* QCIF format */
#define	  COM7_RGB	  0x04	  /* bits 0 and 2 - RGB format */
#define	  COM7_YUV	  0x00	  /* YUV */
#define	  COM7_BAYER	  0x01	  /* Bayer format */
#define	  COM7_PBAYER	  0x05	  /* "Processed bayer" */
#define REG_COM8	0x13	/* AGC/AEC options */
#define   COM8_FASTAEC	  0x80	  /* Enable fast AGC/AEC */
#define   COM8_AECSTEP	  0x40	  /* Unlimited AEC step size */
#define   COM8_BFILT	  0x20	  /* Band filter enable */
#define   COM8_AGC	  0x04	  /* Auto gain enable */
#define   COM8_AWB	  0x02	  /* White balance enable */
#define   COM8_AEC	  0x01	  /* Auto exposure enable */
#define REG_COM9	0x14	/* Control 9  - gain ceiling */
#define REG_COM10	0x15	/* Slave mode, HREF vs HSYNC, signals negate */
#define   COM10_HSYNC	  0x40	  /* HSYNC instead of HREF */
#define   COM10_PCLK_HB	  0x20	  /* Suppress PCLK on horiz blank */
#define   COM10_HREF_REV  0x08	  /* Reverse HREF */
#define   COM10_VS_LEAD	  0x04	  /* VSYNC on clock leading edge */
#define   COM10_VS_NEG	  0x02	  /* VSYNC negative */
#define   COM10_HS_NEG	  0x01	  /* HSYNC negative */
#define REG_HSTART	0x17	/* Horiz start high bits */
#define REG_HSTOP	0x18	/* Horiz stop high bits */
#define REG_VSTART	0x19	/* Vert start high bits */
#define REG_VSTOP	0x1a	/* Vert stop high bits */
#define REG_PSHFT	0x1b	/* Pixel delay after HREF */
#define REG_MIDH	0x1c	/* Manuf. ID high */
#define REG_MIDL	0x1d	/* Manuf. ID low */
#define REG_MVFP	0x1e	/* Mirror / vflip */
#define   MVFP_MIRROR	  0x20	  /* Mirror image */
#define   MVFP_FLIP	  0x10	  /* Vertical flip */
#define REG_BOS		0x20	  /* B channel Offset  */
#define REG_GBOS	0x21	/* Gb channel Offset  */
#define REG_GROS	0x22	/* Gr channel Offset  */
#define REG_ROS		0x23	  /* R channel Offset  */
#define REG_AEW		0x24	/* AGC upper limit */
#define REG_AEB		0x25	/* AGC lower limit */
#define REG_VPT		0x26	/* AGC/AEC fast mode op region */
#define REG_BBIAS	0x27	/* B channel output bias */
#define REG_GBBIAS	0x28	/* Gb channel output bias */
#define REG_GRCOM	0x29	/* Analog BLC & regulator */
#define REG_EXHCH	0x2A	/* Dummy pixel insert MSB */
#define REG_EXHCL	0x2B	/* Dummy pixel insert LSB */
#define REG_RBIAS	0x2C	/* R channel output bias */
#define REG_ADVFL	0x2D	/* LSB of dummy pixel insert in vertical direction 1 bit = 1 line */
#define REG_ADVFH	0x2E	/* MSB of dummy pixel insert in vertical direction 1 bit = 1 line */
#define REG_YAVE	0x2F	/* Y/G channel average value */
#define REG_HSYST	0x30	/* HSYNC rising edge delay LSB*/
#define REG_HSYEN	0x31	/* HSYNC falling edge delay LSB*/
#define REG_HREF	0x32	/* HREF pieces */
#define REG_CHLF	0x33	/* reserved */
#define REG_ADC	  0x37	/* reserved */
#define REG_ACOM	0x38	/* reserved */
#define REG_OFCON	0x39	/* Power down register */
#define 	OFCON_PWRDN	0x08	/* Power down bit */

#define REG_TSLB	0x3a	/* YUVU format */
#define   TSLB_YUYV_MASK	0x0c	  /* UYVY or VYUY - see com13 */

#define REG_COM11	0x3b	/* Night mode, banding filter enable */
#define   COM11_NIGHT	  0x80	  /* NIght mode enable */
#define   COM11_NMFR	  0x60	  /* Two bit NM frame rate */
#define   COM11_BANING	0x01    /* banding filter */
#define REG_COM12	0x3c	/* HREF option, UV average */
#define   COM12_HREF	  0x80	  /* HREF always */
#define REG_COM13	0x3d	/* Gamma selection, Color matrix enable, UV delay*/
#define   COM13_GAMMA	  0x80	  /* Gamma enable */
#define	  COM13_UVSAT	  0x40	  /* UV saturation auto adjustment */
#define   COM13_UVSWAP	  0x01	  /* V before U - w/TSLB */
#define REG_COM14	0x3e	/* Edge enhancement options */
#define REG_EDGE	0x3f	/* Edge enhancement factor */
#define REG_COM15	0x40	/* Output range, RGB 555 vs 565 option */
#define   COM15_R10F0	  0x00	  /* Data range 10 to F0 */
#define	  COM15_R01FE	  0x80	  /*            01 to FE */
#define   COM15_R00FF	  0xc0	  /*            00 to FF */
#define   COM15_RGB565	  0x10	  /* RGB565 output */
#define   COM15_RGB555	  0x30	  /* RGB555 output */
#define   COM15_SWAPRB	  0x04	  /* Swap R&B */
#define REG_COM16	0x41	/* Color matrix coeff double option */
#define   COM16_AWBGAIN   0x08	  /* AWB gain enable */
#define REG_COM17	0x42	/* Single Frame out, Banding filter */

/*
 * This matrix defines how the colors are generated, must be
 * tweaked to adjust hue and saturation.
 *
 * Order: v-red, v-green, v-blue, u-red, u-green, u-blue
 *
 * They are nine-bit signed quantities, with the sign bit
 * stored in 0x58.  Sign for v-red is bit 0, and up from there.
 */
#define	REG_CMATRIX_BASE 0x4f
#define   CMATRIX_LEN 9
#define	REG_CMATRIX_MTX1 0x4f
#define	REG_CMATRIX_MTX2 0x50
#define	REG_CMATRIX_MTX3 0x51
#define	REG_CMATRIX_MTX4 0x52
#define	REG_CMATRIX_MTX5 0x53
#define	REG_CMATRIX_MTX6 0x54
#define	REG_CMATRIX_MTX7 0x55
#define	REG_CMATRIX_MTX8 0x56
#define	REG_CMATRIX_MTX9 0x57

#define REG_CMATRIX_SIGN 0x58

#define REG_LCC1	0x62	/* Lens Correction Option 1 */
#define REG_LCC2	0x63	/* Lens Correction Option 2 */
#define REG_LCC3	0x64	/* Lens Correction Option 3 */
#define REG_LCC4	0x65	/* Lens Correction Option 4 */
#define REG_LCC5	0x66	/* Lens Correction Option 5 */
#define  LCC5_LCC_ENABLE 0x01 /* enable lens correction */
#define  LCC5_LCC_COLOR 0x04 /* Use LCC for green and LCCFB for rest, or just LCC */

#define REG_MANU	0x67	/* Manual U value, when TSLB[4] is high */
#define REG_MANV	0x68	/* Manual V value, when TSLB[4] is high */

#define REG_HV	0x69	/* Manual banding filter MSB */
#define REG_MBD	0x6a	/* Manual banding filter value, when COM11[0] is high */

#define REG_GSP	0x6c	/* Gamma 1 */
#define   GSP_LEN 15
#define REG_GSP	0x6c	/* Gamma 1 */
#define   GST_LEN 14

#define REG_COM22	0x8c	/* Edge enhancement, denoising */
#define   COM22_WHTPCOR	  0x02	  /* White pixel correction enable */
#define   COM22_WHTPCOROPT	 0x01	  /* White pixel correction option */
#define   COM22_DENOISE	 0x10	  /* White pixel correction option */
#define REG_COM23	0x8d	/* Color bar test,color gain */

#define REG_DBLC1	0x8f	/* Digital BLC */
#define REG_DBLC_B	0x90	/* Digital BLC B chan offset */
#define REG_DBLC_R	0x91	/* Digital BLC R chan offset */
#define REG_DM_LNL	0x92	/* Dummy line low 8 bits */
#define REG_DM_LNH	0x93	/* Dummy line high 8 bits */


#define REG_LCCFB	0x9d	/* Lens Correction B chan */
#define REG_LCCFR	0x9e	/* Lens Correction R chan */

#define REG_DBLC_GB	0x9f	/* Digital BLC GB chan offset */
#define REG_DBLC_GR	0xa0	/* Digital BLC GR chan offset */

#define REG_AECHM	0xa1	/* Exposure Value - AEC MSB 6 bits */
#define REG_BD50ST	0xa2	/* Banding Filter Value 50Hz */
#define REG_BD60ST	0xa3	/* Banding Filter Value 60Hz */

#endif //__OV_9550_defs_H__