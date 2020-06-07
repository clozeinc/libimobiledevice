/*
 * idevicewifi.c
 * Simple utility to enable or disable WiFi syncing
 *
 * Copyright (c) 2020  Cloze, Inc., All Rights Reserved.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define TOOL_NAME "idevicewifi"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#ifndef WIN32
#include <signal.h>
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include "common/utils.h"

static void print_usage(void)
{
	printf("Usage: idevicewifi [OPTIONS] [ENABLE]\n");
	printf("\n");
	printf("Display the EnableWifiConnections vulue or set it to ENABLE if specified.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("  -u, --udid UDID\ttarget specific device by UDID\n");
	printf("  -n, --network\t\tconnect to network device\n");
	printf("  -d, --debug\t\tenable communication debugging\n");
	printf("  -h, --help\t\tprint usage information\n");
	printf("  -v, --version\t\tprint version information\n");
	printf("\n");
	printf("Homepage:    <" PACKAGE_URL ">\n");
	printf("Bug Reports: <" PACKAGE_BUGREPORT ">\n");
}

int main(int argc, char** argv)
{
	int c = 0;
	const struct option longopts[] = {
		{ "udid",    required_argument, NULL, 'u' },
		{ "network", no_argument,       NULL, 'n' },
		{ "debug",   no_argument,       NULL, 'd' },
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'v' },
		{ NULL, 0, NULL, 0}
	};
	int res = -1;
	char* udid = NULL;
	int use_network = 0;

#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	while ((c = getopt_long(argc, argv, "du:hnv", longopts, NULL)) != -1) {
		switch (c) {
		case 'u':
			if (!*optarg) {
				fprintf(stderr, "ERROR: UDID must not be empty!\n");
				print_usage();
				exit(2);
			}
			free(udid);
			udid = strdup(optarg);
			break;
		case 'n':
			use_network = 1;
			break;
		case 'h':
			print_usage();
			return 0;
		case 'd':
			idevice_set_debug_level(1);
			break;
		case 'v':
			printf("%s %s\n", TOOL_NAME, PACKAGE_VERSION);
			return 0;
		default:
			print_usage();
			return 2;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 1) {
		print_usage();
		return -1;
	}

	idevice_t device = NULL;
	if (idevice_new_with_options(&device, udid, (use_network) ? IDEVICE_LOOKUP_USBMUX | IDEVICE_LOOKUP_NETWORK : IDEVICE_LOOKUP_USBMUX) != IDEVICE_E_SUCCESS) {
		if (udid) {
			fprintf(stderr, "ERROR: No device found with udid %s.\n", udid);
		} else {
			fprintf(stderr, "ERROR: No device found.\n");
		}
		return -1;
	}

	lockdownd_client_t lockdown = NULL;
	lockdownd_error_t lerr = lockdownd_client_new_with_handshake(device, &lockdown, TOOL_NAME);
	if (lerr != LOCKDOWN_E_SUCCESS) {
		idevice_free(device);
		fprintf(stderr, "ERROR: Could not connect to lockdownd, error code %d\n", lerr);
		return -1;
	}

	plist_t node = NULL;
	lerr = lockdownd_get_value(lockdown, "com.apple.mobile.wireless_lockdown", "EnableWifiConnections", &node);

     if(lerr == LOCKDOWN_E_SUCCESS && node) {
        uint8_t b;
        plist_get_bool_val(node, &b);
        plist_free(node);
        node = NULL;

        if(argc != 0) {
            // setting device property
            int enabled = strcmp(argv[0], "true") == 0;

            if(enabled != b) {
                lerr = lockdownd_set_value(lockdown, "com.apple.mobile.wireless_lockdown", "EnableWifiConnections", plist_new_bool(enabled));

                if (lerr == LOCKDOWN_E_SUCCESS) {
                    res = 0;
                } else {
                    fprintf(stderr, "ERROR: Could not set property, lockdown error %d\n", lerr);
                }

                b = enabled;
            } else {
                res = 0;
            }
        } else {
            res = 0;
        }

        printf("EnableWifiConnections: %s\n", b ? "true" : "false");
    } else {
        fprintf(stderr, "ERROR: Could not get property, lockdown error %d\n", lerr);
    }

	if (argc == 0) {
	} else {
	}

	lockdownd_client_free(lockdown);
	idevice_free(device);

	if (udid) {
		free(udid);
	}

	return res;
}
