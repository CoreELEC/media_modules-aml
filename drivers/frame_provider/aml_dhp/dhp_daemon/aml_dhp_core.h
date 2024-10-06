/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef __AML_DHP_CORE_H_
#define __AML_DHP_CORE_H_

/*
 * aml_data_handle() - Handles data based on its type, using a DMA buffer and metadata.
 *
 * @dev: Pointer to the device or context used for handling the data.
 * @type: The type of data being processed (e.g., command or data type identifier).
 * @data: Pointer to the metadata associated with the data, providing additional information
 *        needed for processing.
 *
 * This function processes data based on the provided type, using a DMA buffer for data
 * transfer and leveraging metadata for any additional processing requirements. The `dev`
 * context is used for device-specific operations related to the data handling.
 *
 * Return: 0 on success, or a negative error code if the data handling fails.
 */
int aml_data_handle(void *dev, unsigned int type, void *data);

#endif //__AML_DHP_CORE_H_

