/*
 * Copyright (c) 2003 Bruce M. Simpson <bms@spc.org>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by Bruce M. Simpson.
 * 4. Neither the name of Bruce M. Simpson nor the names of co-
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bruce M. Simpson AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL Bruce M. Simpson OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-aodv.c,v 1.11 2004-03-24 00:30:19 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

#include "aodv.h"

static void
aodv_extension(const struct aodv_ext *ep, u_int length)
{
	const struct aodv_hello *ah;

	switch (ep->type) {
	case AODV_EXT_HELLO:
		ah = (const struct aodv_hello *)(const void *)ep;
		TCHECK(*ah);
		if (length < sizeof(struct aodv_hello))
			goto trunc;
		printf("\n\text HELLO %ld ms",
		    (unsigned long)EXTRACT_32BITS(&ah->interval));
		break;

	default:
		printf("\n\text %u %u", ep->type, ep->length);
		break;
	}
	return;

trunc:
	printf(" [|hello]");
}

static void
aodv_rreq(const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rreq *ap = (const struct aodv_rreq *)dat;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rreq %u %s%s%s%s%shops %u id 0x%08lx\n"
	    "\tdst %s seq %lu src %s seq %lu", length,
	    ap->rreq_type & RREQ_JOIN ? "[J]" : "",
	    ap->rreq_type & RREQ_REPAIR ? "[R]" : "",
	    ap->rreq_type & RREQ_GRAT ? "[G]" : "",
	    ap->rreq_type & RREQ_DEST ? "[D]" : "",
	    ap->rreq_type & RREQ_UNKNOWN ? "[U] " : " ",
	    ap->rreq_hops,
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_id),
	    ipaddr_string(&ap->rreq_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_ds),
	    ipaddr_string(&ap->rreq_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_os));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension((const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	printf(" [|rreq");
}

static void
aodv_rrep(const u_char *dat, u_int length)
{
	u_int i;
	const struct aodv_rrep *ap = (const struct aodv_rrep *)dat;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rrep %u %s%sprefix %u hops %u\n"
	    "\tdst %s dseq %lu src %s %lu ms", length,
	    ap->rrep_type & RREP_REPAIR ? "[R]" : "",
	    ap->rrep_type & RREP_ACK ? "[A] " : " ",
	    ap->rrep_ps & RREP_PREFIX_MASK,
	    ap->rrep_hops,
	    ipaddr_string(&ap->rrep_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep_ds),
	    ipaddr_string(&ap->rrep_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep_life));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension((const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	printf(" [|rreq");
}

static void
aodv_rerr(const u_char *dat, u_int length)
{
	u_int i, dc;
	const struct aodv_rerr *ap = (const struct aodv_rerr *)dat;
	const struct rerr_unreach *dp;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rerr %s [items %u] [%u]:",
	    ap->rerr_flags & RERR_NODELETE ? "[D]" : "",
	    ap->rerr_dc, length);
	dp = (struct rerr_unreach *)(dat + sizeof(*ap));
	i = length - sizeof(*ap);
	for (dc = ap->rerr_dc; dc != 0; dc--) {
		TCHECK(*dp);
		if (i < sizeof(*dp))
			goto trunc;
		printf(" {%s}(%ld)", ipaddr_string(&dp->u_da),
		    (unsigned long)EXTRACT_32BITS(&dp->u_ds));
		dp++;
		i -= sizeof(*dp);
	}
	return;

trunc:
	printf("[|rerr]");
}

static void
#ifdef INET6
aodv_v6_rreq(const u_char *dat, u_int length)
#else
aodv_v6_rreq(const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i;
	const struct aodv_rreq6 *ap = (const struct aodv_rreq6 *)dat;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" v6 rreq %u %s%s%s%s%shops %u id 0x%08lx\n"
	    "\tdst %s seq %lu src %s seq %lu", length,
	    ap->rreq_type & RREQ_JOIN ? "[J]" : "",
	    ap->rreq_type & RREQ_REPAIR ? "[R]" : "",
	    ap->rreq_type & RREQ_GRAT ? "[G]" : "",
	    ap->rreq_type & RREQ_DEST ? "[D]" : "",
	    ap->rreq_type & RREQ_UNKNOWN ? "[U] " : " ",
	    ap->rreq_hops,
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_id),
	    ip6addr_string(&ap->rreq_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_ds),
	    ip6addr_string(&ap->rreq_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_os));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension((const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	printf(" [|rreq");
#else
	printf(" v6 rreq %u", length);
#endif
}

static void
#ifdef INET6
aodv_v6_rrep(const u_char *dat, u_int length)
#else
aodv_v6_rrep(const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i;
	const struct aodv_rrep6 *ap = (const struct aodv_rrep6 *)dat;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rrep %u %s%sprefix %u hops %u\n"
	   "\tdst %s dseq %lu src %s %lu ms", length,
	    ap->rrep_type & RREP_REPAIR ? "[R]" : "",
	    ap->rrep_type & RREP_ACK ? "[A] " : " ",
	    ap->rrep_ps & RREP_PREFIX_MASK,
	    ap->rrep_hops,
	    ip6addr_string(&ap->rrep_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep_ds),
	    ip6addr_string(&ap->rrep_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep_life));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension((const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	printf(" [|rreq");
#else
	printf(" rrep %u", length);
#endif
}

static void
#ifdef INET6
aodv_v6_rerr(const u_char *dat, u_int length)
#else
aodv_v6_rerr(const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i, dc;
	const struct aodv_rerr *ap = (const struct aodv_rerr *)dat;
	const struct rerr_unreach6 *dp6;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rerr %s [items %u] [%u]:",
	    ap->rerr_flags & RERR_NODELETE ? "[D]" : "",
	    ap->rerr_dc, length);
	dp6 = (struct rerr_unreach6 *)(void *)(ap + 1);
	i = length - sizeof(*ap);
	for (dc = ap->rerr_dc; dc != 0; dc--) {
		TCHECK(*dp6);
		if (i < sizeof(*dp6))
			goto trunc;
		printf(" {%s}(%ld)", ip6addr_string(&dp6->u_da),
		    (unsigned long)EXTRACT_32BITS(&dp6->u_ds));
		dp6++;
		i -= sizeof(*dp6);
	}
	return;

trunc:
	printf("[|rerr]");
#else
	printf(" rerr %u", length);
#endif
}

static void
#ifdef INET6
aodv_v6_draft_01_rreq(const u_char *dat, u_int length)
#else
aodv_v6_draft_01_rreq(const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i;
	const struct aodv_rreq6_draft_01 *ap = (const struct aodv_rreq6_draft_01 *)dat;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rreq %u %s%s%s%s%shops %u id 0x%08lx\n"
	    "\tdst %s seq %lu src %s seq %lu", length,
	    ap->rreq_type & RREQ_JOIN ? "[J]" : "",
	    ap->rreq_type & RREQ_REPAIR ? "[R]" : "",
	    ap->rreq_type & RREQ_GRAT ? "[G]" : "",
	    ap->rreq_type & RREQ_DEST ? "[D]" : "",
	    ap->rreq_type & RREQ_UNKNOWN ? "[U] " : " ",
	    ap->rreq_hops,
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_id),
	    ip6addr_string(&ap->rreq_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_ds),
	    ip6addr_string(&ap->rreq_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rreq_os));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension((const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	printf(" [|rreq");
#else
	printf(" rreq %u", length);
#endif
}

static void
#ifdef INET6
aodv_v6_draft_01_rrep(const u_char *dat, u_int length)
#else
aodv_v6_draft_01_rrep(const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i;
	const struct aodv_rrep6_draft_01 *ap = (const struct aodv_rrep6_draft_01 *)dat;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rrep %u %s%sprefix %u hops %u\n"
	   "\tdst %s dseq %lu src %s %lu ms", length,
	    ap->rrep_type & RREP_REPAIR ? "[R]" : "",
	    ap->rrep_type & RREP_ACK ? "[A] " : " ",
	    ap->rrep_ps & RREP_PREFIX_MASK,
	    ap->rrep_hops,
	    ip6addr_string(&ap->rrep_da),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep_ds),
	    ip6addr_string(&ap->rrep_oa),
	    (unsigned long)EXTRACT_32BITS(&ap->rrep_life));
	i = length - sizeof(*ap);
	if (i >= sizeof(struct aodv_ext))
		aodv_extension((const struct aodv_ext *)(dat + sizeof(*ap)), i);
	return;

trunc:
	printf(" [|rreq");
#else
	printf(" rrep %u", length);
#endif
}

static void
#ifdef INET6
aodv_v6_draft_01_rerr(const u_char *dat, u_int length)
#else
aodv_v6_draft_01_rerr(const u_char *dat _U_, u_int length)
#endif
{
#ifdef INET6
	u_int i, dc;
	const struct aodv_rerr *ap = (const struct aodv_rerr *)dat;
	const struct rerr_unreach6_draft_01 *dp6;

	TCHECK(*ap);
	if (length < sizeof(*ap))
		goto trunc;
	printf(" rerr %s [items %u] [%u]:",
	    ap->rerr_flags & RERR_NODELETE ? "[D]" : "",
	    ap->rerr_dc, length);
	dp6 = (struct rerr_unreach6_draft_01 *)(void *)(ap + 1);
	i = length - sizeof(*ap);
	for (dc = ap->rerr_dc; dc != 0; dc--) {
		TCHECK(*dp6);
		if (i < sizeof(*dp6))
			goto trunc;
		printf(" {%s}(%ld)", ip6addr_string(&dp6->u_da),
		    (unsigned long)EXTRACT_32BITS(&dp6->u_ds));
		dp6++;
		i -= sizeof(*dp6);
	}
	return;

trunc:
	printf("[|rerr]");
#else
	printf(" rerr %u", length);
#endif
}

void
aodv_print(const u_char *dat, u_int length, int is_ip6)
{
	uint8_t msg_type;

	/*
	 * The message type is the first byte; make sure we have it
	 * and then fetch it.
	 */
	TCHECK(*dat);
	msg_type = *dat;
	printf(" aodv");

	switch (msg_type) {

	case AODV_RREQ:
		if (is_ip6)
			aodv_v6_rreq(dat, length);
		else
			aodv_rreq(dat, length);
		break;

	case AODV_RREP:
		if (is_ip6)
			aodv_v6_rrep(dat, length);
		else
			aodv_rrep(dat, length);
		break;

	case AODV_RERR:
		if (is_ip6)
			aodv_v6_rerr(dat, length);
		else
			aodv_rerr(dat, length);
		break;

	case AODV_RREP_ACK:
		printf(" rrep-ack %u", length);
		break;

	case AODV_V6_DRAFT_01_RREQ:
		aodv_v6_draft_01_rreq(dat, length);
		break;

	case AODV_V6_DRAFT_01_RREP:
		aodv_v6_draft_01_rrep(dat, length);
		break;

	case AODV_V6_DRAFT_01_RERR:
		aodv_v6_draft_01_rerr(dat, length);
		break;

	case AODV_V6_DRAFT_01_RREP_ACK:
		printf(" rrep-ack %u", length);
		break;

	default:
		printf(" type %u %u", msg_type, length);
	}
	return;

trunc:
	printf(" [|aodv]");
}
