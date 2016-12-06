/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
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

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>

#include <rte_eal.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_version.h>
#include <rte_devargs.h>

#include "eal_internal_cfg.h"
#include "eal_options.h"

#define BITS_PER_HEX 4

const char
eal_short_options[] =
	"b:" /* pci-blacklist */
	"w:" /* pci-whitelist */
	"c:"
	"d:"
	"m:"
	"n:"
	"r:"
	"v";

const struct option
eal_long_options[] = {
	{OPT_HUGE_DIR, 1, 0, OPT_HUGE_DIR_NUM},
	{OPT_PROC_TYPE, 1, 0, OPT_PROC_TYPE_NUM},
	{OPT_NO_SHCONF, 0, 0, OPT_NO_SHCONF_NUM},
	{OPT_NO_HPET, 0, 0, OPT_NO_HPET_NUM},
	{OPT_VMWARE_TSC_MAP, 0, 0, OPT_VMWARE_TSC_MAP_NUM},
	{OPT_NO_PCI, 0, 0, OPT_NO_PCI_NUM},
	{OPT_NO_HUGE, 0, 0, OPT_NO_HUGE_NUM},
	{OPT_FILE_PREFIX, 1, 0, OPT_FILE_PREFIX_NUM},
	{OPT_SOCKET_MEM, 1, 0, OPT_SOCKET_MEM_NUM},
	{OPT_PCI_WHITELIST, 1, 0, OPT_PCI_WHITELIST_NUM},
	{OPT_PCI_BLACKLIST, 1, 0, OPT_PCI_BLACKLIST_NUM},
	{OPT_VDEV, 1, 0, OPT_VDEV_NUM},
	{OPT_SYSLOG, 1, NULL, OPT_SYSLOG_NUM},
	{OPT_LOG_LEVEL, 1, NULL, OPT_LOG_LEVEL_NUM},
	{OPT_BASE_VIRTADDR, 1, 0, OPT_BASE_VIRTADDR_NUM},
	{OPT_XEN_DOM0, 0, 0, OPT_XEN_DOM0_NUM},
	{OPT_CREATE_UIO_DEV, 1, NULL, OPT_CREATE_UIO_DEV_NUM},
	{OPT_VFIO_INTR, 1, NULL, OPT_VFIO_INTR_NUM},
	{0, 0, 0, 0}
};

/*
 * Parse the coremask given as argument (hexadecimal string) and fill
 * the global configuration (core role and core count) with the parsed
 * value.
 */
static int xdigit2val(unsigned char c)
{
	int val;

	if (isdigit(c))
		val = c - '0';
	else if (isupper(c))
		val = c - 'A' + 10;
	else
		val = c - 'a' + 10;
	return val;
}

static int
eal_parse_coremask(const char *coremask)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	int i, j, idx = 0;
	unsigned count = 0;
	char c;
	int val;

	if (coremask == NULL)
		return -1;
	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*coremask))
		coremask++;
	if (coremask[0] == '0' && ((coremask[1] == 'x')
		|| (coremask[1] == 'X')))
		coremask += 2;
	i = strnlen(coremask, PATH_MAX);
	while ((i > 0) && isblank(coremask[i - 1]))
		i--;
	if (i == 0)
		return -1;

	for (i = i - 1; i >= 0 && idx < RTE_MAX_LCORE; i--) {
		c = coremask[i];
		if (isxdigit(c) == 0) {
			/* invalid characters */
			return -1;
		}
		val = xdigit2val(c);
		for (j = 0; j < BITS_PER_HEX && idx < RTE_MAX_LCORE; j++, idx++)
		{
			if ((1 << j) & val) {
				if (!lcore_config[idx].detected) {
					RTE_LOG(ERR, EAL, "lcore %u "
					        "unavailable\n", idx);
					return -1;
				}
				cfg->lcore_role[idx] = ROLE_RTE;
				if (count == 0)
					cfg->master_lcore = idx;
				count++;
			} else {
				cfg->lcore_role[idx] = ROLE_OFF;
			}
		}
	}
	for (; i >= 0; i--)
		if (coremask[i] != '0')
			return -1;
	for (; idx < RTE_MAX_LCORE; idx++)
		cfg->lcore_role[idx] = ROLE_OFF;
	if (count == 0)
		return -1;
	/* Update the count of enabled logical cores of the EAL configuration */
	cfg->lcore_count = count;
	return 0;
}

