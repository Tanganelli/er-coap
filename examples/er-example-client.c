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
 *      Erbium (Er) CoAP client example.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ssid_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ip_addr.h>
#include "er-coap/er-coap-engine.h"
#include "er-coap/porting.h"

#define COAP_DEBUG 0
#if COAP_DEBUG
#include <stdio.h>
#include <ip_addr.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) printf("%u:%u:%u:%u:%u:%u:%u:%u\n", \
         (ntohl(ipaddr->addr[0]) >> 16) & 0xffff, \
         ntohl(ipaddr->addr[0]) & 0xffff, \
         (ntohl(ipaddr->addr[1]) >> 16) & 0xffff, \
         ntohl(ipaddr->addr[1]) & 0xffff, \
         (ntohl(ipaddr->addr[2]) >> 16) & 0xffff, \
         ntohl(ipaddr->addr[2]) & 0xffff, \
         (ntohl(ipaddr->addr[3]) >> 16) & 0xffff, \
         ntohl(ipaddr->addr[3]) & 0xffff));)
#define PRINT4ADDR(addr) printf("%u.%u.%u.%u", addr != NULL ? ip4_addr1_16(addr) : 0, addr != NULL ? ip4_addr2_16(addr) : 0, addr != NULL ? ip4_addr3_16(addr) : 0, addr != NULL ? ip4_addr4_16(addr) : 0);
#define PRINTLLADDR(addr)
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINT4ADDR(addr)
#define PRINTLLADDR(addr)
#endif

#define LOCAL_PORT      COAP_DEFAULT_PORT + 1
#define REMOTE_PORT     COAP_DEFAULT_PORT

#define TOGGLE_INTERVAL 10

#define RECEIVE_QUEUE_LEN 10
static QueueHandle_t receivequeue;
static QueueHandle_t respqueue;

void client_chunk_handler(void *response) {

//	xQueueSend(respqueue, resp, 0);
	const uint8_t *chunk;

	int len = coap_get_payload(response, &chunk);

	printf("|%.*s", len, (char *) chunk);
}

void client(void *pvParameters) {
	uint8_t status = sdk_wifi_station_get_connect_status();
	while (status != STATION_GOT_IP) {
		status = sdk_wifi_station_get_connect_status();
	}
	ip_addr_t server_ipaddr;
	static coap_packet_t request[1]; /* This way the packet can be treated as pointer as usual. */

	IP4_ADDR(&server_ipaddr, 192, 168, 0, 4);
	PRINTF("Start client\n");
	/* receives all CoAP messages */
	coap_init_engine(&receivequeue);
	while (1) {
		/* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
		coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
		coap_set_header_uri_path(request, "/dimmer");
		const char msg[] = "Toggle!";

		coap_set_payload(request, (uint8_t *) msg, sizeof(msg) - 1);
		PRINTF("Request /dimmer to ");
		PRINT4ADDR(&server_ipaddr);
		PRINTF(":%u\n", REMOTE_PORT);

		COAP_BLOCKING_REQUEST(&server_ipaddr, REMOTE_PORT, request,
				client_chunk_handler);
//		coap_packet_t response;
//		while (xQueueReceive(respqueue, &response, 1000) != pdTRUE) {
//		}
		PRINTF("COAP_BLOCKING_REQUEST\n");
//		const uint8_t *chunk;
//
//		int len = coap_get_payload(&response, &chunk);
//
//		printf("|%.*s", len, (char *) chunk);
		//vTaskDelay(pdMS_TO_TICKS(5000));
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
	respqueue = xQueueCreate(1, sizeof(coap_packet_t));
	xTaskCreate(client, "coap_client", 256, NULL, 2, NULL);
}
