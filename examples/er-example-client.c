/*
 * er-axample-client.c
 *
 *  Created on: 18 feb 2017
 *      Author: giacomo
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

#define COAP_DEBUG 1
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

#define LOCAL_PORT      COAP_DEFAULT_PORT + 1
#define REMOTE_PORT     COAP_DEFAULT_PORT

#define TOGGLE_INTERVAL 10

#define RECEIVE_QUEUE_LEN 10
static QueueHandle_t receivequeue;

void client_chunk_handler(void *response) {
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

	IP4_ADDR(&server_ipaddr, 192, 168, 1, 6);
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
		PRINTF(" : %u\n", REMOTE_PORT);

		COAP_BLOCKING_REQUEST(&server_ipaddr, REMOTE_PORT, request,
				client_chunk_handler);
		vTaskDelay(pdMS_TO_TICKS(1000) );
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
	xTaskCreate(client, "coap_client", 256, NULL, 2, NULL);
}
