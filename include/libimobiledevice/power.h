/**
 * @file libimobiledevice/power.h
 * @brief Send power assertion commands to device
 * \internal
 *
 * Copyright (c) 2023 Cloze, Inc. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef IPOWER_H
#define IPOWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

/** Service identifier passed to lockdownd_start_service() to start the power service */
#define POWER_SERVICE_NAME "com.apple.mobile.assertion_agent"

/** Error Codes */
typedef enum {
	POWER_E_SUCCESS         =  0,
	POWER_E_INVALID_ARG     = -1,
	POWER_E_PLIST_ERROR     = -2,
	POWER_E_MUX_ERROR       = -3,
	POWER_E_SSL_ERROR       = -4,
	POWER_E_NOT_ENOUGH_DATA = -5,
	POWER_E_TIMEOUT         = -6,
	POWER_E_UNKNOWN_ERROR   = -256
} power_error_t;

typedef struct power_client_private power_client_private; /**< \private */
typedef power_client_private *power_client_t; /**< The client handle. */

/**
 * Connects to the power service on the specified device.
 *
 * @param device The device to connect to.
 * @param service The service descriptor returned by lockdownd_start_service.
 * @param client Pointer that will point to a newly allocated
 *     power_client_t upon successful return. Must be freed using
 *     power_client_free() after use.
 *
 * @return POWER_E_SUCCESS on success, POWER_E_INVALID_ARG when
 *     client is NULL, or an POWER_E_* error code otherwise.
 */
power_error_t power_client_new(idevice_t device, lockdownd_service_descriptor_t service, power_client_t * client);

/**
 * Starts a new power service on the specified device and connects to it.
 *
 * @param device The device to connect to.
 * @param client Pointer that will point to a newly allocated
 *     power_client_t upon successful return. Must be freed using
 *     power_client_free() after use.
 * @param label The label to use for communication. Usually the program name.
 *  Pass NULL to disable sending the label in requests to lockdownd.
 *
 * @return POWER_E_SUCCESS on success, or an POWER_E_* error
 *     code otherwise.
 */
power_error_t power_client_start_service(idevice_t device, power_client_t * client, const char* label);

/**
 * Disconnects a power client from the device and frees up the
 * power client data.
 *
 * @param client The power client to disconnect and free.
 *
 * @return POWER_E_SUCCESS on success, POWER_E_INVALID_ARG when
 *     client is NULL, or an POWER_E_* error code otherwise.
 */
power_error_t power_client_free(power_client_t client);


/**
 * Sends a plist to the service.
 *
 * @param client The power client
 * @param plist The plist to send
 *
 * @return POWER_E_SUCCESS on success,
 *  POWER_E_INVALID_ARG when client or plist is NULL
 */
power_error_t power_send(power_client_t client, plist_t plist);

/**
 * Receives a plist from the service.
 *
 * @param client The power client
 * @param plist The plist to store the received data
 *
 * @return POWER_E_SUCCESS on success,
 *  POWER_E_INVALID_ARG when client or plist is NULL
 */
power_error_t power_receive(power_client_t client, plist_t * plist);

/**
 * Receives a plist using the given power client.
 *
 * @param client The power client to use for receiving
 * @param plist pointer to a plist_t that will point to the received plist
 *      upon successful return
 * @param timeout_ms Maximum time in milliseconds to wait for data.
 *
 * @return POWER_E_SUCCESS on success,
 *      POWER_E_INVALID_ARG when client or *plist is NULL,
 *      POWER_E_NOT_ENOUGH_DATA when not enough data
 *      received, POWER_E_TIMEOUT when the connection times out,
 *      POWER_E_PLIST_ERROR when the received data cannot be
 *      converted to a plist, POWER_E_MUX_ERROR when a
 *      communication error occurs, or POWER_E_UNKNOWN_ERROR
 *      when an unspecified error occurs.
 */
power_error_t power_receive_with_timeout(power_client_t client, plist_t * plist, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
