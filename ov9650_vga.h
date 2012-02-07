static struct ov9650_reg  ov9650_vga[] = {
  {REG_COM7,0x80}, /*  SCCB reset, output format  */
  {REG_COM7,0x80}, /*  SCCB reset, output format  */
  {REG_OFCON,0x43}, /*  Power down register  */
  {REG_ACOM,0x12}, /*  reserved  */
  {REG_ADC,0x91}, /*  reserved  */
  {REG_COM5,0x00}, /*  System clock options  */
//
  {REG_MVFP,0x0c}, /*  Mirror / vflip  */
  {REG_BLUE,0x80}, /*  blue gain  */
  {REG_RED,0x80}, /*  red gain  */
  {REG_GAIN,0x00}, /*  Gain lower 8 bits (rest in vref)  */
  {REG_AEC,0xf0}, /*  More bits of AEC value  */
  {REG_COM1,0x00}, /*  Control 1  */
  {REG_COM3,0x04}, /*  Output byte swap, signals reassign  */
  {REG_COM4,0x80}, /*  Vario Pixels   */
	
  {REG_CLKRC,0x80}, /*  Clock control, try to run at 24Mhz  */
	
  {REG_COM7,0x40}, /*  SCCB reset, output format  */
	
	{REG_COM2,0x01}, /*  Output drive  */
	
  {REG_COM9,0x2e}, /*  Control 9  - gain ceiling  */
  {REG_COM10,0x00}, /*  Slave mode, HREF vs HSYNC, signals negate  */
	
  {REG_HSTART,0x1d+8}, /*  Horiz start high bits  */
  {REG_HSTOP,0xbd+8}, /*  Horiz stop high bits  */
  {REG_HREF,0xbf}, /*  HREF pieces  */
	
  {REG_VSTART,0x00}, /*  Vert start high bits  */
  {REG_VSTOP,0x80}, /*  Vert stop high bits  */
  {REG_VREF,0x12}, /*  Pieces of GAIN, VSTART, VSTOP  */
	
  {REG_EDGE,0xa6}, /*  Edge enhancement factor  */
  {REG_COM16,0x02}, /*  Color matrix coeff double option  */
  {REG_COM17,0x08}, /*  Single Frame out, Banding filter  */
  {REG_PSHFT,0x00}, /*  Pixel delay after HREF  */
  {0x16,0x06},
  {REG_CHLF,0xc0}, /*  reserved  */
  {0x34,0xbf},
  {0xa8,0x80},
  {0x96,0x04},
  {REG_TSLB,0x00}, /*  YUVU format  */
  {0x8e,0x00},
  {REG_COM12,0x77}, /*  HREF option, UV average  */

  {0x8b,0x06},
  {0x35,0x91},
  {0x94,0x88},
  {0x95,0x88},
  {REG_COM15,0xc1}, /*  Output range, RGB 555 vs 565 option  */

  {REG_GRCOM,0x3f}, /*  Analog BLC & regulator  */
  {REG_COM6,0x42|1}, /*  HREF & ADBLC options  */
  {REG_COM8,0xe5}, /*  AGC/AEC options  */
  {REG_COM13,0x90}, /*  Gamma selection, Color matrix enable, UV delay */
  {REG_HV,0x80}, /*  Manual banding filter MSB  */

  {0x5c,0x96},
  {0x5d,0x96},
  {0x5e,0x10},
  {0x59,0xeb},
  {0x5a,0x9c},
  {0x5b,0x55},
  {0x43,0xf0},
  {0x44,0x10},
  {0x45,0x55},
  {0x46,0x86},
  {0x47,0x64},
  {0x48,0x86},
  {0x5f,0xe0},
  {0x60,0x8c},
  {0x61,0x20},
  {0xa5,0xd9},
  {0xa4,0x74},
  {REG_COM23,0x02}, /*  Color bar test,color gain  */
  {REG_COM8,(1<<5)}, /*  AGC/AEC options  */
  {0x4f,0x3a},
//0xe7
  {0x50,0x3d},
  {0x51,0x03},
  {0x52,0x12},
  {0x53,0x26},
  {0x54,0x38},
  {0x55,0x40},
  {0x56,0x40},
  {0x57,0x40},
  {0x58,0x0d},
  {REG_COM22,0x23}, /*  Edge enhancement, denoising  */
  {REG_COM14,0x02}, /*  Edge enhancement options  */
  {0xa9,0xb8},
  {0xaa,0x92},
  {0xab,0x0a},
  {REG_DBLC1,0xdf}, /*  Digital BLC  */
  {REG_DBLC_B,0x00}, /*  Digital BLC B chan offset  */
  {REG_DBLC_R,0x00}, /*  Digital BLC R chan offset  */
  {REG_DBLC_GB,0x00}, /*  Digital BLC GB chan offset  */
  {REG_TSLB,0x0c}, /*  YUVU format  */
  {REG_AEW,0x70}, /*  AGC upper limit  */
  {REG_AEB,0x64}, /*  AGC lower limit  */
  {REG_VPT,0xc3}, /*  AGC/AEC fast mode op region  */
  {REG_EXHCH,0x00}, /*  Dummy pixel insert MSB  */
  {REG_EXHCL,0x00}, /*  Dummy pixel insert LSB  */
  {REG_COM11,(1<<7)|(3<<5)}, /*  Night mode, banding filter enable  */
//0x2a,0x12 0x19
  {REG_GSP,0x40}, /*  Gamma 1  */
  {0x6d,0x30},
  {0x6e,0x4b},
  {0x6f,0x60},

  {0x70,0x70},
  {0x71,0x70},
  {0x72,0x70},
  {0x73,0x70},
  {0x74,0x60},
  {0x75,0x60},
  {0x76,0x50},
  {0x77,0x48},
  {0x78,0x3a},
  {0x79,0x2e},
  {0x7a,0x28},
  {0x7b,0x22},
  {0x7c,0x04},
  {0x7d,0x07},
  {0x7e,0x10},
  {0x7f,0x28},
  {0x80,0x36},
  {0x81,0x44},
  {0x82,0x52},
  {0x83,0x60},
  {0x84,0x6c},
  {0x85,0x78},
  {0x86,0x8c},
  {0x87,0x9e},
  {0x88,0xbb},
  {0x89,0xd2},
  {0x8a,0xe6},
  {REG_MBD,0x41}, /*  Manual banding filter value, when COM11[0] is high  */
  {REG_LCC5,0x00}, /*  Lens Correction Option 5  */
  {REG_COM14,0x00}, /*  Edge enhancement options  */
  {REG_EDGE,0xa4}/*  Edge enhancement factor  */
};
