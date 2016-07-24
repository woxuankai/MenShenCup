#include "videodevice.h"
#include "common.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/types.h>
#include <linux/videodev2.h>

#define FILE_VIDEO 	"/dev/video0"


#include <unistd.h>


#define TRUE true
#define FALSE false



VideoDevice::VideoDevice(std::string dev_name)
{
    this->dev_name = dev_name;
    this->fd = -1;
    this->buffers = NULL;
    this->n_buffers = 0;
    this->index = -1;

    if(open_device() == false)
    {
        close_device();
    }

    if(init_device() == false)
    {
        close_device();
    }

    if(start_capturing() == false)
    {
        stop_capturing();
        close_device();
    }
}

VideoDevice::~VideoDevice()
{
    if(stop_capturing() == false)
    {
    }
    if(uninit_device() == false)
    {
    }
    if(close_device() == false)
    {
    }
}
int VideoDevice::open_device()
{
    fd = open(FILE_VIDEO,O_RDWR);
    if(fd == -1)
    {
        printf("Error opening V4L interface\n");
        return false;
    }
    return true;
}

int VideoDevice::close_device()
{
    if(close(fd) == false)
    {
        printf("Error closing V4L interface\n");
        return false;
    }
    return true;
}

int VideoDevice::init_device()
{
    v4l2_capability cap;
    v4l2_format fmt;
    v4l2_streamparm setfps;

    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        printf("Error opening device %s: unable to query device.\n",FILE_VIDEO);
        return false;
    }
    else
    {
        printf("driver:\t\t%s\n",cap.driver);
        printf("card:\t\t%s\n",cap.card);
        printf("bus_info:\t%s\n",cap.bus_info);
        printf("version:\t%d\n",cap.version);
        printf("capabili ties:\t%x\n",cap.capabilities);

        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE)
        {
            printf("Device %s: supports capture.\n",FILE_VIDEO);
        }

        if ((cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING)
        {
            printf("Device %s: supports streaming.\n",FILE_VIDEO);
        }
    }

    //set fmt
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.height = IMG_HEIGTH;
    fmt.fmt.pix.width = IMG_WIDTH;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if(ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
            printf("Unable to set format\n");
            return false;
    }
    if(ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
    {
            printf("Unable to get format\n");
            return false;
    }

    if(fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
        printf("failed to set pixelformat");
     printf("fmt.type:\t\t%d\n",fmt.type);
     printf("pix.pixelformat:\t%c%c%c%c\n",fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat >> 8) & 0xFF,(fmt.fmt.pix.pixelformat >> 16) & 0xFF, (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
     printf("pix.height:\t\t%d\n",fmt.fmt.pix.height);
     printf("pix.width:\t\t%d\n",fmt.fmt.pix.width);
     printf("pix.field:\t\t%d\n",fmt.fmt.pix.field);

    //set fps
    setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps.parm.capture.timeperframe.numerator = 1;
    setfps.parm.capture.timeperframe.denominator = 45;

    if(ioctl(fd, VIDIOC_S_PARM, &setfps) == -1)
    {
        printf("Unable to set frame rate\n");
        return false;
    }
    else
    {
        printf("set fps OK!\n");
    }

    if(ioctl(fd, VIDIOC_G_PARM, &setfps) == -1)
    {
        printf("Unable to get frame rate\n");
        return false;
    }
    else
    {
        printf("get fps OK!\n");
        printf("timeperframe.numerator:\t%d\n",setfps.parm.capture.timeperframe.numerator);
        printf("timeperframe.denominator:\t%d\n",setfps.parm.capture.timeperframe.denominator);

    }

    //mmap
    if(init_mmap() == false )
    {
        printf("cannot mmap!\n");
        return FALSE;

    }

    return true;
}

int VideoDevice::init_mmap()
{
    v4l2_requestbuffers req;

    req.count = 2;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if(ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
    {
        return FALSE;
    }

    if(req.count < 2)
    {
        return FALSE;
    }

    buffers = (buffer*)calloc(req.count, sizeof(*buffers));

    if(!buffers)
    {
        return FALSE;
    }

    for(n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if(ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            return FALSE;
        }

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
                mmap(NULL, // start anywhere// allocate RAM*4
                     buf.length,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     fd, buf.m.offset);

        if(MAP_FAILED == buffers[n_buffers].start)
        {

            return FALSE;
        }
    }
    return TRUE;

}

int VideoDevice::start_capturing()
{
    unsigned int i;
    for(i = 0; i < n_buffers; ++i)
    {
        v4l2_buffer buf;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory =V4L2_MEMORY_MMAP;
        buf.index = i;
        if(-1 == ioctl(fd, VIDIOC_QBUF, &buf))
        {
            return FALSE;
        }
    }

    v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(-1 == ioctl(fd, VIDIOC_STREAMON, &type))
    {
        return FALSE;
    }
    return TRUE;
}

int VideoDevice::stop_capturing()
{
    v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_STREAMOFF, &type) == -1)
    {
        return FALSE;
    }
    return TRUE;
}

int VideoDevice::uninit_device()
{
    unsigned int i;

    for(i = 0; i < n_buffers; ++i)
    {
        if(-1 == munmap(buffers[i].start, buffers[i].length))
        {
            printf("munmap error\n");
            return FALSE;
        }

    }

    delete buffers;
    return TRUE;
}

int VideoDevice::get_frame(unsigned char ** yuv_buffer_pointer, size_t* len)
{
    v4l2_buffer queue_buf;

    queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queue_buf.memory = V4L2_MEMORY_MMAP;

    if(ioctl(fd, VIDIOC_DQBUF, &queue_buf) == -1)
    {
        return FALSE;
    }

    *yuv_buffer_pointer = (unsigned char *)buffers[queue_buf.index].start;
    *len = buffers[queue_buf.index].length;
    index = queue_buf.index;

    return TRUE;

}

int VideoDevice::unget_frame()
{
    if(index != -1)
    {
        v4l2_buffer queue_buf;
        queue_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        queue_buf.memory = V4L2_MEMORY_MMAP;
        queue_buf.index = index;

        if(ioctl(fd, VIDIOC_QBUF, &queue_buf) == -1)
        {
            return FALSE;
        }
        return TRUE;
    }
    return FALSE;
}
