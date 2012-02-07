#ifndef __S3C2440CAMIF_H__
#define __S3C2440CAMIF_H__

#include <asm/uaccess.h>
#include <linux/list.h>
#include <media/v4l2-dev.h>


#define MIN_C_WIDTH		32
#define MAX_C_WIDTH		1280
#define MIN_C_HEIGHT	48
#define MAX_C_HEIGHT	1024

#define MIN_P_WIDTH		32
#define MAX_P_WIDTH		640
#define MIN_P_HEIGHT	48
#define MAX_P_HEIGHT	480

enum
{
	CAMIF_BUFF_INVALID  = 0,
	CAMIF_BUFF_RGB565   = 1,
	CAMIF_BUFF_RGB24    = 2,
	CAMIF_BUFF_YCbCr420 = 3,
	CAMIF_BUFF_YCbCr422 = 4
};


typedef struct  {
	volatile int state;
	void * Y_virt_base, *CB_virt_base,*CR_virt_base;
	u32 Y_order,CBCR_order;

	//struct v4l2_buffer buffer;
	//struct list_head queue;
	int index;
} s3c2440camif_v4l_frame;

typedef struct 
{
	char *name;
	u32   fourcc;          /* v4l2 format id */
	int   depth;
} s3c2410camif_fmt;


#define S3C2440_FRAME_NUM 4


/* for s3c2440camif_dev->state field. */
enum
{
	CAMIF_STATE_FREE       = 0,		// not openned
	CAMIF_STATE_READY      = 1,		// openned, but standby
	CAMIF_STATE_PREVIEWING = 2,	// in previewing
	CAMIF_STATE_CODECING   = 3	// in capturing
};

/* for s3c2440camif_dev->cmdcode field. */
enum
{
	CAMIF_CMD_NONE	= 0,
	CAMIF_CMD_SFMT	= 1<<0,		// source image format changed.
	CAMIF_CMD_WND	=   1<<1,		// window offset changed.
	CAMIF_CMD_ZOOM	= 1<<2,		// zoom picture in/out
	CAMIF_CMD_TFMT	= 1<<3,		// target image format changed.
	CAMIF_CMD_P2C	=   1<<4,		// need camif switches from p-path to c-path
	CAMIF_CMD_C2P	=   1<<5,		// neet camif switches from c-path to p-path

	CAMIF_CMD_STOP	= 1<<16		// stop capture
};

/* external sensor configuration parameters */
typedef struct {
	u16 width;
	u16 height;
	u32 pixel_fmt;
} s3c2440camif_sensor_config;

/* Sensor control function TODO; implement this for 9650 sensor*/
typedef struct  {
/*int __init sccb_init(void);
void __exit sccb_cleanup(void);
int __init s3c2440_ov9650_init(void);
void __exit s3c2440_ov9650_cleanup(void);*/
	int (*init)(void);
	int (*cleanup)(void);
  int (*set_format)(s3c2440camif_sensor_config* cfg);
  int (*get_format)(s3c2440camif_sensor_config* cfg);
	int (*get_largest_format)(s3c2440camif_sensor_config* cfg);
	int (*poweron)(void);
	int (*poweroff)(void);
	
	int (*get_auto_exposure)(void);
	int (*get_auto_gain)(void);
	int (*get_auto_wb)(void);
	int (*get_exposure)(void);
	int (*get_gain)(void);
	
	int (*set_auto_exposure)(int);
	int (*set_auto_gain)(int);
	int (*set_auto_wb)(int);
	int (*set_exposure)(int);
	int (*set_gain)(int);
} s3c2440camif_sensor_operations;

