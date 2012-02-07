#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>


void show_usage(const char *prog)
{
	fprintf(stdout,"Usage:%s \n"
	               "Options: [--width w] [--height h>] [--device <dev>] [--exposure <exp>, set to -1 for auto] [--gain <g>, set to -1 for auto \n",prog);
	
}


void print_control(int dev_fd,int ctl,const char* name)
{
	struct v4l2_queryctrl queryctrl;
	struct v4l2_control control;

	memset (&queryctrl, 0, sizeof (queryctrl));
	memset (&control,0,sizeof(control));
	
	queryctrl.id = ctl;

	if (-1 == ioctl (dev_fd, VIDIOC_QUERYCTRL, &queryctrl)) {
		if (errno != EINVAL) {
			perror ("VIDIOC_QUERYCTRL");
			return;
		} else {
			printf ("%s is not supported\n",name);
			return;
		}
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		printf("%s is disabled\n",name);
	} else {
		control.id=ctl;
		if( !ioctl(dev_fd,VIDIOC_G_CTRL,&control))
		{
			printf("%s = min: %d max:%d value:%d\n",name,queryctrl.minimum,queryctrl.maximum,control.value);
		} else {
			printf("%s = min: %d max:%d value:unknown\n",name,queryctrl.minimum,queryctrl.maximum);
		}
	}
}

void get_capture_parameters(int dev_fd)
{
	print_control(dev_fd,V4L2_CID_EXPOSURE_AUTO,"Auto Exposure");
	print_control(dev_fd,V4L2_CID_AUTOGAIN,"Auto Gain");
	print_control(dev_fd,V4L2_CID_AUTO_WHITE_BALANCE,"Auto WB");
	print_control(dev_fd,V4L2_CID_GAIN,"Gain");
	print_control(dev_fd,V4L2_CID_EXPOSURE,"Exposure");
}

void set_control(int dev_fd,int ctrl,int val,const char *name)
{
	struct v4l2_control control;
	memset (&control,0,sizeof(control));	
	control.id = ctrl;
	control.value=val;
	
	if( ioctl(dev_fd,VIDIOC_S_CTRL,&control))
	{
		fprintf(stderr,"Failed to set control %s\n",name);
	}
}

int main(int argc,char **argv)
{
  const char *output_base;
	char *video_device=strdup("/dev/video0");
	
  int dev_fd;
  int buffer_width=1280;
  int buffer_height=1024;
	int set_size=0;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	char fourcc[5]={0,0,0,0,0};
	unsigned char **buffers;
  int i;
  struct timeval start;
	struct timeval finish;
	double fps;
	int verbose=0;
	int set_exp=0;
	int set_gain=0;
	int exposure=-1;
	int gain=-1;
	
  struct option long_options[] = { 
    {"verbose",  no_argument, &verbose, 1},
    {"quiet",    no_argument, &verbose, 0},
    {"width",   required_argument, 0, 'w'},
    {"height",  required_argument, 0, 'h'},
    {"device",  required_argument, 0, 'd'},
		{"exposure",required_argument, 0, 'e'},
		{"gain",    required_argument, 0, 'g'},
    {0, 0, 0, 0}
  };
    
  int c;
  for (;;)
  {
    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long (argc, argv, "w:h:d:c:e:g:s:", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c)
    {
    case 0:
      break;
    case 'w':
      buffer_width = atoi(optarg);
			set_size=1;
      break;
    case 'h':
      buffer_height = atoi(optarg);
			set_size=1;
      break;
		case 'd':
			video_device=strdup(optarg);
			break;
		case 'g':
			set_gain=1;
			gain=atoi(optarg);
			break;
		case 'e':
			set_exp=1;
			exposure=atoi(optarg);
			break;
    case '?':
      /* getopt_long already printed an error message. */
    default:
      show_usage (argv[0]);
      return 1;
    }
  }

  if ((dev_fd = open( video_device, O_RDWR)) == -1)
    perror("ERROR opening camera interface\n");  
  
	if (0 != ioctl(dev_fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s is no V4L2 device\n",video_device);
			return 1;
		} else {
			perror ("VIDIOC_QUERYCAP");
			return 1;
		}
	}
	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "%s is no video capture device\n",video_device);
		return 1;
	}	
	
	memset(&fmt,sizeof(fmt),0);
	
	fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	
	if(set_size)
	{
		fmt.fmt.pix.width=buffer_width;
		fmt.fmt.pix.height=buffer_height;
		
		if (0 != ioctl (dev_fd, VIDIOC_S_FMT, &fmt))
		{
			perror ("VIDIOC_G_FMT");
			return 1;
		}
	} else {
		fmt.fmt.pix.width=0;
		fmt.fmt.pix.height=0;
		
		if (0 != ioctl (dev_fd, VIDIOC_G_FMT, &fmt))
		{
			perror ("VIDIOC_G_FMT");
			return 1;
		}
	}
	memmove(fourcc,&fmt.fmt.pix.pixelformat,4);
	
	printf("Capture format: %d x %d FOURCC: %4s framesize: %d\n",
				fmt.fmt.pix.width,
				fmt.fmt.pix.height,
				fourcc,
				fmt.fmt.pix.sizeimage);
	
	if(set_gain)
	{
		if(gain>=0)
		{
			set_control(dev_fd,V4L2_CID_AUTOGAIN,0,"Autogain");
			set_control(dev_fd,V4L2_CID_GAIN,gain,"Gain");
		} else {
			set_control(dev_fd,V4L2_CID_AUTOGAIN,1,"Autogain");
		}
	}
	
	if(set_exp)
	{
		if(exposure>=0)
		{
			set_control(dev_fd,V4L2_CID_EXPOSURE_AUTO,0,"Auto exposure");
			set_control(dev_fd,V4L2_CID_EXPOSURE,exposure,"Exposure");
		} else {
			set_control(dev_fd,V4L2_CID_EXPOSURE_AUTO,1,"Auto exposure");
		}
	}

	get_capture_parameters(dev_fd);

	return 0;
}
