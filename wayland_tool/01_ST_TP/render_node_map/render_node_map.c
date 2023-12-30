// atCROSSLEVEL 2012 ( http://atcrosslevel.de )
// released under zlib/libpng license
// kms.cc
// kms tutorial test

// Agressively stripped to the bare miniumum and repurposed for
// DRM testing by Damian Hobson-Garcia (dhobsong@igel.co.jp)

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <gbm.h>

#include <xf86drm.h>

#define XRES 800
#define YRES 600
#define BPP  32

int main(int argc,char* argv[])
{
	int fd;					//drm device handle
	uint32_t* front;			//pointer to memory mirror of framebuffer
	struct gbm_device *gbm;
	struct gbm_bo *bo;
	uint32_t handle;
	char *dev_name;

	if (argc > 1) {
		dev_name = argv[1];
	} else {
		//open default dri device
		dev_name = "/dev/dri/renderD128";
	}
	fd = open(dev_name,O_RDWR);
	if (fd<=0) { printf("Couldn't open %s\n", dev_name); exit(0); }

	gbm = gbm_create_device(fd);
	if (!gbm) { printf("Couldn't create gbm device\n"); close(fd); exit(0); }

	bo = gbm_bo_create(gbm, XRES, YRES, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);

	handle = gbm_bo_get_handle(bo).u32;

	int pfd;
	drmPrimeHandleToFD(fd, handle, DRM_CLOEXEC | DRM_RDWR, &pfd);

	int flags = fcntl(pfd, F_GETFL);

	printf("Access flags: %x\n", flags);

	int size = gbm_bo_get_stride(bo) * gbm_bo_get_height(bo);

	//draw testpattern
	front = mmap(0, size, PROT_READ | PROT_WRITE,  MAP_SHARED, pfd, 0);
	if (front==MAP_FAILED) {
		printf("Could not map dumb buffer!\n");
		exit(0);
	}

	for (int i=0;i<YRES;++i) {
		for (int j=0;j<XRES;++j) {
			front[i*XRES+j] = (i*j) | 0xff000000;
		}
	}

	munmap(front, size);
	close(pfd);
	gbm_bo_destroy(bo);
	close(fd);

	printf("Done:\n");

	return 0;
}
