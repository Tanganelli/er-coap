/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Erbium (Er) REST Engine example.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 * \author
 *      Giacomo Tanganelli <g.tanganelli@iet.unipi.it>
 */
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ssid_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dhcpserver.h>
#include "er-coap/rest-engine.h"
#include "er-coap/porting.h"

#define COAP_DEBUG 0
#if COAP_DEBUG
#include <stdio.h>
#include <ip_addr.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) ip_addr_debug_print(COAP_DEBUG, addr)
#define PRINT4ADDR(addr) ip_addr_debug_print(COAP_DEBUG, addr)
#define PRINTLLADDR(addr)
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINT4ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#define AP_SSID "esp"
#define AP_PSK "mypassword"

#define RECEIVE_QUEUE_LEN 10
static QueueHandle_t receivequeue;

extern resource_t res_hello, res_push, res_event, res_separate, res_mirror;

void set_ap() {
	printf("SoftAP\n");
	sdk_wifi_set_opmode(SOFTAP_MODE);
	struct ip_info ap_ip;
	IP4_ADDR(&ap_ip.ip, 192, 168, 0, 1);
	IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
	IP4_ADDR(&ap_ip.netmask, 255, 255, 255, 0);
	sdk_wifi_set_ip_info(1, &ap_ip);

	struct sdk_softap_config ap_config = { .ssid = AP_SSID, .ssid_hidden = 0,
			.channel = 3, .ssid_len = strlen(AP_SSID), .authmode =
					AUTH_WPA_WPA2_PSK, .password = AP_PSK, .max_connection = 3,
			.beacon_interval = 100, };
	sdk_wifi_softap_set_config(&ap_config);

	ip_addr_t first_client_ip;
	IP4_ADDR(&first_client_ip, 192, 168, 0, 2);
	dhcpserver_start(&first_client_ip, 4);
}

void server(void *pvParameters) {
	uint8_t status = sdk_wifi_station_get_connect_status();
	while (status != STATION_GOT_IP) {

		status = sdk_wifi_station_get_connect_status();
		printf("status: (%d) ", status);
		switch (status) {
		case (STATION_GOT_IP):
			printf("STATION_GOT_IP\n");
			break;
		case (STATION_IDLE):
			printf("STATION_IDLE\n");
			break;
		case (STATION_CONNECTING):
			printf("STATION_CONNECTING\n");
			break;
		case (STATION_WRONG_PASSWORD):
			printf("STATION_WRONG_PASSWORD\n");
			break;
		case (STATION_NO_AP_FOUND):
			printf("STATION_NO_AP_FOUND\n");
			break;
		case (STATION_CONNECT_FAIL):
			printf("STATION_CONNECT_FAIL\n");
			break;
		default:
			set_ap();
			status = STATION_GOT_IP;
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	rest_init_engine(&receivequeue);
	rest_activate_resource(&res_hello, "test/hello");
	rest_activate_resource(&res_push, "test/push");
	rest_activate_resource(&res_event, "test/event");
	rest_activate_resource(&res_separate, "test/separate");
	rest_activate_resource(&res_mirror, "test/mirror");
	for (;;) {
	}
}

void serial(void *pvParameters) {
	PRINTF("SERIAL\n");
	while (1) {
		int c = getchar();
		if (c != EOF && c != '\n') {
			res_event.trigger();
			res_separate.resume();
		}

	}
}

void user_init(void) {
	uart_set_baud(0, 115200);
	printf("SDK version:%s\n", sdk_system_get_sdk_version());
	struct sdk_station_config config = { .ssid = WIFI_SSID, .password =
	WIFI_PASS, };

	/* required to call wifi_set_opmode before station_set_config */
	sdk_wifi_set_opmode(STATION_MODE);
	sdk_wifi_station_set_config(&config);
	sdk_wifi_station_connect();
	receivequeue = xQueueCreate(RECEIVE_QUEUE_LEN, sizeof(received_item_t));
	xTaskCreate(server, "coap_server", 256, NULL, 2, NULL);
	xTaskCreate(serial, "serial", 256, NULL, 2, NULL);
}
