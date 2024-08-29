/*
 * idevicepower.c
 * Retrieves power information from device
 *
 * Copyright (c) 2012 Martin Szulecki All Rights Reserved.
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

#define TOOL_NAME "idevicepower"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#ifndef WIN32
#include <signal.h>
#endif

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/power.h>

enum cmd_mode {
	CMD_NONE = 0,
	CMD_WIRELESS_SYNC,
	CMD_USER_IDLE_SLEEP,
	CMD_SYSTEM_SLEEP
};

static void print_usage(int argc, char **argv, int is_error)
{
	char *name = strrchr(argv[0], '/');
	fprintf(is_error ? stderr : stdout, "Usage: %s [OPTIONS] COMMAND\n", (name ? name + 1: argv[0]));
	fprintf(is_error ? stderr : stdout,
		"\n"
		"Send power assertion to device.\n"
		"\n"
		"Where COMMAND is one of:\n"
		"  sync                  Send wireless sync power assertion\n"
		"  idle                  Send user idle power assertion\n"
		"  sleep                 Send sleep power assertion\n"
		"\n"
		"The following OPTIONS are accepted:\n"
		"  -u, --udid UDID       target specific device by UDID\n"
		"  -t, --timeout SECONDS timeout for assertion (default 60)\n"
		"  -n, --network         connect to network device\n"
		"  -d, --debug           enable communication debugging\n"
		"  -h, --help            prints usage information\n"
		"  -v, --version         prints version information\n"
		"\n"
		"Homepage:    <" PACKAGE_URL ">\n"
		"Bug Reports: <" PACKAGE_BUGREPORT ">\n"
	);
}

int main(int argc, char **argv)
{
	idevice_t device = NULL;
	lockdownd_client_t lockdown_client = NULL;
	power_client_t power_client = NULL;
	lockdownd_error_t ret = LOCKDOWN_E_UNKNOWN_ERROR;
	lockdownd_service_descriptor_t service = NULL;
	int result = EXIT_FAILURE;
	const char *udid = NULL;
	int use_network = 0;
	int cmd = CMD_NONE;
	char* cmd_arg = NULL;
	plist_t assertion = NULL;
	int c = 0;
	int timeout = 60;
	const struct option longopts[] = {
		{ "debug", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "udid", required_argument, NULL, 'u' },
		{ "timeout", required_argument, NULL, 't' },
		{ "network", no_argument, NULL, 'n' },
		{ "version", no_argument, NULL, 'v' },
		{ NULL, 0, NULL, 0}
	};

#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	/* parse cmdline args */
	while ((c = getopt_long(argc, argv, "dhu:t:nv", longopts, NULL)) != -1) {
		switch (c) {
		case 'd':
			idevice_set_debug_level(1);
			break;
		case 'u':
			if (!*optarg) {
				fprintf(stderr, "ERROR: UDID argument must not be empty!\n");
				print_usage(argc, argv, 1);
				return 2;
			}
			udid = optarg;
			break;
		case 't':
			if (!*optarg) {
				fprintf(stderr, "ERROR: Timeout argument must not be empty!\n");
				print_usage(argc, argv, 1);
				return 2;
			}
			timeout = atoi(optarg);
			if(timeout <= 0) {
				fprintf(stderr, "ERROR: Invalid timeout value (must be greater than 0)!\n");
				print_usage(argc, argv, 1);
				return 2;
			}
			break;
		case 'n':
			use_network = 1;
			break;
		case 'h':
			print_usage(argc, argv, 0);
			return 0;
		case 'v':
			printf("%s %s\n", TOOL_NAME, PACKAGE_VERSION);
			return 0;
		default:
			print_usage(argc, argv, 1);
			return 2;
		}
	}
	argc -= optind;
	argv += optind;

	if (!argv[0]) {
		fprintf(stderr, "ERROR: No command specified\n");
		print_usage(argc+optind, argv-optind, 1);
		return 2;
	}

	if (!strcmp(argv[0], "sync")) {
		cmd = CMD_WIRELESS_SYNC;
	}
	else if (!strcmp(argv[0], "idle")) {
		cmd = CMD_USER_IDLE_SLEEP;
	}
	else if (!strcmp(argv[0], "sleep")) {
		cmd = CMD_SYSTEM_SLEEP;
	}

	/* verify options */
	if (cmd == CMD_NONE) {
		fprintf(stderr, "ERROR: Unsupported command '%s'\n", argv[0]);
		print_usage(argc+optind, argv-optind, 1);
		goto cleanup;
	}

	if (IDEVICE_E_SUCCESS != idevice_new_with_options(&device, udid, (use_network) ? IDEVICE_LOOKUP_USBMUX | IDEVICE_LOOKUP_NETWORK : IDEVICE_LOOKUP_USBMUX)) {
		if (udid) {
			printf("No device found with udid %s.\n", udid);
		} else {
			printf("No device found.\n");
		}
		goto cleanup;
	}

	if (LOCKDOWN_E_SUCCESS != (ret = lockdownd_client_new_with_handshake(device, &lockdown_client, TOOL_NAME))) {
		idevice_free(device);
		printf("ERROR: Could not connect to lockdownd, error code %d\n", ret);
		goto cleanup;
	}

	ret = lockdownd_start_service(lockdown_client, "com.apple.mobile.assertion_agent", &service);
	lockdownd_client_free(lockdown_client);

	if (ret != LOCKDOWN_E_SUCCESS) {
		idevice_free(device);
		printf("ERROR: Could not start power agent service: %s\n", lockdownd_strerror(ret));
		goto cleanup;
	}

	if ((ret == LOCKDOWN_E_SUCCESS) && service && (service->port > 0)) {
		if (power_client_new(device, service, &power_client) != POWER_E_SUCCESS) {
			printf("ERROR: Could not connect to power!\n");
		} else {
		    assertion = plist_new_dict();
		    plist_dict_set_item(assertion, "CommandKey", plist_new_string("CommandCreateAssertion"));

			switch (cmd) {
				case CMD_WIRELESS_SYNC:
                    plist_dict_set_item(assertion, "AssertionTypeKey", plist_new_string("AMDPowerAssertionTypeWirelessSync"));
                    break;
				case CMD_USER_IDLE_SLEEP:
                    plist_dict_set_item(assertion, "AssertionTypeKey", plist_new_string("PreventUserIdleSystemSleep"));
                    break;
				case CMD_SYSTEM_SLEEP:
                    plist_dict_set_item(assertion, "AssertionTypeKey", plist_new_string("PreventSystemSleep"));
                    break;
			}

		    plist_dict_set_item(assertion, "AssertionNameKey", plist_new_string(TOOL_NAME));
		    plist_dict_set_item(assertion, "AssertionTimeoutKey", plist_new_uint(timeout));
            plist_dict_set_item(assertion, "AssertionDetailKey", plist_new_string("power update"));

            power_error_t perr = power_send(power_client, assertion);
            plist_free(assertion);
            assertion = NULL;

            if(perr != POWER_E_SUCCESS) {
                printf("ERROR: Could not send power assertion: %d\n", perr);
            } else {
                perr = power_receive(power_client, &assertion);

                if(perr != POWER_E_SUCCESS) {
                    printf("ERROR: Could not receive power assertion: %d\n", perr);
                } else {
                    result = EXIT_SUCCESS;
                }
            }

            if(timeout > 10)
                timeout -= 10;

            sleep(timeout);
			power_client_free(power_client);
		}
	} else {
		printf("ERROR: Could not start power service!\n");
	}

	if (service) {
		lockdownd_service_descriptor_free(service);
		service = NULL;
	}

	idevice_free(device);

cleanup:
	if (assertion) {
		plist_free(assertion);
	}
	if (cmd_arg) {
		free(cmd_arg);
	}
	return result;
}
