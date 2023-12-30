/*
 * simple-libgbm
 *
 * This program is used for testing libgbm with kms backend.
 *
 * ----------------
 * How to build
 * ex)
 * $ source /opt/poky/2.1.2/environment-setup-aarch64-poky-linux
 * $ autoreconf -vif
 * $ ./configure ${CONFIGURE_FLAGS}
 * $ make
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <drm.h>
#include <drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libkms.h>

#include <gbm.h>
#include <gbm_kmsint.h>


#define	LENGTH(a)	(sizeof (a)/sizeof ((a)[0]))
#ifndef LIBGBM_BASE
	#define	LIBGBM_BASE	base.base
#endif

struct settings {
	int drm_fd;
	struct gbm_device *gbm;
	struct gbm_bo *bo;
	struct gbm_bo *bo2;
	struct gbm_surface *surface;
};

struct settings param;

/**
 *
 * Tested function:
 *	kms_device_create (gbm_create_device)
 */
static int
init(void)
{
	const char *name = "rcar-du";
	int fd;
	struct gbm_device *gbm;

	fd = drmOpen(name, NULL);
	if (fd < 0) {
		printf("\tERROR!! drmOpen(%s) failed\n", name);
		return -1;
	}
	param.drm_fd = fd;

	gbm = gbm_create_device(fd);
	if (gbm == NULL) {
		printf("\tERROR!! gbm_create_device(%d) failed\n", fd);
		close(fd);
		return -1;
	}
	param.gbm = gbm;

	return 0;
}

/**
 *
 * Tested function:
 *	gbm_kms_destroy (gbm_device_destroy)
 */
static int
quit(void)
{
	if (param.surface)
		gbm_surface_destroy(param.surface);

	if (param.bo)
		gbm_bo_destroy(param.bo);
	if (param.bo2)
		gbm_bo_destroy(param.bo2);

	gbm_device_destroy(param.gbm);

	close(param.drm_fd);

	return 0;
}

/**
 *
 */
static int
setup_gbm(void)
{
	uint32_t width = 1920u;
	uint32_t height = 1080u;
	uint32_t format = GBM_BO_FORMAT_XRGB8888;
	uint32_t usage = GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE;

	param.bo  = gbm_bo_create(param.gbm, width, height, format, usage);
	param.bo2 = gbm_bo_create(param.gbm, width, height, format, usage);
	if ((!param.bo) || (!param.bo2)) {
		printf("\tERROR!! unexpected behavior in gbm_bo_create\n");
		return -1;
	}
	param.surface = gbm_surface_create(param.gbm, width, height, format, usage);
	if (!param.surface) {
		printf("\tERROR!! unexpected behavior in gbm_surface_create\n");
		return -1;
	}
	return 0;
}

/**
 *
 * Tested function:
 *	gbm_kms_is_format_supported (gbm_device_is_format_supported)
 */
static int
test_supported_format(void)
{
	int ret;
	int i;
	const struct test_format {
		uint32_t format;
		const char *name;
		int expect;
	} list[] = {
		{GBM_FORMAT_ARGB8888, "GBM_FORMAT_ARGB8888", 1},
		{GBM_BO_FORMAT_ARGB8888, "GBM_BO_FORMAT_ARGB8888", 1},
		{GBM_FORMAT_XRGB8888, "GBM_FORMAT_XRGB8888", 1},
		{GBM_BO_FORMAT_XRGB8888, "GBM_BO_FORMAT_XRGB8888", 1},
		{GBM_FORMAT_YUYV, "GBM_FORMAT_YUYV", 0},
		{GBM_FORMAT_RGB565, "GBM_FORMAT_RGB565", 0}
	};

	for (i = 0; i < LENGTH(list); i++) {
		ret = gbm_device_is_format_supported(param.gbm,
						     list[i].format, 0);
		printf("\tgbm_device_is_format_supported: %s %s supported.\n",
			list[i].name, (ret ? "is":"is NOT"));
		if (ret != list[i].expect) {
			printf("\tERROR!! unexpected behavior in "
				"gbm_device_is_format_supported\n");
			return -1;
		}
	}
	return 0;
}

/**
 *
 * Tested functions:
 *	gbm_kms_bo_create (gbm_bo_create)
 *	gbm_kms_bo_destroy (gbm_bo_destroy)
 */
