/*
 * porting.h
 *
 *  Created on: 13 feb 2017
 *      Author: giacomo
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

#ifndef NULL
#define NULL 0
#endif /* NULL */

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
