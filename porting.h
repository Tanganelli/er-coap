/*
 * Copyright (c) 2017, Department of Computer Engineering, UniPi
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
 *
 */

/**
 * \file
 *      A porting of erbium CoAP on the ESP8266
 * \author
 *      Giacomo Tanganelli <g.tanganelli@iet.unipi.it>
 */


#ifndef PORTING_H_
#define PORTING_H_

#include <FreeRTOS.h>
#include <timers.h>
#include <queue.h>
#include <ip_addr.h>
#include <lwip/api.h>
#include <lwip/sys.h>
#include <lwip/udp.h>


#ifndef MAX
#define MAX(n, m)   (((n) < (m)) ? (m) : (n))
#endif

#ifndef MIN
#define MIN(n, m)   (((n) < (m)) ? (n) : (m))
#endif

#ifndef ABS
#define ABS(n)      (((n) < 0) ? -(n) : (n))
#endif

typedef struct{
	struct pbuf *p;
	struct ip_addr addr;
	u16_t port;
}received_item_t;


#endif /* PORTING_H_ */