static int
eal_parse_syslog(const char *facility, struct internal_config *conf)
{
	int i;
	static struct {
		const char *name;
		int value;
	} map[] = {
		{ "auth", LOG_AUTH },
		{ "cron", LOG_CRON },
		{ "daemon", LOG_DAEMON },
		{ "ftp", LOG_FTP },
		{ "kern", LOG_KERN },
		{ "lpr", LOG_LPR },
		{ "mail", LOG_MAIL },
		{ "news", LOG_NEWS },
		{ "syslog", LOG_SYSLOG },
		{ "user", LOG_USER },
		{ "uucp", LOG_UUCP },
		{ "local0", LOG_LOCAL0 },
		{ "local1", LOG_LOCAL1 },
		{ "local2", LOG_LOCAL2 },
		{ "local3", LOG_LOCAL3 },
		{ "local4", LOG_LOCAL4 },
		{ "local5", LOG_LOCAL5 },
		{ "local6", LOG_LOCAL6 },
		{ "local7", LOG_LOCAL7 },
		{ NULL, 0 }
	};

	for (i = 0; map[i].name; i++) {
		if (!strcmp(facility, map[i].name)) {
			conf->syslog_facility = map[i].value;
			return 0;
		}
	}
	return -1;
}

static int
eal_parse_log_level(const char *level, uint32_t *log_level)
{
	char *end;
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(level, &end, 0);

	/* check for errors */
	if ((errno != 0) || (level[0] == '\0') ||
	    end == NULL || (*end != '\0'))
		return -1;

	/* log_level is a uint32_t */
	if (tmp >= UINT32_MAX)
		return -1;

	*log_level = tmp;
	return 0;
}

static enum rte_proc_type_t
eal_parse_proc_type(const char *arg)
{
	if (strncasecmp(arg, "primary", sizeof("primary")) == 0)
		return RTE_PROC_PRIMARY;
	if (strncasecmp(arg, "secondary", sizeof("secondary")) == 0)
		return RTE_PROC_SECONDARY;
	if (strncasecmp(arg, "auto", sizeof("auto")) == 0)
		return RTE_PROC_AUTO;

	return RTE_PROC_INVALID;
}

