/* Generated by wayland-scanner 1.18.0 */

#ifndef IVI_APPLICATION_CLIENT_PROTOCOL_H
#define IVI_APPLICATION_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @page page_ivi_application The ivi_application protocol
 * @section page_ifaces_ivi_application Interfaces
 * - @subpage page_iface_ivi_surface - application interface to surface in ivi compositor
 * - @subpage page_iface_ivi_application - create ivi-style surfaces
 * @section page_copyright_ivi_application Copyright
 * <pre>
 *
 * Copyright (C) 2013 DENSO CORPORATION
 * Copyright (c) 2013 BMW Car IT GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * </pre>
 */
struct ivi_application;
struct ivi_surface;
struct wl_surface;

/**
 * @page page_iface_ivi_surface ivi_surface
 * @section page_iface_ivi_surface_desc Description
 * @section page_iface_ivi_surface_api API
 * See @ref iface_ivi_surface.
 */
/**
 * @defgroup iface_ivi_surface The ivi_surface interface
 */
extern const struct wl_interface ivi_surface_interface;
/**
 * @page page_iface_ivi_application ivi_application
 * @section page_iface_ivi_application_desc Description
 *
 * This interface is exposed as a global singleton.
 * This interface is implemented by servers that provide IVI-style user interfaces.
 * It allows clients to associate a ivi_surface with wl_surface.
 * @section page_iface_ivi_application_api API
 * See @ref iface_ivi_application.
 */
/**
 * @defgroup iface_ivi_application The ivi_application interface
 *
 * This interface is exposed as a global singleton.
 * This interface is implemented by servers that provide IVI-style user interfaces.
 * It allows clients to associate a ivi_surface with wl_surface.
 */
extern const struct wl_interface ivi_application_interface;

/**
 * @ingroup iface_ivi_surface
 * @struct ivi_surface_listener
 */
struct ivi_surface_listener {
	/**
	 * suggest resize
	 *
	 * The configure event asks the client to resize its surface.
	 *
	 * The size is a hint, in the sense that the client is free to
	 * ignore it if it doesn't resize, pick a smaller size (to satisfy
	 * aspect ratio or resize in steps of NxM pixels).
	 *
	 * The client is free to dismiss all but the last configure event
	 * it received.
	 *
	 * The width and height arguments specify the size of the window in
	 * surface-local coordinates.
	 */
	void (*configure)(void *data,
			  struct ivi_surface *ivi_surface,
			  int32_t width,
			  int32_t height);
};

/**
 * @ingroup iface_ivi_surface
 */
static inline int
ivi_surface_add_listener(struct ivi_surface *ivi_surface,
			 const struct ivi_surface_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) ivi_surface,
				     (void (**)(void)) listener, data);
}

#define IVI_SURFACE_DESTROY 0

/**
 * @ingroup iface_ivi_surface
 */
#define IVI_SURFACE_CONFIGURE_SINCE_VERSION 1

/**
 * @ingroup iface_ivi_surface
 */
#define IVI_SURFACE_DESTROY_SINCE_VERSION 1

/** @ingroup iface_ivi_surface */
static inline void
ivi_surface_set_user_data(struct ivi_surface *ivi_surface, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) ivi_surface, user_data);
}

/** @ingroup iface_ivi_surface */
static inline void *
ivi_surface_get_user_data(struct ivi_surface *ivi_surface)
{
	return wl_proxy_get_user_data((struct wl_proxy *) ivi_surface);
}

static inline uint32_t
ivi_surface_get_version(struct ivi_surface *ivi_surface)
{
	return wl_proxy_get_version((struct wl_proxy *) ivi_surface);
}

/**
 * @ingroup iface_ivi_surface
 *
 * This removes link from ivi_id to wl_surface and destroys ivi_surface.
 * The ID, ivi_id, is free and can be used for surface_create again.
 */
static inline void
ivi_surface_destroy(struct ivi_surface *ivi_surface)
{
	wl_proxy_marshal((struct wl_proxy *) ivi_surface,
			 IVI_SURFACE_DESTROY);

	wl_proxy_destroy((struct wl_proxy *) ivi_surface);
}

#ifndef IVI_APPLICATION_ERROR_ENUM
#define IVI_APPLICATION_ERROR_ENUM
enum ivi_application_error {
	/**
	 * given wl_surface has another role
	 */
	IVI_APPLICATION_ERROR_ROLE = 0,
	/**
	 * given ivi_id is assigned to another wl_surface
	 */
	IVI_APPLICATION_ERROR_IVI_ID = 1,
};
#endif /* IVI_APPLICATION_ERROR_ENUM */

#define IVI_APPLICATION_SURFACE_CREATE 0


/**
 * @ingroup iface_ivi_application
 */
#define IVI_APPLICATION_SURFACE_CREATE_SINCE_VERSION 1

/** @ingroup iface_ivi_application */
static inline void
ivi_application_set_user_data(struct ivi_application *ivi_application, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) ivi_application, user_data);
}

/** @ingroup iface_ivi_application */
static inline void *
ivi_application_get_user_data(struct ivi_application *ivi_application)
{
	return wl_proxy_get_user_data((struct wl_proxy *) ivi_application);
}

static inline uint32_t
ivi_application_get_version(struct ivi_application *ivi_application)
{
	return wl_proxy_get_version((struct wl_proxy *) ivi_application);
}

/** @ingroup iface_ivi_application */
static inline void
ivi_application_destroy(struct ivi_application *ivi_application)
{
	wl_proxy_destroy((struct wl_proxy *) ivi_application);
}

/**
 * @ingroup iface_ivi_application
 *
 * This request gives the wl_surface the role of an IVI Surface. Creating more than
 * one ivi_surface for a wl_surface is not allowed. Note, that this still allows the
 * following example:
 *
 * 1. create a wl_surface
 * 2. create ivi_surface for the wl_surface
 * 3. destroy the ivi_surface
 * 4. create ivi_surface for the wl_surface (with the same or another ivi_id as before)
 *
 * surface_create will create a interface:ivi_surface with numeric ID; ivi_id in
 * ivi compositor. These ivi_ids are defined as unique in the system to identify
 * it inside of ivi compositor. The ivi compositor implements business logic how to
 * set properties of the surface with ivi_id according to status of the system.
 * E.g. a unique ID for Car Navigation application is used for implementing special
 * logic of the application about where it shall be located.
 * The server regards following cases as protocol errors and disconnects the client.
 * - wl_surface already has an nother role.
 * - ivi_id is already assigned to an another wl_surface.
 *
 * If client destroys ivi_surface or wl_surface which is assigne to the ivi_surface,
 * ivi_id which is assigned to the ivi_surface is free for reuse.
 */
static inline struct ivi_surface *
ivi_application_surface_create(struct ivi_application *ivi_application, uint32_t ivi_id, struct wl_surface *surface)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_constructor((struct wl_proxy *) ivi_application,
			 IVI_APPLICATION_SURFACE_CREATE, &ivi_surface_interface, ivi_id, surface, NULL);

	return (struct ivi_surface *) id;
}

#ifdef  __cplusplus
}
#endif

#endif
