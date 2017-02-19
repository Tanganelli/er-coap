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
 *      CoAP implementation for the REST Engine.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include "er-coap-engine.h"

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

/*---------------------------------------------------------------------------*/
/*- Variables ---------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static service_callback_t service_cbk = NULL;
QueueHandle_t *receivequeue_ptr;
static QueueHandle_t responsequeue;

///* Dimensions the buffer that the task being created will use as its stack.
//NOTE:  This is the number of words the stack will hold, not the number of
//bytes.  For example, if each stack item is 32-bits, and this is set to 100,
//then 400 bytes (100 * 32-bits) will be allocated. */
//#define STACK_SIZE 256
//
///* Structure that will hold the TCB of the task being created. */
//StaticTask_t xTaskBuffer;
//
///* Buffer that the task being created will use as its stack.  Note this is
//an array of StackType_t variables.  The size of StackType_t is dependent on
//the RTOS port. */
//StackType_t xStack[ STACK_SIZE ];

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
bool uip_newdata() {
	UBaseType_t uxNumberOfItems;
	/* How many items are currently in the queue referenced by the xQueue handle? */
	uxNumberOfItems = uxQueueMessagesWaiting(*receivequeue_ptr);
	return (uxNumberOfItems != 0);
}

static int coap_receive(void) {
	static coap_packet_t message[1]; /* this way the packet can be treated as pointer as usual */
	static coap_packet_t response[1];
	static coap_transaction_t *transaction = NULL;
	erbium_status_code = NO_ERROR;
	received_item_t datagram;

	if (uip_newdata()) {
		xQueueReceive(*receivequeue_ptr, &datagram, 1000);
		struct pbuf *p = datagram.p;
		struct ip_addr *addr = &datagram.addr;
		u16_t port = datagram.port;

		PRINTF("receiving UDP datagram from: ");
		PRINT4ADDR(addr);
		PRINTF(":%u\n  Length: %u\n", port, p->len);

		erbium_status_code = coap_parse_message(message, p->payload, p->len);

		if (erbium_status_code == NO_ERROR) {
			message->addr = *addr;
			message->port = port;
			/*TODO duplicates suppression, if required by application */

			PRINTF("  Parsed: v %u, t %u, tkl %u, c %u, mid %u\n",
					message->version, message->type, message->token_len,
					message->code, message->mid);
			PRINTF("  URL: %.*s\n", message->uri_path_len, message->uri_path);
			PRINTF("  Payload: %.*s\n", message->payload_len, message->payload);

			/* handle requests */
			if (message->code >= COAP_GET && message->code <= COAP_DELETE) {

				/* use transaction buffer for response to confirmable request */
				if ((transaction = coap_new_transaction(message->mid, addr,
						port))) {
					uint32_t block_num = 0;
					uint16_t block_size = COAP_MAX_BLOCK_SIZE;
					uint32_t block_offset = 0;
					int32_t new_offset = 0;

					/* prepare response */
					if (message->type == COAP_TYPE_CON) {
						/* reliable CON requests are answered with an ACK */
						coap_init_message(response, COAP_TYPE_ACK, CONTENT_2_05,
								message->mid);
					} else {
						/* unreliable NON requests are answered with a NON as well */
						coap_init_message(response, COAP_TYPE_NON, CONTENT_2_05,
								coap_get_mid());
						/* mirror token */
					}
					if (message->token_len) {
						coap_set_token(response, message->token,
								message->token_len);
						/* get offset for blockwise transfers */
					}
					if (coap_get_header_block2(message, &block_num, NULL,
							&block_size, &block_offset)) {
						PRINTF(
								"Blockwise: block request %u (%u/%u) @ %u bytes\n",
								block_num, block_size, COAP_MAX_BLOCK_SIZE,
								block_offset);
						block_size = MIN(block_size, COAP_MAX_BLOCK_SIZE);
						new_offset = block_offset;
					}

					/* invoke resource handler */
					if (service_cbk) {

						/* call REST framework and check if found and allowed */
						if (service_cbk(message, response,
								transaction->packet + COAP_MAX_HEADER_SIZE,
								block_size, &new_offset)) {

							if (erbium_status_code == NO_ERROR) {

								/* TODO coap_handle_blockwise(request, response, start_offset, end_offset); */

								/* resource is unaware of Block1 */
								if (IS_OPTION(message,
										COAP_OPTION_BLOCK1) && response->code < BAD_REQUEST_4_00
										&& !IS_OPTION(response, COAP_OPTION_BLOCK1)) {
									PRINTF("Block1 NOT IMPLEMENTED\n");

									erbium_status_code = NOT_IMPLEMENTED_5_01;
									coap_error_message = "NoBlock1Support";

									/* client requested Block2 transfer */
								} else if (IS_OPTION(message,
										COAP_OPTION_BLOCK2)) {

									/* unchanged new_offset indicates that resource is unaware of blockwise transfer */
									if (new_offset == block_offset) {
										PRINTF(
												"Blockwise: unaware resource with payload length %u/%u\n",
												response->payload_len,
												block_size);
										if (block_offset
												>= response->payload_len) {
											PRINTF(
													"handle_incoming_data(): block_offset >= response->payload_len\n");

											response->code = BAD_OPTION_4_02;
											coap_set_payload(response,
													"BlockOutOfScope", 15); /* a const char str[] and sizeof(str) produces larger code size */
										} else {
											coap_set_header_block2(response,
													block_num,
													response->payload_len
															- block_offset
															> block_size,
													block_size);
											coap_set_payload(response,
													response->payload
															+ block_offset,
													MIN(
															response->payload_len
																	- block_offset,
															block_size));
										} /* if(valid offset) */

										/* resource provides chunk-wise data */
									} else {
										PRINTF(
												"Blockwise: blockwise resource, new offset %d\n",
												new_offset);
										coap_set_header_block2(response,
												block_num,
												new_offset != -1
														|| response->payload_len
																> block_size,
												block_size);

										if (response->payload_len
												> block_size) {
											coap_set_payload(response,
													response->payload,
													block_size);
										}
									} /* if(resource aware of blockwise) */

									/* Resource requested Block2 transfer */
								} else if (new_offset != 0) {
									PRINTF(
											"Blockwise: no block option for blockwise resource, using block size %u\n",
											COAP_MAX_BLOCK_SIZE);

									coap_set_header_block2(response, 0,
											new_offset != -1,
											COAP_MAX_BLOCK_SIZE);
									coap_set_payload(response,
											response->payload,
											MIN(response->payload_len,
													COAP_MAX_BLOCK_SIZE));
								} /* blockwise transfer handling */
							} /* no errors/hooks */
							/* successful service callback */
							/* serialize response */
						}
						if (erbium_status_code == NO_ERROR) {
							if ((transaction->packet_len =
									coap_serialize_message(response,
											transaction->packet,
											&transaction->addr,
											transaction->port)) == 0) {
								erbium_status_code = PACKET_SERIALIZATION_ERROR;
							}
						}
					} else {
						erbium_status_code = NOT_IMPLEMENTED_5_01;
						coap_error_message = "NoServiceCallbck"; /* no 'a' to fit into 16 bytes */
					} /* if(service callback) */
				} else {
					erbium_status_code = SERVICE_UNAVAILABLE_5_03;
					coap_error_message = "NoFreeTraBuffer";
				} /* if(transaction buffer) */

				/* handle responses */
			} else {

				if (message->type == COAP_TYPE_CON && message->code == 0) {
					PRINTF("Received Ping\n");
					erbium_status_code = PING_RESPONSE;
				} else if (message->type == COAP_TYPE_ACK) {
					/* transactions are closed through lookup below */
					PRINTF("Received ACK\n");
				} else if (message->type == COAP_TYPE_RST) {
					PRINTF("Received RST\n");
					/* cancel possible subscriptions */
					coap_remove_observer_by_mid(addr, port, message->mid);
				}

				if ((transaction = coap_get_transaction_by_mid(message->mid))) {
					PRINTF("transaction found\n");
					/* free transaction memory before callback, as it may create a new transaction */
					restful_response_handler callback = transaction->callback;
					void *callback_data = transaction->callback_data;

					coap_clear_transaction(transaction);
					PRINTF("clear_transaction\n");
					/* check if someone registered for the response */
					if (callback) {
						callback(callback_data, message);
					}
				}
				/* if(ACKed transaction) */
				transaction = NULL;

#if COAP_OBSERVE_CLIENT
				/* if observe notification */
				if((message->type == COAP_TYPE_CON || message->type == COAP_TYPE_NON)
						&& IS_OPTION(message, COAP_OPTION_OBSERVE)) {
					PRINTF("Observe [%u]\n", message->observe);
					coap_handle_notification(addr, port,
							message);
				}
#endif /* COAP_OBSERVE_CLIENT */
			} /* request or response */
		} /* parsed correctly */

		/* if(parsed correctly) */
		if (erbium_status_code == NO_ERROR) {
			if (transaction) {
				coap_send_transaction(transaction);
			}
		} else if (erbium_status_code == MANUAL_RESPONSE) {
			PRINTF("Clearing transaction for manual response");
			coap_clear_transaction(transaction);
		} else {
			coap_message_type_t reply_type = COAP_TYPE_ACK;

			PRINTF("ERROR %u: %s\n", erbium_status_code, coap_error_message);
			coap_clear_transaction(transaction);

			if (erbium_status_code == PING_RESPONSE) {
				erbium_status_code = 0;
				reply_type = COAP_TYPE_RST;
			} else if (erbium_status_code >= 192) {
				/* set to sendable error code */
				erbium_status_code = INTERNAL_SERVER_ERROR_5_00;
				/* reuse input buffer for error message */
			}
			coap_init_message(message, reply_type, erbium_status_code,
					message->mid);
			coap_set_payload(message, coap_error_message,
					strlen(coap_error_message));

			uint8_t buffer[COAP_MAX_PACKET_SIZE];
			size_t len = coap_serialize_message(message, buffer, addr, port);
			coap_send_message(addr, port, buffer, len);
		}
		pbuf_free(p);
	}

	/* if(new data) */
	return erbium_status_code;
}
/*---------------------------------------------------------------------------*/
void coap_init_engine(QueueHandle_t * queue) {
	receivequeue_ptr = queue;
	responsequeue = xQueueCreate(1, sizeof(uint8_t));
	xTaskCreate(coap_engine, "coap_engine", 256, NULL, 2, NULL);
}
/*---------------------------------------------------------------------------*/
void coap_set_service_callback(service_callback_t callback) {
	service_cbk = callback;
}
/*---------------------------------------------------------------------------*/
rest_resource_flags_t coap_get_rest_method(void *packet) {
	return (rest_resource_flags_t) (1 << (((coap_packet_t *) packet)->code - 1));
}
/*---------------------------------------------------------------------------*/
/*- Server Part -------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/* the discover resource is automatically included for CoAP */