int
eal_parse_common_option(int opt, const char *optarg,
			struct internal_config *conf)
{
	switch (opt) {
	/* blacklist */
	case 'b':
		if (rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_PCI,
				optarg) < 0) {
			return -1;
		}
		break;
	/* whitelist */
	case 'w':
		if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI,
				optarg) < 0) {
			return -1;
		}
		break;
	/* coremask */
	case 'c':
		if (eal_parse_coremask(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid coremask\n");
			return -1;
		}
		break;
	/* size of memory */
	case 'm':
		conf->memory = atoi(optarg);
		conf->memory *= 1024ULL;
		conf->memory *= 1024ULL;
		break;
	/* force number of channels */
	case 'n':
		conf->force_nchannel = atoi(optarg);
		if (conf->force_nchannel == 0 ||
		    conf->force_nchannel > 4) {
			RTE_LOG(ERR, EAL, "invalid channel number\n");
			return -1;
		}
		break;
	/* force number of ranks */
	case 'r':
		conf->force_nrank = atoi(optarg);
		if (conf->force_nrank == 0 ||
		    conf->force_nrank > 16) {
			RTE_LOG(ERR, EAL, "invalid rank number\n");
			return -1;
		}
		break;
	case 'v':
		/* since message is explicitly requested by user, we
		 * write message at highest log level so it can always
		 * be seen
		 * even if info or warning messages are disabled */
		RTE_LOG(CRIT, EAL, "RTE Version: '%s'\n", rte_version());
		break;

	/* long options */
	case OPT_NO_HUGE_NUM:
		conf->no_hugetlbfs = 1;
		break;

	case OPT_NO_PCI_NUM:
		conf->no_pci = 1;
		break;

	case OPT_NO_HPET_NUM:
		conf->no_hpet = 1;
		break;

	case OPT_VMWARE_TSC_MAP_NUM:
		conf->vmware_tsc_map = 1;
		break;

	case OPT_NO_SHCONF_NUM:
		conf->no_shconf = 1;
		break;

	case OPT_PROC_TYPE_NUM:
		conf->process_type = eal_parse_proc_type(optarg);
		break;

	case OPT_VDEV_NUM:
		if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL,
				optarg) < 0) {
			return -1;
		}
		break;

	case OPT_SYSLOG_NUM:
		if (eal_parse_syslog(optarg, conf) < 0) {
			RTE_LOG(ERR, EAL, "invalid parameters for --"
					OPT_SYSLOG "\n");
			return -1;
		}
		break;

	case OPT_LOG_LEVEL_NUM: {
		uint32_t log;

		if (eal_parse_log_level(optarg, &log) < 0) {
			RTE_LOG(ERR, EAL,
				"invalid parameters for --"
				OPT_LOG_LEVEL "\n");
			return -1;
		}
		conf->log_level = log;
		break;
	}

	/* don't know what to do, leave this to caller */
	default:
		return 1;

	}

	return 0;
}

void
eal_common_usage(void)
{
	printf("-c COREMASK -n NUM [-m NB] [-r NUM] [-b <domain:bus:devid.func>]"
	       "[--proc-type primary|secondary|auto]\n\n"
	       "EAL common options:\n"
	       "  -c COREMASK  : A hexadecimal bitmask of cores to run on\n"
	       "  -n NUM       : Number of memory channels\n"
	       "  -v           : Display version information on startup\n"
	       "  -m MB        : memory to allocate (see also --"OPT_SOCKET_MEM")\n"
	       "  -r NUM       : force number of memory ranks (don't detect)\n"
	       "  --"OPT_SYSLOG"     : set syslog facility\n"
	       "  --"OPT_LOG_LEVEL"  : set default log level\n"
	       "  --"OPT_PROC_TYPE"  : type of this process\n"
	       "  --"OPT_PCI_BLACKLIST", -b: add a PCI device in black list.\n"
	       "               Prevent EAL from using this PCI device. The argument\n"
	       "               format is <domain:bus:devid.func>.\n"
	       "  --"OPT_PCI_WHITELIST", -w: add a PCI device in white list.\n"
	       "               Only use the specified PCI devices. The argument format\n"
	       "               is <[domain:]bus:devid.func>. This option can be present\n"
	       "               several times (once per device).\n"
	       "               [NOTE: PCI whitelist cannot be used with -b option]\n"
	       "  --"OPT_VDEV": add a virtual device.\n"
	       "               The argument format is <driver><id>[,key=val,...]\n"
	       "               (ex: --vdev=eth_pcap0,iface=eth2).\n"
	       "  --"OPT_VMWARE_TSC_MAP": use VMware TSC map instead of native RDTSC\n"
	       "\nEAL options for DEBUG use only:\n"
	       "  --"OPT_NO_HUGE"  : use malloc instead of hugetlbfs\n"
	       "  --"OPT_NO_PCI"   : disable pci\n"
	       "  --"OPT_NO_HPET"  : disable hpet\n"
	       "  --"OPT_NO_SHCONF": no shared config (mmap'd files)\n"
	       "\n");
}
