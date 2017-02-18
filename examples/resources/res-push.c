/*
 * res-push.c
 *
 *  Created on: 12 feb 2017
 *      Author: giacomo
 */

#include <string.h>
#include <FreeRTOS.h>
#include <timers.h>
#include "er-coap/rest-engine.h"

#define COAP_DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF("[%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x]", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF("[%02x:%02x:%02x:%02x:%02x:%02x]", (lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3], (lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

static void res_get_handler(void *request, void *response, uint8_t *buffer,
		uint16_t preferred_size, int32_t *offset);
static void res_periodic_handler(TimerHandle_t xTimer);

PERIODIC_RESOURCE(res_push, "title=\"Periodic demo\";obs", res_get_handler,
		NULL, NULL, NULL, 5000, res_periodic_handler);
/*
 * Use local resource state that is accessed by res_get_handler() and altered by res_periodic_handler() or PUT or POST.
 */
static int32_t event_counter = 0;

static void res_get_handler(void *request, void *response, uint8_t *buffer,
		uint16_t preferred_size, int32_t *offset) {
	/*
	 * For minimal complexity, request query and options should be ignored for GET on observable resources.
	 * Otherwise the requests must be stored with the observer list and passed by REST.notify_subscribers().
	 * This would be a TODO in the corresponding files in contiki/apps/erbium/!
	 */
	REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
	REST.set_header_max_age(response, res_push.periodic->period / 1000);
	REST.set_response_payload(response, buffer,
			snprintf((char *) buffer, preferred_size, "VERY LONG EVENT %lu",
					event_counter));

	/* The REST.subscription_handler() will be called for observable resources by the REST framework. */
}
/*
 * Additionally, a handler function named [resource name]_handler must be implemented for each PERIODIC_RESOURCE.
 * It will be called by the REST manager process with the defined period.
 */
static void res_periodic_handler(TimerHandle_t xTimer) {
	PRINTF("PERIODIC HANDLE\n");
	/* Do a periodic task here, e.g., sampling a sensor. */
	++event_counter;

	/* Usually a condition is defined under with subscribers are notified, e.g., large enough delta in sensor reading. */
	if (1) {
		/* Notify the registered observers which will trigger the res_get_handler to create the response. */
		REST.notify_subscribers(&res_push);
	}
}