extern resource_t res_well_known_core;
#ifdef WITH_DTLS
extern resource_t res_dtls;
#endif

/*---------------------------------------------------------------------------*/
void coap_engine(void *pvParameters) {

	PRINTF("Starting %s receiver...\n", coap_rest_implementation.name);

	rest_activate_resource(&res_well_known_core, ".well-known/core");

	coap_register_as_transaction_handler();
	coap_init_connection(SERVER_LISTEN_PORT);

	while (1) {
		received_item_t datagram;
		PRINTF("Before Queue peek\n");
		if (xQueuePeek(*receivequeue_ptr, &datagram, 1000)) {
			PRINTF("Queue peek\n");
			coap_receive();
		}
	}

}
/*---------------------------------------------------------------------------*/
/*- Client Part -------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void coap_blocking_request_callback(void *callback_data, void *response) {
	struct request_state_t *state = (struct request_state_t *) callback_data;
	PRINTF("coap_blocking_request_callback\n");
	state->response = (coap_packet_t *) response;
	uint8_t msg = 1;
	xQueueSend(responsequeue, &msg, 0);
//	process_poll(state->process);

}
/*---------------------------------------------------------------------------*/
void coap_blocking_request(void *pvParameters) {
	request_parameters_t *params = (request_parameters_t*) pvParameters;
	PRINTF("coap_blocking_request to ");
	struct request_state_t *state = &params->request_state;
	coap_packet_t *request = params->request;
	ip_addr_t *remote_ipaddr = params->remote_ipaddr;
	uint16_t remote_port = params->remote_port;
	blocking_response_handler request_callback = params->request_callback;
	PRINT4ADDR(remote_ipaddr);
	PRINTF(":%d\n", remote_port);
	static uint8_t more;
	static uint32_t res_block;
	static uint8_t block_error;

	state->block_num = 0;
	state->response = NULL;

	more = 0;
	res_block = 0;
	block_error = 0;

	do {
		request->mid = coap_get_mid();
		if ((state->transaction = coap_new_transaction(request->mid,
				remote_ipaddr, remote_port))) {
			state->transaction->callback = coap_blocking_request_callback;
			state->transaction->callback_data = state;

			if (state->block_num > 0) {
				coap_set_header_block2(request, state->block_num, 0,
				REST_MAX_CHUNK_SIZE);
			}
			state->transaction->packet_len = coap_serialize_message(request,
					state->transaction->packet, remote_ipaddr, remote_port);

			coap_send_transaction(state->transaction);
			PRINTF("Requested #%u (MID %u)\n", state->block_num, request->mid);
			uint8_t resp;

			/* Wait for the next event. */
			while (xQueueReceive(responsequeue, &resp, 1000) != pdTRUE) {

			}
			PRINTF("Response received\n");

//			PT_YIELD_UNTIL(&state->pt, ev == PROCESS_EVENT_POLL);

			if (!state->response) {
				PRINTF("Server not responding\n");
				return;
			}

			coap_get_header_block2(state->response, &res_block, &more, NULL,
			NULL);

			PRINTF("Received #%u%s (%u bytes)\n", res_block, more ? "+" : "",
					state->response->payload_len);

			if (res_block == state->block_num) {
				PRINTF("call callback");
				request_callback(state->response);
				++(state->block_num);
			} else {
				PRINTF("WRONG BLOCK %u/%u\n", res_block, state->block_num);
				++block_error;
			}
		} else {
			PRINTF("Could not allocate transaction buffer");
			return;
		}
	} while (more && block_error < COAP_MAX_ATTEMPTS);
	PRINTF("client end\n");
}
/*---------------------------------------------------------------------------*/
/*- REST Engine Interface ---------------------------------------------------*/
/*---------------------------------------------------------------------------*/
const struct rest_implementation coap_rest_implementation = { "CoAP-18",
		coap_init_engine, coap_set_service_callback, coap_get_header_uri_path,
		coap_get_rest_method, coap_set_status_code,
		coap_get_header_content_format, coap_set_header_content_format,
		coap_get_header_accept, coap_get_header_size2, coap_set_header_size2,
		coap_get_header_max_age, coap_set_header_max_age, coap_set_header_etag,
		coap_get_header_if_match, coap_get_header_if_none_match,
		coap_get_header_uri_host, coap_set_header_location_path,
		coap_get_payload, coap_set_payload, coap_get_header_uri_query,
		coap_get_query_variable, coap_get_post_variable, coap_notify_observers,
		coap_observe_handler,

		{ CONTENT_2_05, CREATED_2_01, CHANGED_2_04, DELETED_2_02, VALID_2_03,

		BAD_REQUEST_4_00, UNAUTHORIZED_4_01, BAD_OPTION_4_02, FORBIDDEN_4_03,
				NOT_FOUND_4_04, METHOD_NOT_ALLOWED_4_05, NOT_ACCEPTABLE_4_06,
				REQUEST_ENTITY_TOO_LARGE_4_13, UNSUPPORTED_MEDIA_TYPE_4_15,

				INTERNAL_SERVER_ERROR_5_00, NOT_IMPLEMENTED_5_01,
				BAD_GATEWAY_5_02, SERVICE_UNAVAILABLE_5_03,
				GATEWAY_TIMEOUT_5_04, PROXYING_NOT_SUPPORTED_5_05 },

		{ TEXT_PLAIN, TEXT_XML, TEXT_CSV, TEXT_HTML, IMAGE_GIF, IMAGE_JPEG,
				IMAGE_PNG, IMAGE_TIFF, AUDIO_RAW, VIDEO_RAW,
				APPLICATION_LINK_FORMAT, APPLICATION_XML,
				APPLICATION_OCTET_STREAM, APPLICATION_RDF_XML,
				APPLICATION_SOAP_XML, APPLICATION_ATOM_XML,
				APPLICATION_XMPP_XML, APPLICATION_EXI, APPLICATION_FASTINFOSET,
				APPLICATION_SOAP_FASTINFOSET, APPLICATION_JSON,
				APPLICATION_X_OBIX_BINARY } };
/*---------------------------------------------------------------------------*/
