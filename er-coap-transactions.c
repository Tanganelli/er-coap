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
 *      CoAP module for reliable transport
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include "er-coap-transactions.h"
#include "er-coap-observe.h"
#include "contiki-list.h"
#include "memb.h"

#define COAP_DEBUG 0
#if COAP_DEBUG
#include <stdio.h>
#include <ip_addr.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) ip_addr_debug_print(COAP_DEBUG, addr)
#define PRINT4ADDR(addr) printf("%u.%u.%u.%u", addr != NULL ? ip4_addr1_16(addr) : 0, addr != NULL ? ip4_addr2_16(addr) : 0, addr != NULL ? ip4_addr3_16(addr) : 0, addr != NULL ? ip4_addr4_16(addr) : 0);
#define PRINTLLADDR(addr)
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINT4ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/*---------------------------------------------------------------------------*/
MEMB(transactions_memb, coap_transaction_t, COAP_MAX_OPEN_TRANSACTIONS);
LIST(transactions_list);
static int initialized = 0;

//static struct process *transaction_handler_process = NULL;

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void coap_register_as_transaction_handler() {
	//TODO
//  transaction_handler_process = PROCESS_CURRENT();
}
coap_transaction_t *
coap_new_transaction(uint16_t mid, ip_addr_t *addr, uint16_t port) {
	if (!initialized) {
		memb_init(&transactions_memb);
		list_init(transactions_list);
		initialized = 1;
	}
	coap_transaction_t *t = memb_alloc(&transactions_memb);

	if (t) {
		t->mid = mid;
		t->retrans_counter = 0;
		uint32_t wait_time = (COAP_RESPONSE_TIMEOUT
				+ (rand() % COAP_RESPONSE_TIMEOUT_BACKOFF_MASK )) * 1000;
		t->retrans_timer = xTimerCreate("retransmitTimer",
				pdMS_TO_TICKS(wait_time), pdFALSE, t, coap_check_transactions);
		/* save client address */
		ip_addr_copy(t->addr, *addr);
		t->port = port;

		list_add(transactions_list, t); /* list itself makes sure same element is not added twice */
	}

	return t;
}
/*---------------------------------------------------------------------------*/
void coap_send_transaction(coap_transaction_t *t) {
	PRINTF("Sending transaction %u\n", t->mid);

	coap_send_message(&t->addr, t->port, t->packet, t->packet_len);

	if (COAP_TYPE_CON
			== ((COAP_HEADER_TYPE_MASK & t->packet[0])
					>> COAP_HEADER_TYPE_POSITION)) {
		if (t->retrans_counter < COAP_MAX_RETRANSMIT) {
			/* not timed out yet */
			PRINTF("Keeping transaction %u\n", t->mid);

			if (t->retrans_counter == 0) {
				xTimerStart(t->retrans_timer, 0); PRINTF("Initial interval %u\n",
						(xTimerGetPeriod(t->retrans_timer)));

			} else {
				TickType_t new_period = xTimerGetPeriod(t->retrans_timer) * 2;
				xTimerChangePeriod(t->retrans_timer, new_period, 0);
				xTimerStart(t->retrans_timer, 0); PRINTF("Doubled (%u) interval %u\n", t->retrans_counter,
						xTimerGetPeriod(t->retrans_timer));
			}

			t = NULL;
		} else {
			/* timed out */
			PRINTF("Timeout\n");
			restful_response_handler callback = t->callback;
			void *callback_data = t->callback_data;

			/* handle observers */
			coap_remove_observer_by_client(&t->addr, t->port);

			coap_clear_transaction(t);

			if (callback) {
				callback(callback_data, NULL);
			}
		}
	} else {
		coap_clear_transaction(t);
	}
}
/*---------------------------------------------------------------------------*/
void coap_clear_transaction(coap_transaction_t *t) {
	if (t) {
		PRINTF("Freeing transaction %u: %p\n", t->mid, t);

		xTimerDelete(t->retrans_timer, 0);
		list_remove(transactions_list, t);
		memb_free(&transactions_memb, t);
	}
}
coap_transaction_t *
coap_get_transaction_by_mid(uint16_t mid) {
	coap_transaction_t *t = NULL;

	for (t = (coap_transaction_t *) list_head(transactions_list); t; t =
			t->next) {
		if (t->mid == mid) {
			PRINTF("Found transaction for MID %u: %p\n", t->mid, t);
			return t;
		}
	}
	return NULL;
}
/*---------------------------------------------------------------------------*/
void coap_check_transactions(TimerHandle_t xTimer) {
	coap_transaction_t *t = (coap_transaction_t *) pvTimerGetTimerID(xTimer);
	++(t->retrans_counter);
	PRINTF("Retransmitting %u (%u)\n", t->mid, t->retrans_counter);
	coap_send_transaction(t);
}
/*---------------------------------------------------------------------------*/