static int
test_bo_allocation(void)
{
	int i;
	struct gbm_bo *bo;
	const struct test_param {
		uint32_t width;
		uint32_t height;
		uint32_t format;
		uint32_t usage;
		int expect;
	} list[] = {
		{64u, 64u, GBM_BO_FORMAT_ARGB8888, GBM_BO_USE_CURSOR_64X64, 1},
		{1920u, 1080u, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_WRITE, 1},
		{1920u, 1080u, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING, 1},
		{1920u, 1080u, GBM_FORMAT_RGB565, GBM_BO_USE_RENDERING, 0}
	};
	struct gbm_kms_device test_dev;

	for (i = 0; i < LENGTH(list); i++) {
		bo = gbm_bo_create(param.gbm,
				   list[i].width, list[i].height,
				   list[i].format, list[i].usage);
		printf("\tgbm_bo_create %s.\n", (bo ? "succeeded":"failed"));

		if (bo)
			gbm_bo_destroy(bo);

		if ((bo && !list[i].expect) || (!bo && list[i].expect)) {
			printf("\tERROR!! unexpected behavior in "
				"gbm_bo_create\n");
			return -1;
		}
	}

	/* Error case in gbm_kms_bo_create */
	memcpy(&test_dev, (struct gbm_kms_device *)param.gbm,
		sizeof (struct gbm_kms_device));
	test_dev.LIBGBM_BASE.fd = -1;	/* Error @ drmPrimeHandleToFD */
	bo = gbm_bo_create(&test_dev.LIBGBM_BASE, 1920u, 1080u,
				GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
	if(bo) {
		printf("\tERROR!! unexpected behavior in gbm_bo_create\n");
		return -1;
	}
	return 0;
}


/**
 *
 * Tested functions:
 *	gbm_kms_import_wl_buffer
 *	gbm_kms_import_fd
 *	gbm_kms_bo_import (gbm_bo_import)
 *	gbm_kms_bo_destroy (gbm_bo_destroy)
 */
static int
test_bo_import(void)
{
	int i;
	int ret = 0;
	struct gbm_bo *bo;
	struct gbm_import_fd_data fd_data, fd_data2;
	uint32_t fb_id;
	struct drm_mode_create_dumb create_arg;
	struct drm_mode_destroy_dumb destroy_arg;
	const struct test_param {
		uint32_t type;
		void *buffer;
		uint32_t usage;
		int expect;
	} list[] = {
		{GBM_BO_IMPORT_WL_BUFFER, NULL, 0, 0},
		{GBM_BO_IMPORT_FD, &fd_data, 0, 1},
		{GBM_BO_IMPORT_FD, &fd_data2, 0, 0},
		{GBM_BO_IMPORT_EGL_IMAGE, NULL, 0, 0},
	};

	/* preparation for gbm_import_fd_data */
	memset(&create_arg, 0, sizeof(create_arg));
	create_arg.bpp = 32;
	create_arg.width = 640;
	create_arg.height = 480;
	ret = drmIoctl(param.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
	if (ret) {
		printf("\tERROR!! drmIoctl(%d) failed\n", param.drm_fd);
		goto end1;
	}
	memset(&destroy_arg, 0, sizeof(destroy_arg));
	destroy_arg.handle = create_arg.handle;
	ret = drmModeAddFB(param.drm_fd, create_arg.width, create_arg.height,
			   24, create_arg.bpp, create_arg.pitch,
			   create_arg.handle, &fb_id);
	if (ret) {
		printf("\tERROR!! drmModeAddFB(%d) failed\n", param.drm_fd);
		goto end2;
	}
	ret = drmPrimeHandleToFD(param.drm_fd, create_arg.handle, DRM_CLOEXEC,
				 &fd_data.fd);
	if (ret) {
		printf("\tERROR!! drmPrimeHandleToFD(%d) failed\n",
			param.drm_fd);
		goto end3;
	}
	fd_data.width = create_arg.width;
	fd_data.height = create_arg.height;
	fd_data.stride = create_arg.pitch;
	fd_data.format = GBM_BO_FORMAT_ARGB8888;

	memcpy(&fd_data2, &fd_data, sizeof fd_data);
	fd_data2.fd = -1;

	for (i = 0; i < LENGTH(list); i++) {
		bo = gbm_bo_import(param.gbm, list[i].type,
				   list[i].buffer, list[i].usage);
		printf("\tgbm_bo_import %s.\n", (bo ? "succeeded":"failed"));

		if (bo)
			gbm_bo_destroy(bo);

		if ((bo && !list[i].expect) || (!bo && list[i].expect)) {
			printf("\tERROR!! unexpected behavior in "
				"gbm_bo_import\n");
			return -1;
		}
	}

end3:
	drmModeRmFB(param.drm_fd, fb_id);
end2:
	drmIoctl(param.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
end1:
	return ret;
}

/**
 *
 * Tested functions:
 *	gbm_kms_bo_write (gbm_bo_write)
 */
static int
test_bo_write(void)
{
	void *buf;
	size_t count;
	int ret;
	struct gbm_kms_bo test_bo;

	memset(&test_bo, 0, sizeof test_bo);
	memcpy(&test_bo.base, param.bo, sizeof (struct gbm_bo));
	/* gbm_kms_bo_write (fail) */
	ret = gbm_bo_write(&test_bo.base, NULL, 0);
	printf("\tgbm_bo_write %s.\n", (ret ? "failed":"succeeded"));
	if (!ret) {
		printf("\tERROR!! unexpected behavior in gbm_bo_write\n");
		return -1;
	}

	count = (size_t)(param.bo->stride * param.bo->height);
	buf = calloc(1, count);
	if (!buf) {
		printf("\tERROR!! calloc failed\n");
		return -1;
	}
	/* gbm_kms_bo_write (succeed) */
	ret = gbm_bo_write(param.bo, buf, count);
	printf("\tgbm_bo_write %s.\n", (ret ? "failed":"succeeded"));
	if (ret) {
		printf("\tERROR!! unexpected behavior in gbm_bo_write\n");
	}

	free(buf);

	return ret;
}

/**
 *
 * Tested functions:
 *	gbm_kms_surface_create (gbm_surface_create)
 *	gbm_kms_surface_destroy (gbm_surface_destroy)
 */
static int
test_surface_allocation(void)
{
	struct gbm_surface *surface;
	int i;
	const struct test_param {
		uint32_t width;
		uint32_t height;
		uint32_t format;
	} list[] = {
		{640u, 480u, GBM_BO_FORMAT_ARGB8888},
		{1920u, 1080u, GBM_BO_FORMAT_XRGB8888},
		{1024u, 768u, GBM_FORMAT_RGB565}
	};
	for (i = 0; i < LENGTH(list); i++) {
		surface = gbm_surface_create(param.gbm,
					     list[i].width, list[i].height,
					     list[i].format, 0);
		printf("\tgbm_surface_create %s.\n",
			(surface ? "succeeded":"failed"));
		if (!surface) {
			printf("\tERROR!! unexpected behavior in "
				"gbm_surface_create\n");
			return -1;
		}
		gbm_surface_destroy(surface);
	}
	return 0;
}

/**
 *
 * Tested functions:
 *	_gbm_kms_set_bo (gbm_kms_set_bo)
 */
static int
test_surface_set_bo(void)
{
	int ret;
	struct gbm_kms_surface *surface = (struct gbm_kms_surface *)param.surface;
	struct gbm_kms_bo *bo = (struct gbm_kms_bo *)param.bo;

	/* Error case in gbm_kms_set_bo */
	ret = gbm_kms_set_bo(surface, -1, bo->addr, bo->fd, bo->base.stride);
	if (!ret) {
		printf("\tERROR!! unexpected behavior in gbm_kms_set_bo\n");
		return -1;
	}
	ret = gbm_kms_set_bo(surface, 2, bo->addr, bo->fd, bo->base.stride);
	if (!ret) {
		printf("\tERROR!! unexpected behavior in gbm_kms_set_bo\n");
		return -1;
	}
	ret = gbm_kms_set_bo(surface, 0, NULL, bo->fd, 0);
	if (ret) {
		printf("\tERROR!! unexpected behavior in gbm_kms_set_bo\n");
		return -1;
	}

	/* Normal case */
	ret = gbm_kms_set_bo(surface, 0, bo->addr, bo->fd, bo->base.stride);
	if (ret) {
		printf("\tERROR!! unexpected behavior in gbm_kms_set_bo\n");
		return -1;
	}
	bo = (struct gbm_kms_bo *)param.bo2;
	ret = gbm_kms_set_bo(surface, 1, bo->addr, bo->fd, bo->base.stride);
	if (ret) {
		printf("\tERROR!! unexpected behavior in gbm_kms_set_bo\n");
		return -1;
	}

	/* Overwritten case */
	ret = gbm_kms_set_bo(surface, 1, bo->addr, bo->fd, bo->base.stride);
	if (ret) {
		printf("\tERROR!! unexpected behavior in gbm_kms_set_bo\n");
		return -1;
	}
	return 0;
}

/**
 *
 * Tested functions:
 *	gbm_kms_surface_lock_front_buffer (gbm_surface_lock_front_buffer)
 *	gbm_kms_surface_release_buffer (gbm_surface_release_buffer)
 *	gbm_kms_surface_has_free_buffers (gbm_surface_has_free_buffers)
 */
static int
test_surface_cotrol(void)
{

	struct gbm_bo *bo;
	struct gbm_kms_surface *surface = (struct gbm_kms_surface *)param.surface;
	int ret;

	/* The front buffer is not set */
	bo = gbm_surface_lock_front_buffer(param.surface);
	if (bo) {
		printf("\tERROR!! unexpected behavior in "
			"gbm_surface_lock_front_buffer\n");
		return -1;
	}

	/* #0 and #1 are free */
	ret = gbm_surface_has_free_buffers(param.surface);
	if (!ret) {
		printf("\tERROR!! gbm_surface_has_free_buffers: "
			"No free buffers could be found in the surface.\n");
		return -1;
	}

	/* Set the front to #0 */
	gbm_kms_set_front(surface, 0);
	bo = gbm_surface_lock_front_buffer(param.surface);
	if (!bo) {
		printf("\tERROR!! unexpected behavior in "
			"gbm_surface_lock_front_buffer\n");
		return -1;
	} else if ((struct gbm_kms_bo *)bo != surface->bo[0]) {
		printf("\tERROR!! gbm_surface_lock_front_buffer: "
			"An expected front buffer could not be obtained.\n");
		return -1;
	}
	/* #0 is locked and #1 is free */
	ret = gbm_surface_has_free_buffers(param.surface);
	if (!ret) {
		printf("\tERROR!! gbm_surface_has_free_buffers: "
			"No free buffers could be found in the surface.\n");
		return -1;
	}

	/* Set the front to #1 */
	gbm_kms_set_front(surface, 1);
	bo = gbm_surface_lock_front_buffer(param.surface);
	if (!bo) {
		printf("\tERROR!! unexpected behavior in "
			"gbm_surface_lock_front_buffer\n");
		return -1;
	} else if ((struct gbm_kms_bo *)bo != surface->bo[1]) {
		printf("\tERROR!! gbm_surface_lock_front_buffer: "
			"An expected front buffer could not be obtained.\n");
		return -1;
	}
	/* #0 and #1 are locked */
	ret = gbm_surface_has_free_buffers(param.surface);
	if (ret) {
		printf("\tERROR!! gbm_surface_has_free_buffers: "
			"Unexpected free buffers could be found in the surface.\n");
		return -1;
	}

	/* Release a locked buffer #0 */
	gbm_surface_release_buffer(param.surface, (struct gbm_bo *)surface->bo[0]);
	/* #0 is free and #1 is locked */
	ret = gbm_surface_has_free_buffers(param.surface);
	if (!ret) {
		printf("\tERROR!! gbm_surface_has_free_buffers: "
			"No free buffers could be found in the surface.\n");
		return -1;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int ret = 0;

	ret = init();
	if (ret)
		return ret;
	ret = setup_gbm();
	if (ret)
		goto out;

	ret = test_supported_format();
	if (ret)
		goto out;

	ret = test_bo_allocation();
	if (ret)
		goto out;
	ret = test_bo_import();
	if (ret)
		goto out;
	ret = test_bo_write();
	if (ret)
		goto out;

	ret = test_surface_allocation();
	if (ret)
		goto out;
	ret = test_surface_set_bo();
	if (ret)
		goto out;
	ret = test_surface_cotrol();
	if (ret)
		goto out;

out:
	ret |= quit();
	if (ret)
		printf("\t[NG] Did NOT pass\n");
	else
		printf("\t[OK] Passed successfully\n");

	return ret;
}