/* main s3c2440 camif structure. */
typedef struct 
{
	struct video_device *video_dev;
	
	/* streaming */
	struct list_head ready_q;
	struct list_head done_q;
	struct list_head working_q;
	
	/* v4l2 format */
	struct v4l2_format v2f;
	/* buffer sizes */
  u32 Ysize;
	u32 CbCrsize;
	
	/* streaming */
	//struct v4l2_streamparm streamparm;
  /* standart */
	//struct v4l2_standard standard;
	
	/* crop TODO: implement this*/
	/*struct v4l2_rect crop_bounds;
	struct v4l2_rect crop_defrect;
	struct v4l2_rect crop_current;*/

	/* hardware clock. */
	struct clk * clk;

	/* reference count. */
	struct mutex rcmutex;
	int rc;
	
	s3c2440camif_v4l_frame frame[S3C2440_FRAME_NUM];
	
	/* the input images's format select. */
	int input;

	/* source(input) image size. */
	//u32 srcHsize;
	//u32 srcVsize;

	/* windowed image size. */
	u32 wndHsize;
	u32 wndVsize;
	
	s3c2440camif_sensor_config sensor; //sensor parameters
	s3c2440camif_sensor_operations *sensor_op; //sensor operations

	/* codec-path target(output) image size. */
	u32 coTargetHsize;
	u32 coTargetVsize;

	/* preview-path target(preview) image size. */
	u32 preTargetHsize;
	u32 preTargetVsize;
	
	bool bypass_mode; /* scaling hardware is bypassed */
	
	/* the camera interface state. */
	int state;	// CMAIF_STATE_FREE, CAMIF_STATE_PREVIEWING, CAMIF_STATE_CAPTURING.
  
  /* last captured frame */
  volatile int last_frame;

	/* for executing camif commands. */
	int cmdcode;				// command code, CAMIF_CMD_START, CAMIF_CMD_CFG, etc.
	wait_queue_head_t cmdqueue;	// wait queue for waiting untile command completed (if in preview or in capturing).
	wait_queue_head_t cap_queue; //wait queue for waiting captured frame
	
	
	
} s3c2440camif_dev;

#define S3C244X_CAMIFREG(x) ((x) + camif_base_addr)

/* CAMIF control registers */
#define S3C244X_CISRCFMT		S3C244X_CAMIFREG(0x00)
#define S3C244X_CIWDOFST		S3C244X_CAMIFREG(0x04)
#define S3C244X_CIGCTRL			S3C244X_CAMIFREG(0x08)
#define S3C244X_CICOYSA1		S3C244X_CAMIFREG(0x18)
#define S3C244X_CICOYSA2		S3C244X_CAMIFREG(0x1C)
#define S3C244X_CICOYSA3		S3C244X_CAMIFREG(0x20)
#define S3C244X_CICOYSA4		S3C244X_CAMIFREG(0x24)
#define S3C244X_CICOCBSA1		S3C244X_CAMIFREG(0x28)
#define S3C244X_CICOCBSA2		S3C244X_CAMIFREG(0x2C)
#define S3C244X_CICOCBSA3		S3C244X_CAMIFREG(0x30)
#define S3C244X_CICOCBSA4		S3C244X_CAMIFREG(0x34)
#define S3C244X_CICOCRSA1		S3C244X_CAMIFREG(0x38)
#define S3C244X_CICOCRSA2		S3C244X_CAMIFREG(0x3C)
#define S3C244X_CICOCRSA3		S3C244X_CAMIFREG(0x40)
#define S3C244X_CICOCRSA4		S3C244X_CAMIFREG(0x44)
#define S3C244X_CICOTRGFMT		S3C244X_CAMIFREG(0x48)
#define S3C244X_CICOCTRL		S3C244X_CAMIFREG(0x4C)
#define S3C244X_CICOSCPRERATIO	S3C244X_CAMIFREG(0x50)
#define S3C244X_CICOSCPREDST	S3C244X_CAMIFREG(0x54)
#define S3C244X_CICOSCCTRL		S3C244X_CAMIFREG(0x58)
#define S3C244X_CICOTAREA		S3C244X_CAMIFREG(0x5C)
#define S3C244X_CICOSTATUS		S3C244X_CAMIFREG(0x64)
#define S3C244X_CIPRCLRSA1		S3C244X_CAMIFREG(0x6C)
#define S3C244X_CIPRCLRSA2		S3C244X_CAMIFREG(0x70)
#define S3C244X_CIPRCLRSA3		S3C244X_CAMIFREG(0x74)
#define S3C244X_CIPRCLRSA4		S3C244X_CAMIFREG(0x78)
#define S3C244X_CIPRTRGFMT		S3C244X_CAMIFREG(0x7C)
#define S3C244X_CIPRCTRL		S3C244X_CAMIFREG(0x80)
#define S3C244X_CIPRSCPRERATIO	S3C244X_CAMIFREG(0x84)
#define S3C244X_CIPRSCPREDST	S3C244X_CAMIFREG(0x88)
#define S3C244X_CIPRSCCTRL		S3C244X_CAMIFREG(0x8C)
#define S3C244X_CIPRTAREA		S3C244X_CAMIFREG(0x90)
#define S3C244X_CIPRSTATUS		S3C244X_CAMIFREG(0x98)
#define S3C244X_CIIMGCPT		S3C244X_CAMIFREG(0xA0)

#endif
