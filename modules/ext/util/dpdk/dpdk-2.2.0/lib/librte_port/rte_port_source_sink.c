/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdint.h>
#include <string.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

#include "rte_port_source_sink.h"

/*
 * Port SOURCE
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_SOURCE_STATS_PKTS_IN_ADD(port, val) \
	port->stats.n_pkts_in += val
#define RTE_PORT_SOURCE_STATS_PKTS_DROP_ADD(port, val) \
	port->stats.n_pkts_drop += val

#else

#define RTE_PORT_SOURCE_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_SOURCE_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_source {
	struct rte_port_in_stats stats;

	struct rte_mempool *mempool;
};

static void *
rte_port_source_create(void *params, int socket_id)
{
	struct rte_port_source_params *p =
			(struct rte_port_source_params *) params;
	struct rte_port_source *port;

	/* Check input arguments*/
	if ((p == NULL) || (p->mempool == NULL)) {
		RTE_LOG(ERR, PORT, "%s: Invalid params\n", __func__);
		return NULL;
	}

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate port\n", __func__);
		return NULL;
	}

	/* Initialization */
	port->mempool = (struct rte_mempool *) p->mempool;

	return port;
}

static int
rte_port_source_free(void *port)
{
	/* Check input parameters */
	if (port == NULL)
		return 0;

	rte_free(port);

	return 0;
}

static int
rte_port_source_rx(void *port, struct rte_mbuf **pkts, uint32_t n_pkts)
{
	struct rte_port_source *p = (struct rte_port_source *) port;
	uint32_t i;

	if (rte_mempool_get_bulk(p->mempool, (void **) pkts, n_pkts) != 0)
		return 0;

	for (i = 0; i < n_pkts; i++) {
		rte_mbuf_refcnt_set(pkts[i], 1);
		rte_pktmbuf_reset(pkts[i]);
	}

	RTE_PORT_SOURCE_STATS_PKTS_IN_ADD(p, n_pkts);

	return n_pkts;
}

static int
rte_port_source_stats_read(void *port,
		struct rte_port_in_stats *stats, int clear)
{
	struct rte_port_source *p =
		(struct rte_port_source *) port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Port SINK
 */
#ifdef RTE_PORT_STATS_COLLECT

#define RTE_PORT_SINK_STATS_PKTS_IN_ADD(port, val) \
	(port->stats.n_pkts_in += val)
#define RTE_PORT_SINK_STATS_PKTS_DROP_ADD(port, val) \
	(port->stats.n_pkts_drop += val)

#else

#define RTE_PORT_SINK_STATS_PKTS_IN_ADD(port, val)
#define RTE_PORT_SINK_STATS_PKTS_DROP_ADD(port, val)

#endif

struct rte_port_sink {
	struct rte_port_out_stats stats;
};

static void *
rte_port_sink_create(__rte_unused void *params, int socket_id)
{
	struct rte_port_sink *port;

	/* Memory allocation */
	port = rte_zmalloc_socket("PORT", sizeof(*port),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (port == NULL) {
		RTE_LOG(ERR, PORT, "%s: Failed to allocate port\n", __func__);
		return NULL;
	}

	return port;
}

static int
rte_port_sink_tx(void *port, struct rte_mbuf *pkt)
{
	__rte_unused struct rte_port_sink *p = (struct rte_port_sink *) port;

	RTE_PORT_SINK_STATS_PKTS_IN_ADD(p, 1);
	rte_pktmbuf_free(pkt);
	RTE_PORT_SINK_STATS_PKTS_DROP_ADD(p, 1);

	return 0;
}

static int
rte_port_sink_tx_bulk(void *port, struct rte_mbuf **pkts,
	uint64_t pkts_mask)
{
	__rte_unused struct rte_port_sink *p = (struct rte_port_sink *) port;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_mask);
		uint32_t i;

		RTE_PORT_SINK_STATS_PKTS_IN_ADD(p, n_pkts);
		RTE_PORT_SINK_STATS_PKTS_DROP_ADD(p, n_pkts);
		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = pkts[i];

			rte_pktmbuf_free(pkt);
		}
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = __builtin_ctzll(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];

			RTE_PORT_SINK_STATS_PKTS_IN_ADD(p, 1);
			RTE_PORT_SINK_STATS_PKTS_DROP_ADD(p, 1);
			rte_pktmbuf_free(pkt);
			pkts_mask &= ~pkt_mask;
		}
	}

	return 0;
}

static int
rte_port_sink_stats_read(void *port, struct rte_port_out_stats *stats,
		int clear)
{
	struct rte_port_sink *p =
		(struct rte_port_sink *) port;

	if (stats != NULL)
		memcpy(stats, &p->stats, sizeof(p->stats));

	if (clear)
		memset(&p->stats, 0, sizeof(p->stats));

	return 0;
}

/*
 * Summary of port operations
 */
struct rte_port_in_ops rte_port_source_ops = {
	.f_create = rte_port_source_create,
	.f_free = rte_port_source_free,
	.f_rx = rte_port_source_rx,
	.f_stats = rte_port_source_stats_read,
};

struct rte_port_out_ops rte_port_sink_ops = {
	.f_create = rte_port_sink_create,
	.f_free = NULL,
	.f_tx = rte_port_sink_tx,
	.f_tx_bulk = rte_port_sink_tx_bulk,
	.f_flush = NULL,
	.f_stats = rte_port_sink_stats_read,
};
