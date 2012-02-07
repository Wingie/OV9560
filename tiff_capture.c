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
#include <tiffio.h>
#include <getopt.h>
#include <poll.h>

void show_usage(const char *prog)
{
	fprintf(stdout,"Usage:%s <output_base>\n"
	               "Options: [--width w] [--height h> [--count c] [--device <dev>] [--compress] [--verbose] [--exposure <exp>, set to -1 for auto] [--gain <g>, set to -1 for auto --skip <n> capture n dummy frames and don't store them]\n",prog);
	
}

int save_tiff_image(const char *file,int width,int height,uint8_t *buf,int compress)
{
	int err=0;
	TIFF* tif = TIFFOpen(file, "w");
	int y;
	
	if(!tif)
	{
		fprintf(stderr,"Can't open file %s for writing\n",file);
		return -1;
	}
	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK); // PHOTOMETRIC_MINISBLACK, PHOTOMETRIC_RGB , PHOTOMETRIC_YCBCR , PHOTOMETRIC_MINISWHITE,  PHOTOMETRIC_PALETTE
	
	TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE,8);
	TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL,1);
	//TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT,SAMPLEFORMAT_UINT); //SAMPLEFORMAT_IEEEFP
	if(compress)
	{
		TIFFSetField(tif, TIFFTAG_COMPRESSION,COMPRESSION_DEFLATE);//COMPRESSION_LZW,COMPRESSION_DEFLATE
		//TIFFSetField(tif, TIFFTAG_PREDICTOR, 2 );
	}
	
	//TIFFSetField(tif, TIFFTAG_COLORMAP, red, green, blue);
	TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	//TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, comment);
	
	for(y=0;y<height;y++)
	{
			if((err=TIFFWriteScanline(tif,(tdata_t)(buf+width*y),y,0))==-1)
					break;
	}
	
	TIFFClose(tif);
	
	if(err==-1)
			fprintf(stderr,"Error writing to %s file\n",file);
	return err;
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
	
  int fcount=1;
	int compress=0;
  int dev_fd;
  int buffer_width=1280;
  int buffer_height=1024;
  int buffer_size;
  int Ysize,CbCrsize;
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
	int exposure=0;
	int gain=0;
	int skip=0;
	struct pollfd wait_fd;
	struct timespec wait_timeout;
	
  struct option long_options[] = { 
    {"verbose",  no_argument, &verbose, 1},
    {"quiet",    no_argument, &verbose, 0},
		{"compress", no_argument, &compress, 1},
		{"nocompress", no_argument, &compress, 0},
    {"width",   required_argument, 0, 'w'},
    {"height",  required_argument, 0, 'h'},
    {"device",  required_argument, 0, 'd'},
		{"count",   required_argument, 0, 'c'},
		{"exposure",required_argument, 0, 'e'},
		{"gain",    required_argument, 0, 'g'},
		{"skip",    required_argument, 0, 's'},
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
      break;
    case 'h':
      buffer_height = atoi(optarg);
      break;
		case 'd':
			video_device=strdup(optarg);
			break;
		case 'c':
			fcount=atoi(optarg);
			break;
		case 'g':
			set_gain=1;
			gain=atoi(optarg);
			break;
		case 'e':
			set_exp=1;
			exposure=atoi(optarg);
			break;
		case 's':
			skip=atoi(optarg);
			break;
    case '?':
      /* getopt_long already printed an error message. */
    default:
      show_usage (argv[0]);
      return 1;
    }
  }

  if((argc - optind)<1)
  {
    show_usage(argv[0]);
    return 1;
  }
  output_base=argv[optind];
  
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
	
	if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
		fprintf (stderr, "%s does not support read i/o\n", video_device);
		return 1;
	}	
	
	memset(&fmt,sizeof(fmt),0);
	
	fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width=buffer_width;
	fmt.fmt.pix.height=buffer_height;
	
	if (0 != ioctl (dev_fd, VIDIOC_S_FMT, &fmt))
	{
		perror ("VIDIOC_S_FMT");
		return 1;
	}
	
	buffer_width=  fmt.fmt.pix.width;
	buffer_height= fmt.fmt.pix.height;
	memmove(fourcc,&fmt.fmt.pix.pixelformat,4);
	
	if(verbose) 
	{
		printf("Goint to capture frames %d x %d format: %4s framesize: %d\n",
				 fmt.fmt.pix.width,
				 fmt.fmt.pix.height,
				 fourcc,
				 fmt.fmt.pix.sizeimage);
				 
	}
	
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

	if(verbose)
		get_capture_parameters(dev_fd);

  Ysize=buffer_width*buffer_height;
  CbCrsize=buffer_width*buffer_height/2;
  buffer_size=Ysize; //fmt.fmt.pix.sizeimage; //YCbCr422
  
	buffers=malloc(sizeof(unsigned char *)*fcount);
	
	if(!buffers)
	{
		perror("Can't allocate buffer!\n");
		return 1;
	}
	
	//try to allocate memory for all buffers
	for(i=0;i<fcount;i++)
	{
		if((buffers[i]=malloc(buffer_size))==NULL)
			break;
	}
	
	if(i==0) //didn't allocate any buffer
	{
		perror("Can't allocate single buffer!\n");
		return 1;
	}
	
	fcount=i;
  //printf("Enough memory to capture %d images to %s\n",fcount,output_base);

	if(skip && verbose)
		printf("Skipping %d frames\n",skip);
	wait_fd.fd=dev_fd;
	wait_fd.events=POLLIN;
	wait_fd.revents=0;
	
	for(i=0;i<skip;i++) //skipping dummy frames
	{
		int rd=0;
		//poll(&wait_fd,1,100);
		if((rd=read(dev_fd, buffers[0], buffer_size))<buffer_size)
    {
      fprintf(stderr,"Expected %d got %d!\n",buffer_size,rd);
      continue;
    }
		if(verbose) 
		{	
			printf("s");
			fflush(stdout);
		}
	}
	
	gettimeofday(&start,NULL);
  for(i=0;i<fcount;i++)
  {
    int rd=0;
    
    if((rd=read(dev_fd, buffers[i], buffer_size))<buffer_size)
    {
      fprintf(stderr,"Expected %d got %d!\n",buffer_size,rd);
      continue;
    }
		if(verbose) 
		{	
			printf("*");
			fflush(stdout);
		}
	}
	gettimeofday(&finish,NULL);
  close(dev_fd);
	fps=(double)(fcount)/((finish.tv_sec-start.tv_sec)+(finish.tv_usec-start.tv_usec)/1e6);
	
	if(verbose) 
		printf("\n%f fps, Saving collectected frames...\n",fps);
	
	for(i=0;i<fcount;i++)
	{
    FILE *img=NULL;
    char tmp[1024];
		
    sprintf(tmp,"%s_%03d.tif",output_base,i);
    
    if((img=fopen(tmp,"wb"))==NULL)
    {
      fprintf(stderr,"Can't open file %s for writing\n",tmp);
      break;
    } 
    save_tiff_image(tmp,buffer_width,buffer_height,buffers[i],compress);
    
    /*
    sprintf(tmp,"%s_%03d_CbCr.ppm",output_base,i);
    
    if((img=fopen(tmp,"wb"))==NULL)
    {
      fprintf(stderr,"Can't open file %s for writing\n",tmp);
      break;
    } 
    
    fprintf(img,"P5\n%d %d\n255\n",buffer_width/2,buffer_height*2);
    if(fwrite(buffer+Ysize,1,CbCrsize*2,img)<CbCrsize*2)
    {
      fprintf(stderr,"Can't write to file %s\n",tmp);
      fclose(img);
      break;
    }
    fclose(img);*/
    
		if(verbose)
			printf("%d\t",i);
    fflush(stdout);
		free(buffers[i]);
  }
	
	if(verbose)
		printf("\n");
  free(buffers);
	return 0;
}
