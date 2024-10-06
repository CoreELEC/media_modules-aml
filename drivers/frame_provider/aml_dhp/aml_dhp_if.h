/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef _AML_DHP_IF_H_
#define _AML_DHP_IF_H_

#include <linux/types.h>

#include "aml_dhp_types.h"

#define _IOCTL_DHP_MAGIC	'Q'

/* The processed data is returned to userspace as an FD. */
#define IOCTL_DHP_GET_FD	_IOR(_IOCTL_DHP_MAGIC, 0, __u64)

/* Used for mapping of a page. */
#define IOCTL_DHP_MMAP		_IOWR(_IOCTL_DHP_MAGIC, 1, __u64)

/* Used for mapping of a page list. */
#define IOCTL_DHP_SCT_MAP	_IOWR(_IOCTL_DHP_MAGIC, 2, __u64) //TODO

/* Userspace submits a data processing task to the proc driver. */
#define IOCTL_DHP_SET_TASK	_IOW(_IOCTL_DHP_MAGIC, 3, __u64) //TODO

#define DHP_VER(a,b,c)	(((a) << 16) + ((b) << 8) + (c))

// Maximum number of metadata entries for data units
#define AML_DU_META_MAX		(32)

/*
 * struct aml_du_base - Base structure for data unit operations.
 *
 * @src       : Source memory information for the data unit.
 * @dst       : Destination memory information for the data unit.
 * @meta      : Metadata array for additional information related to the data unit.
 */
struct aml_du_base {
	struct aml_du_mem	src;
	struct aml_du_mem	dst;
	__u32			meta[AML_DU_META_MAX];
} __attribute__((packed));

/*
 * get_du_type - Retrieves the data unit type from the base structure.
 *
 * @base      : Pointer to the aml_du_base structure.
 *
 * Returns the type of the data unit stored in the metadata.
 */
static inline __u32 get_du_type(struct aml_du_base *base)
{
	return base->meta[0];
}

/*
 * get_du_meta - Retrieves a pointer to the metadata of the data unit.
 *
 * @base      : Pointer to the aml_du_base structure.
 *
 * Returns a pointer to the metadata array.
 */
static inline void *get_du_meta(struct aml_du_base *base)
{
	return base->meta;
}

/*
 * tag_to_string - Converts an integer tag into a string representation.
 *
 * @tag       : The integer tag to convert.
 * @str       : Pointer to the character array where the string will be stored.
 */
static inline void tag_to_string(int tag, char* str)
{
	str[0]	= (tag >> 24) & 0xFF;
	str[1]	= (tag >> 16) & 0xFF;
	str[2]	= (tag >> 8) & 0xFF;
	str[3]	= tag & 0xFF;
	str[4]	= '\0';
}

/*
 * ver_to_string - Converts a version number into a string representation.
 *
 * @ver       : The version number as an integer.
 * @str       : Pointer to the character array where the version string will be stored.
 */
static void inline ver_to_string(int ver, char* str)
{
	int major	= (ver >> 16) & 0xFF;	/* major */
	int minor	= (ver >> 8) & 0xFF;	/* minor */
	int patch	= ver & 0xFF;			/* patch */

	sprintf(str, "v%d.%d.%d", major, minor, patch);
}

/*
 * struct aml_dhp_ioctl_data - IOCTL data structure containing different types of data.
 *
 * @version   : Version of the data structure, used for compatibility checks.
 * @type      : Indicates the data type of the data unit (DU) being passed; used to determine the appropriate processing method.
 * @base      : Base data structure, providing common parameters for processing.
 * @mem       : Type of memory descriptor, used for memory management tasks.
 * @data      : General-purpose data buffer for storing arbitrary data; can be used for various operations as needed.
 * @fd        : File descriptor of the DMA buffer (dmabuf) associated with the DU data, used for memory operations.
 * @reserved  : Reserved fields for future use or expansion, ensuring compatibility with future versions.
 */
struct aml_dhp_ioctl_data {
	__u32	version;
	__u32	type;
	union {
		struct aml_du_base	base;
		struct aml_du_mem	mem;
		__u32			data[64];
	};
	__s32	fd;
	__u32	reserved[16];
};

/**
 * aml_dhp_request - Perform data processing using source and destination memory buffers.
 * @src: Pointer to the source memory buffer (aml_du_mem structure).
 * @dst: Pointer to the destination memory buffer (aml_du_mem structure).
 * @meta: Pointer to metadata for the task.
 * @msize: Size of the metadata in bytes.
 *
 * This function executes a data processing task using the provided source and destination
 * memory buffers through the DHP (Data Handler Proxy) driver. It performs the necessary
 * operations as described by the metadata and waits for the task to complete within a
 * predefined timeout period.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int aml_dhp_request(struct aml_du_mem *src, struct aml_du_mem *dst, void *meta, int msize);

#endif //_AML_DHP_IF_H_

