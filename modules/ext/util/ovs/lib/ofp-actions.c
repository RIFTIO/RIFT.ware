/*
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>
#include "ofp-actions.h"
#include "bundle.h"
#include "byte-order.h"
#include "compiler.h"
#include "dynamic-string.h"
#include "hmap.h"
#include "learn.h"
#include "meta-flow.h"
#include "multipath.h"
#include "nx-match.h"
#include "ofp-parse.h"
#include "ofp-util.h"
#include "ofpbuf.h"
#include "unaligned.h"
#include "util.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(ofp_actions);

static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);

struct ofp_action_header;

/* Raw identifiers for OpenFlow actions.
 *
 * Decoding and encoding OpenFlow actions across multiple versions is difficult
 * to do in a clean, consistent way.  This enumeration lays out all of the
 * forms of actions that Open vSwitch supports.
 *
 * The comments here must follow a stylized form because the
 * "extract-ofp-actions" program parses them at build time to generate data
 * tables.
 *
 *   - The first part of each comment specifies the vendor, OpenFlow versions,
 *     and type for each protocol that supports the action:
 *
 *         # The vendor is OF for standard OpenFlow actions, NX for Nicira
 *           extension actions.  (Support for other vendors can be added, but
 *           it can't be done just based on a vendor ID definition alone
 *           because OpenFlow doesn't define a standard way to specify a
 *           subtype for vendor actions, so other vendors might do it different
 *           from Nicira.)
 *
 *         # The version can specify a specific OpenFlow version, a version
 *           range delimited by "-", or an open-ended range with "+".
 *
 *         # The type, in parentheses, is the action type number (for standard
 *           OpenFlow actions) or subtype (for vendor extension actions).
 *
 *         # Optionally one may add "is deprecated" followed by a
 *           human-readable reason in parentheses (which will be used in log
 *           messages), if a particular action should no longer be used.
 *
 *     Multiple such specifications may be separated by commas.
 *
 *   - The second part describes the action's wire format.  It may be:
 *
 *         # "struct <name>": The struct fully specifies the wire format.  The
 *           action is exactly the size of the struct.  (Thus, the struct must
 *           be an exact multiple of 8 bytes in size.)
 *
 *         # "struct <name>, ...": The struct specifies the beginning of the
 *           wire format.  An instance of the action is either the struct's
 *           exact size, or a multiple of 8 bytes longer.
 *
 *         # "uint<N>_t" or "ovs_be<N>": The action consists of a (standard or
 *           vendor extension) header, followed by 0 or more pad bytes to align
 *           to a multiple of <N> bits, followed by an argument of the given
 *           type, followed by 0 or more pad bytes to bring the total action up
 *           to a multiple of 8 bytes.
 *
 *         # "void": The action is just a (standard or vendor extension)
 *           header.
 *
 *   - Optional additional text enclosed in square brackets is commentary for
 *     the human reader.
 */
enum ofp_raw_action_type {
/* ## ----------------- ## */
/* ## Standard actions. ## */
/* ## ----------------- ## */

    /* OF1.0(0): struct ofp10_action_output. */
    OFPAT_RAW10_OUTPUT,
    /* OF1.1+(0): struct ofp11_action_output. */
    OFPAT_RAW11_OUTPUT,

    /* OF1.0(1): uint16_t. */
    OFPAT_RAW10_SET_VLAN_VID,
    /* OF1.0(2): uint8_t. */
    OFPAT_RAW10_SET_VLAN_PCP,

    /* OF1.1(1), OF1.2+(1) is deprecated (use Set-Field): uint16_t.
     *
     * [Semantics differ slightly between the 1.0 and 1.1 versions of the VLAN
     * modification actions: the 1.0 versions push a VLAN header if none is
     * present, but the 1.1 versions do not.  That is the only reason that we
     * distinguish their raw action types.] */
    OFPAT_RAW11_SET_VLAN_VID,
    /* OF1.1(2), OF1.2+(2) is deprecated (use Set-Field): uint8_t. */
    OFPAT_RAW11_SET_VLAN_PCP,

    /* OF1.1+(17): ovs_be16.
     *
     * [The argument is the Ethertype, e.g. ETH_TYPE_VLAN_8021Q, not the VID or
     * TCI.] */
    OFPAT_RAW11_PUSH_VLAN,

    /* OF1.0(3): void. */
    OFPAT_RAW10_STRIP_VLAN,
    /* OF1.1+(18): void. */
    OFPAT_RAW11_POP_VLAN,

    /* OF1.0(4), OF1.1(3), OF1.2+(3) is deprecated (use Set-Field): struct
     * ofp_action_dl_addr. */
    OFPAT_RAW_SET_DL_SRC,

    /* OF1.0(5), OF1.1(4), OF1.2+(4) is deprecated (use Set-Field): struct
     * ofp_action_dl_addr. */
    OFPAT_RAW_SET_DL_DST,

    /* OF1.0(6), OF1.1(5), OF1.2+(5) is deprecated (use Set-Field):
     * ovs_be32. */
    OFPAT_RAW_SET_NW_SRC,

    /* OF1.0(7), OF1.1(6), OF1.2+(6) is deprecated (use Set-Field):
     * ovs_be32. */
    OFPAT_RAW_SET_NW_DST,

    /* OF1.0(8), OF1.1(7), OF1.2+(7) is deprecated (use Set-Field): uint8_t. */
    OFPAT_RAW_SET_NW_TOS,

    /* OF1.1(8), OF1.2+(8) is deprecated (use Set-Field): uint8_t. */
    OFPAT_RAW11_SET_NW_ECN,

    /* OF1.0(9), OF1.1(9), OF1.2+(9) is deprecated (use Set-Field):
     * ovs_be16. */
    OFPAT_RAW_SET_TP_SRC,

    /* OF1.0(10), OF1.1(10), OF1.2+(10) is deprecated (use Set-Field):
     * ovs_be16. */
    OFPAT_RAW_SET_TP_DST,

    /* OF1.0(11): struct ofp10_action_enqueue. */
    OFPAT_RAW10_ENQUEUE,

    /* NX1.0(30), OF1.1(13), OF1.2+(13) is deprecated (use Set-Field):
     * ovs_be32. */
    OFPAT_RAW_SET_MPLS_LABEL,

    /* NX1.0(31), OF1.1(14), OF1.2+(14) is deprecated (use Set-Field):
     * uint8_t. */
    OFPAT_RAW_SET_MPLS_TC,

    /* NX1.0(25), OF1.1(15), OF1.2+(15) is deprecated (use Set-Field):
     * uint8_t. */
    OFPAT_RAW_SET_MPLS_TTL,

    /* NX1.0(26), OF1.1+(16): void. */
    OFPAT_RAW_DEC_MPLS_TTL,

    /* NX1.0(23), OF1.1+(19): ovs_be16.
     *
     * [The argument is the Ethertype, e.g. ETH_TYPE_MPLS, not the label.] */
    OFPAT_RAW_PUSH_MPLS,

    /* NX1.0(24), OF1.1+(20): ovs_be16.
     *
     * [The argument is the Ethertype, e.g. ETH_TYPE_IPV4 if at BoS or
     * ETH_TYPE_MPLS otherwise, not the label.] */
    OFPAT_RAW_POP_MPLS,

    /* NX1.0(4), OF1.1+(21): uint32_t. */
    OFPAT_RAW_SET_QUEUE,

    /* OF1.1+(22): uint32_t. */
    OFPAT_RAW11_GROUP,

    /* OF1.1+(23): uint8_t. */
    OFPAT_RAW11_SET_NW_TTL,

    /* NX1.0(18), OF1.1+(24): void. */
    OFPAT_RAW_DEC_NW_TTL,
    /* NX1.0+(21): struct nx_action_cnt_ids, ... */
    NXAST_RAW_DEC_TTL_CNT_IDS,

    /* OF1.2-1.4(25): struct ofp12_action_set_field, ... */
    OFPAT_RAW12_SET_FIELD,
    /* OF1.5+(25): struct ofp12_action_set_field, ... */
    OFPAT_RAW15_SET_FIELD,
    /* NX1.0-1.4(7): struct nx_action_reg_load.
     *
     * [In OpenFlow 1.5, set_field is a superset of reg_load functionality, so
     * we drop reg_load.] */
    NXAST_RAW_REG_LOAD,
    /* NX1.0-1.4(33): struct nx_action_reg_load2, ...
     *
     * [In OpenFlow 1.5, set_field is a superset of reg_load2 functionality, so
     * we drop reg_load2.] */
    NXAST_RAW_REG_LOAD2,

    /* OF1.5+(28): struct ofp15_action_copy_field, ... */
    OFPAT_RAW15_COPY_FIELD,
    /* ONF1.3-1.4(3200): struct onf_action_copy_field, ... */
    ONFACT_RAW13_COPY_FIELD,
    /* NX1.0-1.4(6): struct nx_action_reg_move, ... */
    NXAST_RAW_REG_MOVE,

/* ## ------------------------- ## */
/* ## Nicira extension actions. ## */
/* ## ------------------------- ## */

/* Actions similar to standard actions are listed with the standard actions. */

    /* NX1.0+(1): uint16_t. */
    NXAST_RAW_RESUBMIT,
    /* NX1.0+(14): struct nx_action_resubmit. */
    NXAST_RAW_RESUBMIT_TABLE,

    /* NX1.0+(2): uint32_t. */
    NXAST_RAW_SET_TUNNEL,
    /* NX1.0+(9): uint64_t. */
    NXAST_RAW_SET_TUNNEL64,

    /* NX1.0+(5): void. */
    NXAST_RAW_POP_QUEUE,

    /* NX1.0+(8): struct nx_action_note, ... */
    NXAST_RAW_NOTE,

    /* NX1.0+(10): struct nx_action_multipath. */
    NXAST_RAW_MULTIPATH,

    /* NX1.0+(12): struct nx_action_bundle, ... */
    NXAST_RAW_BUNDLE,
    /* NX1.0+(13): struct nx_action_bundle, ... */
    NXAST_RAW_BUNDLE_LOAD,

    /* NX1.0+(15): struct nx_action_output_reg. */
    NXAST_RAW_OUTPUT_REG,
    /* NX1.0+(32): struct nx_action_output_reg2. */
    NXAST_RAW_OUTPUT_REG2,

    /* NX1.0+(16): struct nx_action_learn, ... */
    NXAST_RAW_LEARN,

    /* NX1.0+(17): void. */
    NXAST_RAW_EXIT,

    /* NX1.0+(19): struct nx_action_fin_timeout. */
    NXAST_RAW_FIN_TIMEOUT,

    /* NX1.0+(20): struct nx_action_controller. */
    NXAST_RAW_CONTROLLER,

    /* NX1.0+(22): struct nx_action_write_metadata. */
    NXAST_RAW_WRITE_METADATA,

    /* NX1.0+(27): struct nx_action_stack. */
    NXAST_RAW_STACK_PUSH,

    /* NX1.0+(28): struct nx_action_stack. */
    NXAST_RAW_STACK_POP,

    /* NX1.0+(29): struct nx_action_sample. */
    NXAST_RAW_SAMPLE,

    /* NX1.0+(34): struct nx_action_conjunction. */
    NXAST_RAW_CONJUNCTION,
};

/* OpenFlow actions are always a multiple of 8 bytes in length. */
#define OFP_ACTION_ALIGN 8

/* Define a few functions for working with instructions. */
#define DEFINE_INST(ENUM, STRUCT, EXTENSIBLE, NAME)             \
    static inline const struct STRUCT * OVS_UNUSED              \
    instruction_get_##ENUM(const struct ofp11_instruction *inst)\
    {                                                           \
        ovs_assert(inst->type == htons(ENUM));                  \
        return ALIGNED_CAST(struct STRUCT *, inst);             \
    }                                                           \
                                                                \
    static inline void OVS_UNUSED                               \
    instruction_init_##ENUM(struct STRUCT *s)                   \
    {                                                           \
        memset(s, 0, sizeof *s);                                \
        s->type = htons(ENUM);                                  \
        s->len = htons(sizeof *s);                              \
    }                                                           \
                                                                \
    static inline struct STRUCT * OVS_UNUSED                    \
    instruction_put_##ENUM(struct ofpbuf *buf)                  \
    {                                                           \
        struct STRUCT *s = ofpbuf_put_uninit(buf, sizeof *s);   \
        instruction_init_##ENUM(s);                             \
        return s;                                               \
    }
OVS_INSTRUCTIONS
#undef DEFINE_INST

static void ofpacts_update_instruction_actions(struct ofpbuf *openflow,
                                               size_t ofs);
static void pad_ofpat(struct ofpbuf *openflow, size_t start_ofs);

static enum ofperr ofpacts_verify(const struct ofpact[], size_t ofpacts_len,
                                  uint32_t allowed_ovsinsts);

static void ofpact_put_set_field(struct ofpbuf *openflow, enum ofp_version,
                                 enum mf_field_id, uint64_t value);

static enum ofperr ofpact_pull_raw(struct ofpbuf *, enum ofp_version,
                                   enum ofp_raw_action_type *, uint64_t *arg);
static void *ofpact_put_raw(struct ofpbuf *, enum ofp_version,
                            enum ofp_raw_action_type, uint64_t arg);

static char *OVS_WARN_UNUSED_RESULT ofpacts_parse(
    char *str, struct ofpbuf *ofpacts, enum ofputil_protocol *usable_protocols,
    bool allow_instructions);

#include "ofp-actions.inc1"

/* Output actions. */

/* Action structure for OFPAT10_OUTPUT, which sends packets out 'port'.
 * When the 'port' is the OFPP_CONTROLLER, 'max_len' indicates the max
 * number of bytes to send.  A 'max_len' of zero means no bytes of the
 * packet should be sent. */
struct ofp10_action_output {
    ovs_be16 type;                  /* OFPAT10_OUTPUT. */
    ovs_be16 len;                   /* Length is 8. */
    ovs_be16 port;                  /* Output port. */
    ovs_be16 max_len;               /* Max length to send to controller. */
};
OFP_ASSERT(sizeof(struct ofp10_action_output) == 8);

/* Action structure for OFPAT_OUTPUT, which sends packets out 'port'.
   * When the 'port' is the OFPP_CONTROLLER, 'max_len' indicates the max
   * number of bytes to send. A 'max_len' of zero means no bytes of the
   * packet should be sent.*/
struct ofp11_action_output {
    ovs_be16 type;                    /* OFPAT11_OUTPUT. */
    ovs_be16 len;                     /* Length is 16. */
    ovs_be32 port;                    /* Output port. */
    ovs_be16 max_len;                 /* Max length to send to controller. */
    uint8_t pad[6];                   /* Pad to 64 bits. */
};
OFP_ASSERT(sizeof(struct ofp11_action_output) == 16);

static enum ofperr
decode_OFPAT_RAW10_OUTPUT(const struct ofp10_action_output *oao,
                          struct ofpbuf *out)
{
    struct ofpact_output *output;

    output = ofpact_put_OUTPUT(out);
    output->port = u16_to_ofp(ntohs(oao->port));
    output->max_len = ntohs(oao->max_len);

    return ofpact_check_output_port(output->port, OFPP_MAX);
}

static enum ofperr
decode_OFPAT_RAW11_OUTPUT(const struct ofp11_action_output *oao,
                       struct ofpbuf *out)
{
    struct ofpact_output *output;
    enum ofperr error;

    output = ofpact_put_OUTPUT(out);
    output->max_len = ntohs(oao->max_len);

    error = ofputil_port_from_ofp11(oao->port, &output->port);
    if (error) {
        return error;
    }

    return ofpact_check_output_port(output->port, OFPP_MAX);
}

static void
encode_OUTPUT(const struct ofpact_output *output,
              enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version == OFP10_VERSION) {
        struct ofp10_action_output *oao;

        oao = put_OFPAT10_OUTPUT(out);
        oao->port = htons(ofp_to_u16(output->port));
        oao->max_len = htons(output->max_len);
    } else {
        struct ofp11_action_output *oao;

        oao = put_OFPAT11_OUTPUT(out);
        oao->port = ofputil_port_to_ofp11(output->port);
        oao->max_len = htons(output->max_len);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_OUTPUT(const char *arg, struct ofpbuf *ofpacts,
             enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    if (strchr(arg, '[')) {
        struct ofpact_output_reg *output_reg;

        output_reg = ofpact_put_OUTPUT_REG(ofpacts);
        output_reg->max_len = UINT16_MAX;
        return mf_parse_subfield(&output_reg->src, arg);
    } else {
        struct ofpact_output *output;

        output = ofpact_put_OUTPUT(ofpacts);
        if (!ofputil_port_from_string(arg, &output->port)) {
            return xasprintf("%s: output to unknown port", arg);
        }
        output->max_len = output->port == OFPP_CONTROLLER ? UINT16_MAX : 0;
        return NULL;
    }
}

static void
format_OUTPUT(const struct ofpact_output *a, struct ds *s)
{
    if (ofp_to_u16(a->port) < ofp_to_u16(OFPP_MAX)) {
        ds_put_format(s, "output:%"PRIu16, a->port);
    } else {
        ofputil_format_port(a->port, s);
        if (a->port == OFPP_CONTROLLER) {
            ds_put_format(s, ":%"PRIu16, a->max_len);
        }
    }
}

/* Group actions. */

static enum ofperr
decode_OFPAT_RAW11_GROUP(uint32_t group_id, struct ofpbuf *out)
{
    ofpact_put_GROUP(out)->group_id = group_id;
    return 0;
}

static void
encode_GROUP(const struct ofpact_group *group,
             enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version == OFP10_VERSION) {
        /* XXX */
    } else {
        put_OFPAT11_GROUP(out, group->group_id);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_GROUP(char *arg, struct ofpbuf *ofpacts,
                    enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_u32(arg, &ofpact_put_GROUP(ofpacts)->group_id);
}

static void
format_GROUP(const struct ofpact_group *a, struct ds *s)
{
    ds_put_format(s, "group:%"PRIu32, a->group_id);
}

/* Action structure for NXAST_CONTROLLER.
 *
 * This generalizes using OFPAT_OUTPUT to send a packet to OFPP_CONTROLLER.  In
 * addition to the 'max_len' that OFPAT_OUTPUT supports, it also allows
 * specifying:
 *
 *    - 'reason': The reason code to use in the ofp_packet_in or nx_packet_in.
 *
 *    - 'controller_id': The ID of the controller connection to which the
 *      ofp_packet_in should be sent.  The ofp_packet_in or nx_packet_in is
 *      sent only to controllers that have the specified controller connection
 *      ID.  See "struct nx_controller_id" for more information. */
struct nx_action_controller {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* Length is 16. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_CONTROLLER. */
    ovs_be16 max_len;               /* Maximum length to send to controller. */
    ovs_be16 controller_id;         /* Controller ID to send packet-in. */
    uint8_t reason;                 /* enum ofp_packet_in_reason (OFPR_*). */
    uint8_t zero;                   /* Must be zero. */
};
OFP_ASSERT(sizeof(struct nx_action_controller) == 16);

static enum ofperr
decode_NXAST_RAW_CONTROLLER(const struct nx_action_controller *nac,
                            struct ofpbuf *out)
{
    struct ofpact_controller *oc;

    oc = ofpact_put_CONTROLLER(out);
    oc->max_len = ntohs(nac->max_len);
    oc->controller_id = ntohs(nac->controller_id);
    oc->reason = nac->reason;
    return 0;
}

static void
encode_CONTROLLER(const struct ofpact_controller *controller,
                  enum ofp_version ofp_version OVS_UNUSED,
                  struct ofpbuf *out)
{
    struct nx_action_controller *nac;

    nac = put_NXAST_CONTROLLER(out);
    nac->max_len = htons(controller->max_len);
    nac->controller_id = htons(controller->controller_id);
    nac->reason = controller->reason;
}

static char * OVS_WARN_UNUSED_RESULT
parse_CONTROLLER(char *arg, struct ofpbuf *ofpacts,
                  enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    enum ofp_packet_in_reason reason = OFPR_ACTION;
    uint16_t controller_id = 0;
    uint16_t max_len = UINT16_MAX;

    if (!arg[0]) {
        /* Use defaults. */
    } else if (strspn(arg, "0123456789") == strlen(arg)) {
        char *error = str_to_u16(arg, "max_len", &max_len);
        if (error) {
            return error;
        }
    } else {
        char *name, *value;

        while (ofputil_parse_key_value(&arg, &name, &value)) {
            if (!strcmp(name, "reason")) {
                if (!ofputil_packet_in_reason_from_string(value, &reason)) {
                    return xasprintf("unknown reason \"%s\"", value);
                }
            } else if (!strcmp(name, "max_len")) {
                char *error = str_to_u16(value, "max_len", &max_len);
                if (error) {
                    return error;
                }
            } else if (!strcmp(name, "id")) {
                char *error = str_to_u16(value, "id", &controller_id);
                if (error) {
                    return error;
                }
            } else {
                return xasprintf("unknown key \"%s\" parsing controller "
                                 "action", name);
            }
        }
    }

    if (reason == OFPR_ACTION && controller_id == 0) {
        struct ofpact_output *output;

        output = ofpact_put_OUTPUT(ofpacts);
        output->port = OFPP_CONTROLLER;
        output->max_len = max_len;
    } else {
        struct ofpact_controller *controller;

        controller = ofpact_put_CONTROLLER(ofpacts);
        controller->max_len = max_len;
        controller->reason = reason;
        controller->controller_id = controller_id;
    }

    return NULL;
}

static void
format_CONTROLLER(const struct ofpact_controller *a, struct ds *s)
{
    if (a->reason == OFPR_ACTION && a->controller_id == 0) {
        ds_put_format(s, "CONTROLLER:%"PRIu16, a->max_len);
    } else {
        enum ofp_packet_in_reason reason = a->reason;

        ds_put_cstr(s, "controller(");
        if (reason != OFPR_ACTION) {
            char reasonbuf[OFPUTIL_PACKET_IN_REASON_BUFSIZE];

            ds_put_format(s, "reason=%s,",
                          ofputil_packet_in_reason_to_string(
                              reason, reasonbuf, sizeof reasonbuf));
        }
        if (a->max_len != UINT16_MAX) {
            ds_put_format(s, "max_len=%"PRIu16",", a->max_len);
        }
        if (a->controller_id != 0) {
            ds_put_format(s, "id=%"PRIu16",", a->controller_id);
        }
        ds_chomp(s, ',');
        ds_put_char(s, ')');
    }
}

/* Enqueue action. */
struct ofp10_action_enqueue {
    ovs_be16 type;            /* OFPAT10_ENQUEUE. */
    ovs_be16 len;             /* Len is 16. */
    ovs_be16 port;            /* Port that queue belongs. Should
                                 refer to a valid physical port
                                 (i.e. < OFPP_MAX) or OFPP_IN_PORT. */
    uint8_t pad[6];           /* Pad for 64-bit alignment. */
    ovs_be32 queue_id;        /* Where to enqueue the packets. */
};
OFP_ASSERT(sizeof(struct ofp10_action_enqueue) == 16);

static enum ofperr
decode_OFPAT_RAW10_ENQUEUE(const struct ofp10_action_enqueue *oae,
                           struct ofpbuf *out)
{
    struct ofpact_enqueue *enqueue;

    enqueue = ofpact_put_ENQUEUE(out);
    enqueue->port = u16_to_ofp(ntohs(oae->port));
    enqueue->queue = ntohl(oae->queue_id);
    if (ofp_to_u16(enqueue->port) >= ofp_to_u16(OFPP_MAX)
        && enqueue->port != OFPP_IN_PORT
        && enqueue->port != OFPP_LOCAL) {
        return OFPERR_OFPBAC_BAD_OUT_PORT;
    }
    return 0;
}

static void
encode_ENQUEUE(const struct ofpact_enqueue *enqueue,
               enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version == OFP10_VERSION) {
        struct ofp10_action_enqueue *oae;

        oae = put_OFPAT10_ENQUEUE(out);
        oae->port = htons(ofp_to_u16(enqueue->port));
        oae->queue_id = htonl(enqueue->queue);
    } else {
        /* XXX */
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_ENQUEUE(char *arg, struct ofpbuf *ofpacts,
              enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    char *sp = NULL;
    char *port = strtok_r(arg, ":q,", &sp);
    char *queue = strtok_r(NULL, "", &sp);
    struct ofpact_enqueue *enqueue;

    if (port == NULL || queue == NULL) {
        return xstrdup("\"enqueue\" syntax is \"enqueue:PORT:QUEUE\" or "
                       "\"enqueue(PORT,QUEUE)\"");
    }

    enqueue = ofpact_put_ENQUEUE(ofpacts);
    if (!ofputil_port_from_string(port, &enqueue->port)) {
        return xasprintf("%s: enqueue to unknown port", port);
    }
    return str_to_u32(queue, &enqueue->queue);
}

static void
format_ENQUEUE(const struct ofpact_enqueue *a, struct ds *s)
{
    ds_put_format(s, "enqueue:");
    ofputil_format_port(a->port, s);
    ds_put_format(s, ":%"PRIu32, a->queue);
}

/* Action structure for NXAST_OUTPUT_REG.
 *
 * Outputs to the OpenFlow port number written to src[ofs:ofs+nbits].
 *
 * The format and semantics of 'src' and 'ofs_nbits' are similar to those for
 * the NXAST_REG_LOAD action.
 *
 * The acceptable nxm_header values for 'src' are the same as the acceptable
 * nxm_header values for the 'src' field of NXAST_REG_MOVE.
 *
 * The 'max_len' field indicates the number of bytes to send when the chosen
 * port is OFPP_CONTROLLER.  Its semantics are equivalent to the 'max_len'
 * field of OFPAT_OUTPUT.
 *
 * The 'zero' field is required to be zeroed for forward compatibility. */
struct nx_action_output_reg {
    ovs_be16 type;              /* OFPAT_VENDOR. */
    ovs_be16 len;               /* 24. */
    ovs_be32 vendor;            /* NX_VENDOR_ID. */
    ovs_be16 subtype;           /* NXAST_OUTPUT_REG. */

    ovs_be16 ofs_nbits;         /* (ofs << 6) | (n_bits - 1). */
    ovs_be32 src;               /* Source. */

    ovs_be16 max_len;           /* Max length to send to controller. */

    uint8_t zero[6];            /* Reserved, must be zero. */
};
OFP_ASSERT(sizeof(struct nx_action_output_reg) == 24);

/* Action structure for NXAST_OUTPUT_REG2.
 *
 * Like the NXAST_OUTPUT_REG but organized so that there is room for a 64-bit
 * experimenter OXM as 'src'.
 */
struct nx_action_output_reg2 {
    ovs_be16 type;              /* OFPAT_VENDOR. */
    ovs_be16 len;               /* 24. */
    ovs_be32 vendor;            /* NX_VENDOR_ID. */
    ovs_be16 subtype;           /* NXAST_OUTPUT_REG2. */

    ovs_be16 ofs_nbits;         /* (ofs << 6) | (n_bits - 1). */
    ovs_be16 max_len;           /* Max length to send to controller. */

    /* Followed by:
     * - 'src', as an OXM/NXM header (either 4 or 8 bytes).
     * - Enough 0-bytes to pad the action out to 24 bytes. */
    uint8_t pad[10];
};
OFP_ASSERT(sizeof(struct nx_action_output_reg2) == 24);

static enum ofperr
decode_NXAST_RAW_OUTPUT_REG(const struct nx_action_output_reg *naor,
                            struct ofpbuf *out)
{
    struct ofpact_output_reg *output_reg;

    if (!is_all_zeros(naor->zero, sizeof naor->zero)) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    output_reg = ofpact_put_OUTPUT_REG(out);
    output_reg->ofpact.raw = NXAST_RAW_OUTPUT_REG;
    output_reg->src.field = mf_from_nxm_header(ntohl(naor->src));
    output_reg->src.ofs = nxm_decode_ofs(naor->ofs_nbits);
    output_reg->src.n_bits = nxm_decode_n_bits(naor->ofs_nbits);
    output_reg->max_len = ntohs(naor->max_len);

    return mf_check_src(&output_reg->src, NULL);
}

static enum ofperr
decode_NXAST_RAW_OUTPUT_REG2(const struct nx_action_output_reg2 *naor,
                            struct ofpbuf *out)
{
    struct ofpact_output_reg *output_reg;
    enum ofperr error;
    struct ofpbuf b;

    output_reg = ofpact_put_OUTPUT_REG(out);
    output_reg->ofpact.raw = NXAST_RAW_OUTPUT_REG2;
    output_reg->src.ofs = nxm_decode_ofs(naor->ofs_nbits);
    output_reg->src.n_bits = nxm_decode_n_bits(naor->ofs_nbits);
    output_reg->max_len = ntohs(naor->max_len);

    ofpbuf_use_const(&b, naor, ntohs(naor->len));
    ofpbuf_pull(&b, OBJECT_OFFSETOF(naor, pad));
    error = nx_pull_header(&b, &output_reg->src.field, NULL);
    if (error) {
        return error;
    }
    if (!is_all_zeros(b.data, b.size)) {
        return OFPERR_NXBRC_MUST_BE_ZERO;
    }

    return mf_check_src(&output_reg->src, NULL);
}

static void
encode_OUTPUT_REG(const struct ofpact_output_reg *output_reg,
                  enum ofp_version ofp_version OVS_UNUSED,
                  struct ofpbuf *out)
{
    /* If 'output_reg' came in as an NXAST_RAW_OUTPUT_REG2 action, or if it
     * cannot be encoded in the older form, encode it as
     * NXAST_RAW_OUTPUT_REG2. */
    if (output_reg->ofpact.raw == NXAST_RAW_OUTPUT_REG2
        || !mf_nxm_header(output_reg->src.field->id)) {
        struct nx_action_output_reg2 *naor = put_NXAST_OUTPUT_REG2(out);
        size_t size = out->size;

        naor->ofs_nbits = nxm_encode_ofs_nbits(output_reg->src.ofs,
                                               output_reg->src.n_bits);
        naor->max_len = htons(output_reg->max_len);

        out->size = size - sizeof naor->pad;
        nx_put_header(out, output_reg->src.field->id, 0, false);
        out->size = size;
    } else {
        struct nx_action_output_reg *naor = put_NXAST_OUTPUT_REG(out);

        naor->ofs_nbits = nxm_encode_ofs_nbits(output_reg->src.ofs,
                                               output_reg->src.n_bits);
        naor->src = htonl(mf_nxm_header(output_reg->src.field->id));
        naor->max_len = htons(output_reg->max_len);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_OUTPUT_REG(const char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return parse_OUTPUT(arg, ofpacts, usable_protocols);
}

static void
format_OUTPUT_REG(const struct ofpact_output_reg *a, struct ds *s)
{
    ds_put_cstr(s, "output:");
    mf_format_subfield(&a->src, s);
}

/* Action structure for NXAST_BUNDLE and NXAST_BUNDLE_LOAD.
 *
 * The bundle actions choose a slave from a supplied list of options.
 * NXAST_BUNDLE outputs to its selection.  NXAST_BUNDLE_LOAD writes its
 * selection to a register.
 *
 * The list of possible slaves follows the nx_action_bundle structure. The size
 * of each slave is governed by its type as indicated by the 'slave_type'
 * parameter. The list of slaves should be padded at its end with zeros to make
 * the total length of the action a multiple of 8.
 *
 * Switches infer from the 'slave_type' parameter the size of each slave.  All
 * implementations must support the NXM_OF_IN_PORT 'slave_type' which indicates
 * that the slaves are OpenFlow port numbers with NXM_LENGTH(NXM_OF_IN_PORT) ==
 * 2 byte width.  Switches should reject actions which indicate unknown or
 * unsupported slave types.
 *
 * Switches use a strategy dictated by the 'algorithm' parameter to choose a
 * slave.  If the switch does not support the specified 'algorithm' parameter,
 * it should reject the action.
 *
 * Several algorithms take into account liveness when selecting slaves.  The
 * liveness of a slave is implementation defined (with one exception), but will
 * generally take into account things like its carrier status and the results
 * of any link monitoring protocols which happen to be running on it.  In order
 * to give controllers a place-holder value, the OFPP_NONE port is always
 * considered live.
 *
 * Some slave selection strategies require the use of a hash function, in which
 * case the 'fields' and 'basis' parameters should be populated.  The 'fields'
 * parameter (one of NX_HASH_FIELDS_*) designates which parts of the flow to
 * hash.  Refer to the definition of "enum nx_hash_fields" for details.  The
 * 'basis' parameter is used as a universal hash parameter.  Different values
 * of 'basis' yield different hash results.
 *
 * The 'zero' parameter at the end of the action structure is reserved for
 * future use.  Switches are required to reject actions which have nonzero
 * bytes in the 'zero' field.
 *
 * NXAST_BUNDLE actions should have 'ofs_nbits' and 'dst' zeroed.  Switches
 * should reject actions which have nonzero bytes in either of these fields.
 *
 * NXAST_BUNDLE_LOAD stores the OpenFlow port number of the selected slave in
 * dst[ofs:ofs+n_bits].  The format and semantics of 'dst' and 'ofs_nbits' are
 * similar to those for the NXAST_REG_LOAD action. */
struct nx_action_bundle {
    ovs_be16 type;              /* OFPAT_VENDOR. */
    ovs_be16 len;               /* Length including slaves. */
    ovs_be32 vendor;            /* NX_VENDOR_ID. */
    ovs_be16 subtype;           /* NXAST_BUNDLE or NXAST_BUNDLE_LOAD. */

    /* Slave choice algorithm to apply to hash value. */
    ovs_be16 algorithm;         /* One of NX_BD_ALG_*. */

    /* What fields to hash and how. */
    ovs_be16 fields;            /* One of NX_HASH_FIELDS_*. */
    ovs_be16 basis;             /* Universal hash parameter. */

    ovs_be32 slave_type;        /* NXM_OF_IN_PORT. */
    ovs_be16 n_slaves;          /* Number of slaves. */

    ovs_be16 ofs_nbits;         /* (ofs << 6) | (n_bits - 1). */
    ovs_be32 dst;               /* Destination. */

    uint8_t zero[4];            /* Reserved. Must be zero. */
};
OFP_ASSERT(sizeof(struct nx_action_bundle) == 32);

static enum ofperr
decode_bundle(bool load, const struct nx_action_bundle *nab,
              struct ofpbuf *ofpacts)
{
    static struct vlog_rate_limit rl = VLOG_RATE_LIMIT_INIT(1, 5);
    struct ofpact_bundle *bundle;
    uint32_t slave_type;
    size_t slaves_size, i;
    enum ofperr error;

    bundle = ofpact_put_BUNDLE(ofpacts);

    bundle->n_slaves = ntohs(nab->n_slaves);
    bundle->basis = ntohs(nab->basis);
    bundle->fields = ntohs(nab->fields);
    bundle->algorithm = ntohs(nab->algorithm);
    slave_type = ntohl(nab->slave_type);
    slaves_size = ntohs(nab->len) - sizeof *nab;

    error = OFPERR_OFPBAC_BAD_ARGUMENT;
    if (!flow_hash_fields_valid(bundle->fields)) {
        VLOG_WARN_RL(&rl, "unsupported fields %d", (int) bundle->fields);
    } else if (bundle->n_slaves > BUNDLE_MAX_SLAVES) {
        VLOG_WARN_RL(&rl, "too many slaves");
    } else if (bundle->algorithm != NX_BD_ALG_HRW
               && bundle->algorithm != NX_BD_ALG_ACTIVE_BACKUP) {
        VLOG_WARN_RL(&rl, "unsupported algorithm %d", (int) bundle->algorithm);
    } else if (slave_type != mf_nxm_header(MFF_IN_PORT)) {
        VLOG_WARN_RL(&rl, "unsupported slave type %"PRIu16, slave_type);
    } else {
        error = 0;
    }

    if (!is_all_zeros(nab->zero, sizeof nab->zero)) {
        VLOG_WARN_RL(&rl, "reserved field is nonzero");
        error = OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    if (load) {
        bundle->dst.field = mf_from_nxm_header(ntohl(nab->dst));
        bundle->dst.ofs = nxm_decode_ofs(nab->ofs_nbits);
        bundle->dst.n_bits = nxm_decode_n_bits(nab->ofs_nbits);

        if (bundle->dst.n_bits < 16) {
            VLOG_WARN_RL(&rl, "bundle_load action requires at least 16 bit "
                         "destination.");
            error = OFPERR_OFPBAC_BAD_ARGUMENT;
        }
    } else {
        if (nab->ofs_nbits || nab->dst) {
            VLOG_WARN_RL(&rl, "bundle action has nonzero reserved fields");
            error = OFPERR_OFPBAC_BAD_ARGUMENT;
        }
    }

    if (slaves_size < bundle->n_slaves * sizeof(ovs_be16)) {
        VLOG_WARN_RL(&rl, "Nicira action %s only has %"PRIuSIZE" bytes "
                     "allocated for slaves.  %"PRIuSIZE" bytes are required "
                     "for %"PRIu16" slaves.",
                     load ? "bundle_load" : "bundle", slaves_size,
                     bundle->n_slaves * sizeof(ovs_be16), bundle->n_slaves);
        error = OFPERR_OFPBAC_BAD_LEN;
    }

    for (i = 0; i < bundle->n_slaves; i++) {
        uint16_t ofp_port = ntohs(((ovs_be16 *)(nab + 1))[i]);
        ofpbuf_put(ofpacts, &ofp_port, sizeof ofp_port);
    }

    bundle = ofpacts->header;
    ofpact_update_len(ofpacts, &bundle->ofpact);

    if (!error) {
        error = bundle_check(bundle, OFPP_MAX, NULL);
    }
    return error;
}

static enum ofperr
decode_NXAST_RAW_BUNDLE(const struct nx_action_bundle *nab, struct ofpbuf *out)
{
    return decode_bundle(false, nab, out);
}

static enum ofperr
decode_NXAST_RAW_BUNDLE_LOAD(const struct nx_action_bundle *nab,
                             struct ofpbuf *out)
{
    return decode_bundle(true, nab, out);
}

static void
encode_BUNDLE(const struct ofpact_bundle *bundle,
              enum ofp_version ofp_version OVS_UNUSED,
              struct ofpbuf *out)
{
    int slaves_len = ROUND_UP(2 * bundle->n_slaves, OFP_ACTION_ALIGN);
    struct nx_action_bundle *nab;
    ovs_be16 *slaves;
    size_t i;

    nab = (bundle->dst.field
           ? put_NXAST_BUNDLE_LOAD(out)
           : put_NXAST_BUNDLE(out));
    nab->len = htons(ntohs(nab->len) + slaves_len);
    nab->algorithm = htons(bundle->algorithm);
    nab->fields = htons(bundle->fields);
    nab->basis = htons(bundle->basis);
    nab->slave_type = htonl(mf_nxm_header(MFF_IN_PORT));
    nab->n_slaves = htons(bundle->n_slaves);
    if (bundle->dst.field) {
        nab->ofs_nbits = nxm_encode_ofs_nbits(bundle->dst.ofs,
                                              bundle->dst.n_bits);
        nab->dst = htonl(mf_nxm_header(bundle->dst.field->id));
    }

    slaves = ofpbuf_put_zeros(out, slaves_len);
    for (i = 0; i < bundle->n_slaves; i++) {
        slaves[i] = htons(ofp_to_u16(bundle->slaves[i]));
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_BUNDLE(const char *arg, struct ofpbuf *ofpacts,
             enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return bundle_parse(arg, ofpacts);
}

static char * OVS_WARN_UNUSED_RESULT
parse_bundle_load(const char *arg, struct ofpbuf *ofpacts)
{
    return bundle_parse_load(arg, ofpacts);
}

static void
format_BUNDLE(const struct ofpact_bundle *a, struct ds *s)
{
    bundle_format(a, s);
}

/* Set VLAN actions. */

static enum ofperr
decode_set_vlan_vid(uint16_t vid, bool push_vlan_if_needed, struct ofpbuf *out)
{
    if (vid & ~0xfff) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    } else {
        struct ofpact_vlan_vid *vlan_vid = ofpact_put_SET_VLAN_VID(out);
        vlan_vid->vlan_vid = vid;
        vlan_vid->push_vlan_if_needed = push_vlan_if_needed;
        return 0;
    }
}

static enum ofperr
decode_OFPAT_RAW10_SET_VLAN_VID(uint16_t vid, struct ofpbuf *out)
{
    return decode_set_vlan_vid(vid, true, out);
}

static enum ofperr
decode_OFPAT_RAW11_SET_VLAN_VID(uint16_t vid, struct ofpbuf *out)
{
    return decode_set_vlan_vid(vid, false, out);
}

static void
encode_SET_VLAN_VID(const struct ofpact_vlan_vid *vlan_vid,
                    enum ofp_version ofp_version, struct ofpbuf *out)
{
    uint16_t vid = vlan_vid->vlan_vid;

    /* Push a VLAN tag, if none is present and this form of the action calls
     * for such a feature. */
    if (ofp_version > OFP10_VERSION
        && vlan_vid->push_vlan_if_needed
        && !vlan_vid->flow_has_vlan) {
        put_OFPAT11_PUSH_VLAN(out, htons(ETH_TYPE_VLAN_8021Q));
    }

    if (ofp_version == OFP10_VERSION) {
        put_OFPAT10_SET_VLAN_VID(out, vid);
    } else if (ofp_version == OFP11_VERSION) {
        put_OFPAT11_SET_VLAN_VID(out, vid);
    } else {
        ofpact_put_set_field(out, ofp_version,
                             MFF_VLAN_VID, vid | OFPVID12_PRESENT);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_set_vlan_vid(char *arg, struct ofpbuf *ofpacts, bool push_vlan_if_needed)
{
    struct ofpact_vlan_vid *vlan_vid;
    uint16_t vid;
    char *error;

    error = str_to_u16(arg, "VLAN VID", &vid);
    if (error) {
        return error;
    }

    if (vid & ~VLAN_VID_MASK) {
        return xasprintf("%s: not a valid VLAN VID", arg);
    }
    vlan_vid = ofpact_put_SET_VLAN_VID(ofpacts);
    vlan_vid->vlan_vid = vid;
    vlan_vid->push_vlan_if_needed = push_vlan_if_needed;
    return NULL;
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_VLAN_VID(char *arg, struct ofpbuf *ofpacts,
                   enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return parse_set_vlan_vid(arg, ofpacts, false);
}

static void
format_SET_VLAN_VID(const struct ofpact_vlan_vid *a, struct ds *s)
{
    ds_put_format(s, "%s:%"PRIu16,
                  a->push_vlan_if_needed ? "mod_vlan_vid" : "set_vlan_vid",
                  a->vlan_vid);
}

/* Set PCP actions. */

static enum ofperr
decode_set_vlan_pcp(uint8_t pcp, bool push_vlan_if_needed, struct ofpbuf *out)
{
    if (pcp & ~7) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    } else {
        struct ofpact_vlan_pcp *vlan_pcp = ofpact_put_SET_VLAN_PCP(out);
        vlan_pcp->vlan_pcp = pcp;
        vlan_pcp->push_vlan_if_needed = push_vlan_if_needed;
        return 0;
    }
}

static enum ofperr
decode_OFPAT_RAW10_SET_VLAN_PCP(uint8_t pcp, struct ofpbuf *out)
{
    return decode_set_vlan_pcp(pcp, true, out);
}

static enum ofperr
decode_OFPAT_RAW11_SET_VLAN_PCP(uint8_t pcp, struct ofpbuf *out)
{
    return decode_set_vlan_pcp(pcp, false, out);
}

static void
encode_SET_VLAN_PCP(const struct ofpact_vlan_pcp *vlan_pcp,
                    enum ofp_version ofp_version, struct ofpbuf *out)
{
    uint8_t pcp = vlan_pcp->vlan_pcp;

    /* Push a VLAN tag, if none is present and this form of the action calls
     * for such a feature. */
    if (ofp_version > OFP10_VERSION
        && vlan_pcp->push_vlan_if_needed
        && !vlan_pcp->flow_has_vlan) {
        put_OFPAT11_PUSH_VLAN(out, htons(ETH_TYPE_VLAN_8021Q));
    }

    if (ofp_version == OFP10_VERSION) {
        put_OFPAT10_SET_VLAN_PCP(out, pcp);
    } else if (ofp_version == OFP11_VERSION) {
        put_OFPAT11_SET_VLAN_PCP(out, pcp);
    } else {
        ofpact_put_set_field(out, ofp_version, MFF_VLAN_PCP, pcp);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_set_vlan_pcp(char *arg, struct ofpbuf *ofpacts, bool push_vlan_if_needed)
{
    struct ofpact_vlan_pcp *vlan_pcp;
    uint8_t pcp;
    char *error;

    error = str_to_u8(arg, "VLAN PCP", &pcp);
    if (error) {
        return error;
    }

    if (pcp & ~7) {
        return xasprintf("%s: not a valid VLAN PCP", arg);
    }
    vlan_pcp = ofpact_put_SET_VLAN_PCP(ofpacts);
    vlan_pcp->vlan_pcp = pcp;
    vlan_pcp->push_vlan_if_needed = push_vlan_if_needed;
    return NULL;
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_VLAN_PCP(char *arg, struct ofpbuf *ofpacts,
                   enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return parse_set_vlan_pcp(arg, ofpacts, false);
}

static void
format_SET_VLAN_PCP(const struct ofpact_vlan_pcp *a, struct ds *s)
{
    ds_put_format(s, "%s:%"PRIu8,
                  a->push_vlan_if_needed ? "mod_vlan_pcp" : "set_vlan_pcp",
                  a->vlan_pcp);
}

/* Strip VLAN actions. */

static enum ofperr
decode_OFPAT_RAW10_STRIP_VLAN(struct ofpbuf *out)
{
    ofpact_put_STRIP_VLAN(out)->ofpact.raw = OFPAT_RAW10_STRIP_VLAN;
    return 0;
}

static enum ofperr
decode_OFPAT_RAW11_POP_VLAN(struct ofpbuf *out)
{
    ofpact_put_STRIP_VLAN(out)->ofpact.raw = OFPAT_RAW11_POP_VLAN;
    return 0;
}

static void
encode_STRIP_VLAN(const struct ofpact_null *null OVS_UNUSED,
                  enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version == OFP10_VERSION) {
        put_OFPAT10_STRIP_VLAN(out);
    } else {
        put_OFPAT11_POP_VLAN(out);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_STRIP_VLAN(char *arg OVS_UNUSED, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    ofpact_put_STRIP_VLAN(ofpacts)->ofpact.raw = OFPAT_RAW10_STRIP_VLAN;
    return NULL;
}

static char * OVS_WARN_UNUSED_RESULT
parse_pop_vlan(struct ofpbuf *ofpacts)
{
    ofpact_put_STRIP_VLAN(ofpacts)->ofpact.raw = OFPAT_RAW11_POP_VLAN;
    return NULL;
}

static void
format_STRIP_VLAN(const struct ofpact_null *a, struct ds *s)
{
    ds_put_cstr(s, (a->ofpact.raw == OFPAT_RAW11_POP_VLAN
                    ? "pop_vlan"
                    : "strip_vlan"));
}

/* Push VLAN action. */

static enum ofperr
decode_OFPAT_RAW11_PUSH_VLAN(ovs_be16 eth_type, struct ofpbuf *out)
{
    if (eth_type != htons(ETH_TYPE_VLAN_8021Q)) {
        /* XXX 802.1AD(QinQ) isn't supported at the moment */
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }
    ofpact_put_PUSH_VLAN(out);
    return 0;
}

static void
encode_PUSH_VLAN(const struct ofpact_null *null OVS_UNUSED,
                 enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version == OFP10_VERSION) {
        /* PUSH is a side effect of a SET_VLAN_VID/PCP, which should
         * follow this action. */
    } else {
        /* XXX ETH_TYPE_VLAN_8021AD case */
        put_OFPAT11_PUSH_VLAN(out, htons(ETH_TYPE_VLAN_8021Q));
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_PUSH_VLAN(char *arg, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    uint16_t ethertype;
    char *error;

    *usable_protocols &= OFPUTIL_P_OF11_UP;
    error = str_to_u16(arg, "ethertype", &ethertype);
    if (error) {
        return error;
    }

    if (ethertype != ETH_TYPE_VLAN_8021Q) {
        /* XXX ETH_TYPE_VLAN_8021AD case isn't supported */
        return xasprintf("%s: not a valid VLAN ethertype", arg);
    }

    ofpact_put_PUSH_VLAN(ofpacts);
    return NULL;
}

static void
format_PUSH_VLAN(const struct ofpact_null *a OVS_UNUSED, struct ds *s)
{
    /* XXX 802.1AD case*/
    ds_put_format(s, "push_vlan:%#"PRIx16, ETH_TYPE_VLAN_8021Q);
}

/* Action structure for OFPAT10_SET_DL_SRC/DST and OFPAT11_SET_DL_SRC/DST. */
struct ofp_action_dl_addr {
    ovs_be16 type;                  /* Type. */
    ovs_be16 len;                   /* Length is 16. */
    uint8_t dl_addr[OFP_ETH_ALEN];  /* Ethernet address. */
    uint8_t pad[6];
};
OFP_ASSERT(sizeof(struct ofp_action_dl_addr) == 16);

static enum ofperr
decode_OFPAT_RAW_SET_DL_SRC(const struct ofp_action_dl_addr *a,
                            struct ofpbuf *out)
{
    memcpy(ofpact_put_SET_ETH_SRC(out)->mac, a->dl_addr, ETH_ADDR_LEN);
    return 0;
}

static enum ofperr
decode_OFPAT_RAW_SET_DL_DST(const struct ofp_action_dl_addr *a,
                            struct ofpbuf *out)
{
    memcpy(ofpact_put_SET_ETH_DST(out)->mac, a->dl_addr, ETH_ADDR_LEN);
    return 0;
}

static void
encode_SET_ETH_addr(const struct ofpact_mac *mac, enum ofp_version ofp_version,
                    enum ofp_raw_action_type raw, enum mf_field_id field,
                    struct ofpbuf *out)
{
    const uint8_t *addr = mac->mac;

    if (ofp_version < OFP12_VERSION) {
        struct ofp_action_dl_addr *oada;

        oada = ofpact_put_raw(out, ofp_version, raw, 0);
        memcpy(oada->dl_addr, addr, ETH_ADDR_LEN);
    } else {
        ofpact_put_set_field(out, ofp_version, field,
                             eth_addr_to_uint64(addr));
    }
}

static void
encode_SET_ETH_SRC(const struct ofpact_mac *mac, enum ofp_version ofp_version,
                   struct ofpbuf *out)
{
    encode_SET_ETH_addr(mac, ofp_version, OFPAT_RAW_SET_DL_SRC, MFF_ETH_SRC,
                        out);

}

static void
encode_SET_ETH_DST(const struct ofpact_mac *mac,
                               enum ofp_version ofp_version,
                               struct ofpbuf *out)
{
    encode_SET_ETH_addr(mac, ofp_version, OFPAT_RAW_SET_DL_DST, MFF_ETH_DST,
                        out);

}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_ETH_SRC(char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_mac(arg, ofpact_put_SET_ETH_SRC(ofpacts)->mac);
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_ETH_DST(char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_mac(arg, ofpact_put_SET_ETH_DST(ofpacts)->mac);
}

static void
format_SET_ETH_SRC(const struct ofpact_mac *a, struct ds *s)
{
    ds_put_format(s, "mod_dl_src:"ETH_ADDR_FMT, ETH_ADDR_ARGS(a->mac));
}

static void
format_SET_ETH_DST(const struct ofpact_mac *a, struct ds *s)
{
    ds_put_format(s, "mod_dl_dst:"ETH_ADDR_FMT, ETH_ADDR_ARGS(a->mac));
}

/* Set IPv4 address actions. */

static enum ofperr
decode_OFPAT_RAW_SET_NW_SRC(ovs_be32 ipv4, struct ofpbuf *out)
{
    ofpact_put_SET_IPV4_SRC(out)->ipv4 = ipv4;
    return 0;
}

static enum ofperr
decode_OFPAT_RAW_SET_NW_DST(ovs_be32 ipv4, struct ofpbuf *out)
{
    ofpact_put_SET_IPV4_DST(out)->ipv4 = ipv4;
    return 0;
}

static void
encode_SET_IPV4_addr(const struct ofpact_ipv4 *ipv4,
                     enum ofp_version ofp_version,
                     enum ofp_raw_action_type raw, enum mf_field_id field,
                     struct ofpbuf *out)
{
    ovs_be32 addr = ipv4->ipv4;
    if (ofp_version < OFP12_VERSION) {
        ofpact_put_raw(out, ofp_version, raw, ntohl(addr));
    } else {
        ofpact_put_set_field(out, ofp_version, field, ntohl(addr));
    }
}

static void
encode_SET_IPV4_SRC(const struct ofpact_ipv4 *ipv4,
                    enum ofp_version ofp_version, struct ofpbuf *out)
{
    encode_SET_IPV4_addr(ipv4, ofp_version, OFPAT_RAW_SET_NW_SRC, MFF_IPV4_SRC,
                         out);
}

static void
encode_SET_IPV4_DST(const struct ofpact_ipv4 *ipv4,
                    enum ofp_version ofp_version, struct ofpbuf *out)
{
    encode_SET_IPV4_addr(ipv4, ofp_version, OFPAT_RAW_SET_NW_DST, MFF_IPV4_DST,
                         out);
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_IPV4_SRC(char *arg, struct ofpbuf *ofpacts,
                   enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_ip(arg, &ofpact_put_SET_IPV4_SRC(ofpacts)->ipv4);
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_IPV4_DST(char *arg, struct ofpbuf *ofpacts,
                   enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_ip(arg, &ofpact_put_SET_IPV4_DST(ofpacts)->ipv4);
}

static void
format_SET_IPV4_SRC(const struct ofpact_ipv4 *a, struct ds *s)
{
    ds_put_format(s, "mod_nw_src:"IP_FMT, IP_ARGS(a->ipv4));
}

static void
format_SET_IPV4_DST(const struct ofpact_ipv4 *a, struct ds *s)
{
    ds_put_format(s, "mod_nw_dst:"IP_FMT, IP_ARGS(a->ipv4));
}

/* Set IPv4/v6 TOS actions. */

static enum ofperr
decode_OFPAT_RAW_SET_NW_TOS(uint8_t dscp, struct ofpbuf *out)
{
    if (dscp & ~IP_DSCP_MASK) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    } else {
        ofpact_put_SET_IP_DSCP(out)->dscp = dscp;
        return 0;
    }
}

static void
encode_SET_IP_DSCP(const struct ofpact_dscp *dscp,
                   enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version < OFP12_VERSION) {
        put_OFPAT_SET_NW_TOS(out, ofp_version, dscp->dscp);
    } else {
        ofpact_put_set_field(out, ofp_version,
                             MFF_IP_DSCP_SHIFTED, dscp->dscp >> 2);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_IP_DSCP(char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    uint8_t tos;
    char *error;

    error = str_to_u8(arg, "TOS", &tos);
    if (error) {
        return error;
    }

    if (tos & ~IP_DSCP_MASK) {
        return xasprintf("%s: not a valid TOS", arg);
    }
    ofpact_put_SET_IP_DSCP(ofpacts)->dscp = tos;
    return NULL;
}

static void
format_SET_IP_DSCP(const struct ofpact_dscp *a, struct ds *s)
{
    ds_put_format(s, "mod_nw_tos:%d", a->dscp);
}

/* Set IPv4/v6 ECN actions. */

static enum ofperr
decode_OFPAT_RAW11_SET_NW_ECN(uint8_t ecn, struct ofpbuf *out)
{
    if (ecn & ~IP_ECN_MASK) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    } else {
        ofpact_put_SET_IP_ECN(out)->ecn = ecn;
        return 0;
    }
}

static void
encode_SET_IP_ECN(const struct ofpact_ecn *ip_ecn,
                  enum ofp_version ofp_version, struct ofpbuf *out)
{
    uint8_t ecn = ip_ecn->ecn;
    if (ofp_version == OFP10_VERSION) {
        /* XXX */
    } else if (ofp_version == OFP11_VERSION) {
        put_OFPAT11_SET_NW_ECN(out, ecn);
    } else {
        ofpact_put_set_field(out, ofp_version, MFF_IP_ECN, ecn);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_IP_ECN(char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    uint8_t ecn;
    char *error;

    error = str_to_u8(arg, "ECN", &ecn);
    if (error) {
        return error;
    }

    if (ecn & ~IP_ECN_MASK) {
        return xasprintf("%s: not a valid ECN", arg);
    }
    ofpact_put_SET_IP_ECN(ofpacts)->ecn = ecn;
    return NULL;
}

static void
format_SET_IP_ECN(const struct ofpact_ecn *a, struct ds *s)
{
    ds_put_format(s, "mod_nw_ecn:%d", a->ecn);
}

/* Set IPv4/v6 TTL actions. */

static enum ofperr
decode_OFPAT_RAW11_SET_NW_TTL(uint8_t ttl, struct ofpbuf *out)
{
    ofpact_put_SET_IP_TTL(out)->ttl = ttl;
    return 0;
}

static void
encode_SET_IP_TTL(const struct ofpact_ip_ttl *ttl,
                  enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version >= OFP11_VERSION) {
        put_OFPAT11_SET_NW_TTL(out, ttl->ttl);
    } else {
        /* XXX */
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_IP_TTL(char *arg, struct ofpbuf *ofpacts,
                  enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    uint8_t ttl;
    char *error;

    error = str_to_u8(arg, "TTL", &ttl);
    if (error) {
        return error;
    }

    ofpact_put_SET_IP_TTL(ofpacts)->ttl = ttl;
    return NULL;
}

static void
format_SET_IP_TTL(const struct ofpact_ip_ttl *a, struct ds *s)
{
    ds_put_format(s, "mod_nw_ttl:%d", a->ttl);
}

/* Set TCP/UDP/SCTP port actions. */

static enum ofperr
decode_OFPAT_RAW_SET_TP_SRC(ovs_be16 port, struct ofpbuf *out)
{
    ofpact_put_SET_L4_SRC_PORT(out)->port = ntohs(port);
    return 0;
}

static enum ofperr
decode_OFPAT_RAW_SET_TP_DST(ovs_be16 port, struct ofpbuf *out)
{
    ofpact_put_SET_L4_DST_PORT(out)->port = ntohs(port);
    return 0;
}

static void
encode_SET_L4_port(const struct ofpact_l4_port *l4_port,
                   enum ofp_version ofp_version, enum ofp_raw_action_type raw,
                   enum mf_field_id field, struct ofpbuf *out)
{
    uint16_t port = l4_port->port;

    if (ofp_version >= OFP12_VERSION && field != MFF_N_IDS) {
        ofpact_put_set_field(out, ofp_version, field, port);
    } else {
        ofpact_put_raw(out, ofp_version, raw, port);
    }
}

static void
encode_SET_L4_SRC_PORT(const struct ofpact_l4_port *l4_port,
                       enum ofp_version ofp_version, struct ofpbuf *out)
{
    uint8_t proto = l4_port->flow_ip_proto;
    enum mf_field_id field = (proto == IPPROTO_TCP ? MFF_TCP_SRC
                              : proto == IPPROTO_UDP ? MFF_UDP_SRC
                              : proto == IPPROTO_SCTP ? MFF_SCTP_SRC
                              : MFF_N_IDS);

    encode_SET_L4_port(l4_port, ofp_version, OFPAT_RAW_SET_TP_SRC, field, out);
}

static void
encode_SET_L4_DST_PORT(const struct ofpact_l4_port *l4_port,
                       enum ofp_version ofp_version,
                       struct ofpbuf *out)
{
    uint8_t proto = l4_port->flow_ip_proto;
    enum mf_field_id field = (proto == IPPROTO_TCP ? MFF_TCP_DST
                              : proto == IPPROTO_UDP ? MFF_UDP_DST
                              : proto == IPPROTO_SCTP ? MFF_SCTP_DST
                              : MFF_N_IDS);

    encode_SET_L4_port(l4_port, ofp_version, OFPAT_RAW_SET_TP_DST, field, out);
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_L4_SRC_PORT(char *arg, struct ofpbuf *ofpacts,
                      enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_u16(arg, "source port",
                      &ofpact_put_SET_L4_SRC_PORT(ofpacts)->port);
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_L4_DST_PORT(char *arg, struct ofpbuf *ofpacts,
                      enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_u16(arg, "destination port",
                      &ofpact_put_SET_L4_DST_PORT(ofpacts)->port);
}

static void
format_SET_L4_SRC_PORT(const struct ofpact_l4_port *a, struct ds *s)
{
    ds_put_format(s, "mod_tp_src:%d", a->port);
}

static void
format_SET_L4_DST_PORT(const struct ofpact_l4_port *a, struct ds *s)
{
    ds_put_format(s, "mod_tp_dst:%d", a->port);
}

/* Action structure for OFPAT_COPY_FIELD. */
struct ofp15_action_copy_field {
    ovs_be16 type;              /* OFPAT_COPY_FIELD. */
    ovs_be16 len;               /* Length is padded to 64 bits. */
    ovs_be16 n_bits;            /* Number of bits to copy. */
    ovs_be16 src_offset;        /* Starting bit offset in source. */
    ovs_be16 dst_offset;        /* Starting bit offset in destination. */
    uint8_t pad[2];
    /* Followed by:
     * - OXM header for source field.
     * - OXM header for destination field.
     * - Padding with 0-bytes to a multiple of 8 bytes.
     * The "pad2" member is the beginning of the above. */
    uint8_t pad2[4];
};
OFP_ASSERT(sizeof(struct ofp15_action_copy_field) == 16);

/* Action structure for OpenFlow 1.3 extension copy-field action.. */
struct onf_action_copy_field {
    ovs_be16 type;              /* OFPAT_EXPERIMENTER. */
    ovs_be16 len;               /* Length is padded to 64 bits. */
    ovs_be32 experimenter;      /* ONF_VENDOR_ID. */
    ovs_be16 exp_type;          /* 3200. */
    uint8_t pad[2];             /* Not used. */
    ovs_be16 n_bits;            /* Number of bits to copy. */
    ovs_be16 src_offset;        /* Starting bit offset in source. */
    ovs_be16 dst_offset;        /* Starting bit offset in destination. */
    uint8_t pad2[2];            /* Not used. */
    /* Followed by:
     * - OXM header for source field.
     * - OXM header for destination field.
     * - Padding with 0-bytes (either 0 or 4 of them) to a multiple of 8 bytes.
     * The "pad3" member is the beginning of the above. */
    uint8_t pad3[4];            /* Not used. */
};
OFP_ASSERT(sizeof(struct onf_action_copy_field) == 24);

/* Action structure for NXAST_REG_MOVE.
 *
 * Copies src[src_ofs:src_ofs+n_bits] to dst[dst_ofs:dst_ofs+n_bits], where
 * a[b:c] denotes the bits within 'a' numbered 'b' through 'c' (not including
 * bit 'c').  Bit numbering starts at 0 for the least-significant bit, 1 for
 * the next most significant bit, and so on.
 *
 * 'src' and 'dst' are nxm_header values with nxm_hasmask=0.  (It doesn't make
 * sense to use nxm_hasmask=1 because the action does not do any kind of
 * matching; it uses the actual value of a field.)
 *
 * The following nxm_header values are potentially acceptable as 'src':
 *
 *   - NXM_OF_IN_PORT
 *   - NXM_OF_ETH_DST
 *   - NXM_OF_ETH_SRC
 *   - NXM_OF_ETH_TYPE
 *   - NXM_OF_VLAN_TCI
 *   - NXM_OF_IP_TOS
 *   - NXM_OF_IP_PROTO
 *   - NXM_OF_IP_SRC
 *   - NXM_OF_IP_DST
 *   - NXM_OF_TCP_SRC
 *   - NXM_OF_TCP_DST
 *   - NXM_OF_UDP_SRC
 *   - NXM_OF_UDP_DST
 *   - NXM_OF_ICMP_TYPE
 *   - NXM_OF_ICMP_CODE
 *   - NXM_OF_ARP_OP
 *   - NXM_OF_ARP_SPA
 *   - NXM_OF_ARP_TPA
 *   - NXM_NX_TUN_ID
 *   - NXM_NX_ARP_SHA
 *   - NXM_NX_ARP_THA
 *   - NXM_NX_ICMPV6_TYPE
 *   - NXM_NX_ICMPV6_CODE
 *   - NXM_NX_ND_SLL
 *   - NXM_NX_ND_TLL
 *   - NXM_NX_REG(idx) for idx in the switch's accepted range.
 *   - NXM_NX_PKT_MARK
 *   - NXM_NX_TUN_IPV4_SRC
 *   - NXM_NX_TUN_IPV4_DST
 *
 * The following nxm_header values are potentially acceptable as 'dst':
 *
 *   - NXM_OF_ETH_DST
 *   - NXM_OF_ETH_SRC
 *   - NXM_OF_IP_TOS
 *   - NXM_OF_IP_SRC
 *   - NXM_OF_IP_DST
 *   - NXM_OF_TCP_SRC
 *   - NXM_OF_TCP_DST
 *   - NXM_OF_UDP_SRC
 *   - NXM_OF_UDP_DST
 *   - NXM_NX_ARP_SHA
 *   - NXM_NX_ARP_THA
 *   - NXM_OF_ARP_OP
 *   - NXM_OF_ARP_SPA
 *   - NXM_OF_ARP_TPA
 *     Modifying any of the above fields changes the corresponding packet
 *     header.
 *
 *   - NXM_OF_IN_PORT
 *
 *   - NXM_NX_REG(idx) for idx in the switch's accepted range.
 *
 *   - NXM_NX_PKT_MARK
 *
 *   - NXM_OF_VLAN_TCI.  Modifying this field's value has side effects on the
 *     packet's 802.1Q header.  Setting a value with CFI=0 removes the 802.1Q
 *     header (if any), ignoring the other bits.  Setting a value with CFI=1
 *     adds or modifies the 802.1Q header appropriately, setting the TCI field
 *     to the field's new value (with the CFI bit masked out).
 *
 *   - NXM_NX_TUN_ID, NXM_NX_TUN_IPV4_SRC, NXM_NX_TUN_IPV4_DST.  Modifying
 *     any of these values modifies the corresponding tunnel header field used
 *     for the packet's next tunnel encapsulation, if allowed by the
 *     configuration of the output tunnel port.
 *
 * A given nxm_header value may be used as 'src' or 'dst' only on a flow whose
 * nx_match satisfies its prerequisites.  For example, NXM_OF_IP_TOS may be
 * used only if the flow's nx_match includes an nxm_entry that specifies
 * nxm_type=NXM_OF_ETH_TYPE, nxm_hasmask=0, and nxm_value=0x0800.
 *
 * The switch will reject actions for which src_ofs+n_bits is greater than the
 * width of 'src' or dst_ofs+n_bits is greater than the width of 'dst' with
 * error type OFPET_BAD_ACTION, code OFPBAC_BAD_ARGUMENT.
 *
 * This action behaves properly when 'src' overlaps with 'dst', that is, it
 * behaves as if 'src' were copied out to a temporary buffer, then the
 * temporary buffer copied to 'dst'.
 */
struct nx_action_reg_move {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* Length is 24. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_REG_MOVE. */
    ovs_be16 n_bits;                /* Number of bits. */
    ovs_be16 src_ofs;               /* Starting bit offset in source. */
    ovs_be16 dst_ofs;               /* Starting bit offset in destination. */
    /* Followed by:
     * - OXM/NXM header for source field (4 or 8 bytes).
     * - OXM/NXM header for destination field (4 or 8 bytes).
     * - Padding with 0-bytes to a multiple of 8 bytes, if necessary. */
};
OFP_ASSERT(sizeof(struct nx_action_reg_move) == 16);

static enum ofperr
decode_copy_field__(ovs_be16 src_offset, ovs_be16 dst_offset, ovs_be16 n_bits,
                    const void *action, ovs_be16 action_len, size_t oxm_offset,
                    struct ofpbuf *ofpacts)
{
    struct ofpact_reg_move *move;
    enum ofperr error;
    struct ofpbuf b;

    move = ofpact_put_REG_MOVE(ofpacts);
    move->ofpact.raw = ONFACT_RAW13_COPY_FIELD;
    move->src.ofs = ntohs(src_offset);
    move->src.n_bits = ntohs(n_bits);
    move->dst.ofs = ntohs(dst_offset);
    move->dst.n_bits = ntohs(n_bits);

    ofpbuf_use_const(&b, action, ntohs(action_len));
    ofpbuf_pull(&b, oxm_offset);
    error = nx_pull_header(&b, &move->src.field, NULL);
    if (error) {
        return error;
    }
    error = nx_pull_header(&b, &move->dst.field, NULL);
    if (error) {
        return error;
    }

    if (!is_all_zeros(b.data, b.size)) {
        return OFPERR_NXBRC_MUST_BE_ZERO;
    }

    return nxm_reg_move_check(move, NULL);
}

static enum ofperr
decode_OFPAT_RAW15_COPY_FIELD(const struct ofp15_action_copy_field *oacf,
                              struct ofpbuf *ofpacts)
{
    return decode_copy_field__(oacf->src_offset, oacf->dst_offset,
                               oacf->n_bits, oacf, oacf->len,
                               OBJECT_OFFSETOF(oacf, pad2), ofpacts);
}

static enum ofperr
decode_ONFACT_RAW13_COPY_FIELD(const struct onf_action_copy_field *oacf,
                               struct ofpbuf *ofpacts)
{
    return decode_copy_field__(oacf->src_offset, oacf->dst_offset,
                               oacf->n_bits, oacf, oacf->len,
                               OBJECT_OFFSETOF(oacf, pad3), ofpacts);
}

static enum ofperr
decode_NXAST_RAW_REG_MOVE(const struct nx_action_reg_move *narm,
                          struct ofpbuf *ofpacts)
{
    struct ofpact_reg_move *move;
    enum ofperr error;
    struct ofpbuf b;

    move = ofpact_put_REG_MOVE(ofpacts);
    move->ofpact.raw = NXAST_RAW_REG_MOVE;
    move->src.ofs = ntohs(narm->src_ofs);
    move->src.n_bits = ntohs(narm->n_bits);
    move->dst.ofs = ntohs(narm->dst_ofs);
    move->dst.n_bits = ntohs(narm->n_bits);

    ofpbuf_use_const(&b, narm, ntohs(narm->len));
    ofpbuf_pull(&b, sizeof *narm);
    error = nx_pull_header(&b, &move->src.field, NULL);
    if (error) {
        return error;
    }
    error = nx_pull_header(&b, &move->dst.field, NULL);
    if (error) {
        return error;
    }
    if (!is_all_zeros(b.data, b.size)) {
        return OFPERR_NXBRC_MUST_BE_ZERO;
    }

    return nxm_reg_move_check(move, NULL);
}

static void
encode_REG_MOVE(const struct ofpact_reg_move *move,
                enum ofp_version ofp_version, struct ofpbuf *out)
{
    /* For OpenFlow 1.3, the choice of ONFACT_RAW13_COPY_FIELD versus
     * NXAST_RAW_REG_MOVE is somewhat difficult.  Neither one is guaranteed to
     * be supported by every OpenFlow 1.3 implementation.  It would be ideal to
     * probe for support.  Until we have that ability, we currently prefer
     * NXAST_RAW_REG_MOVE for backward compatibility with older Open vSwitch
     * versions.  */
    size_t start_ofs = out->size;
    if (ofp_version >= OFP15_VERSION) {
        struct ofp15_action_copy_field *copy = put_OFPAT15_COPY_FIELD(out);
        copy->n_bits = htons(move->dst.n_bits);
        copy->src_offset = htons(move->src.ofs);
        copy->dst_offset = htons(move->dst.ofs);
        out->size = out->size - sizeof copy->pad2;
        nx_put_header(out, move->src.field->id, ofp_version, false);
        nx_put_header(out, move->dst.field->id, ofp_version, false);
    } else if (ofp_version == OFP13_VERSION
               && move->ofpact.raw == ONFACT_RAW13_COPY_FIELD) {
        struct onf_action_copy_field *copy = put_ONFACT13_COPY_FIELD(out);
        copy->n_bits = htons(move->dst.n_bits);
        copy->src_offset = htons(move->src.ofs);
        copy->dst_offset = htons(move->dst.ofs);
        out->size = out->size - sizeof copy->pad3;
        nx_put_header(out, move->src.field->id, ofp_version, false);
        nx_put_header(out, move->dst.field->id, ofp_version, false);
    } else {
        struct nx_action_reg_move *narm = put_NXAST_REG_MOVE(out);
        narm->n_bits = htons(move->dst.n_bits);
        narm->src_ofs = htons(move->src.ofs);
        narm->dst_ofs = htons(move->dst.ofs);
        nx_put_header(out, move->src.field->id, 0, false);
        nx_put_header(out, move->dst.field->id, 0, false);
    }
    pad_ofpat(out, start_ofs);
}

static char * OVS_WARN_UNUSED_RESULT
parse_REG_MOVE(const char *arg, struct ofpbuf *ofpacts,
               enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_reg_move *move = ofpact_put_REG_MOVE(ofpacts);
    const char *full_arg = arg;
    char *error;

    error = mf_parse_subfield__(&move->src, &arg);
    if (error) {
        return error;
    }
    if (strncmp(arg, "->", 2)) {
        return xasprintf("%s: missing `->' following source", full_arg);
    }
    arg += 2;
    error = mf_parse_subfield(&move->dst, arg);
    if (error) {
        return error;
    }

    if (move->src.n_bits != move->dst.n_bits) {
        return xasprintf("%s: source field is %d bits wide but destination is "
                         "%d bits wide", full_arg,
                         move->src.n_bits, move->dst.n_bits);
    }
    return NULL;
}

static void
format_REG_MOVE(const struct ofpact_reg_move *a, struct ds *s)
{
    nxm_format_reg_move(a, s);
}

/* Action structure for OFPAT12_SET_FIELD. */
struct ofp12_action_set_field {
    ovs_be16 type;                  /* OFPAT12_SET_FIELD. */
    ovs_be16 len;                   /* Length is padded to 64 bits. */

    /* Followed by:
     * - An OXM header, value, and (in OpenFlow 1.5+) optionally a mask.
     * - Enough 0-bytes to pad out to a multiple of 64 bits.
     *
     * The "pad" member is the beginning of the above. */
    uint8_t pad[4];
};
OFP_ASSERT(sizeof(struct ofp12_action_set_field) == 8);

/* Action structure for NXAST_REG_LOAD.
 *
 * Copies value[0:n_bits] to dst[ofs:ofs+n_bits], where a[b:c] denotes the bits
 * within 'a' numbered 'b' through 'c' (not including bit 'c').  Bit numbering
 * starts at 0 for the least-significant bit, 1 for the next most significant
 * bit, and so on.
 *
 * 'dst' is an nxm_header with nxm_hasmask=0.  See the documentation for
 * NXAST_REG_MOVE, above, for the permitted fields and for the side effects of
 * loading them.
 *
 * The 'ofs' and 'n_bits' fields are combined into a single 'ofs_nbits' field
 * to avoid enlarging the structure by another 8 bytes.  To allow 'n_bits' to
 * take a value between 1 and 64 (inclusive) while taking up only 6 bits, it is
 * also stored as one less than its true value:
 *
 *  15                           6 5                0
 * +------------------------------+------------------+
 * |              ofs             |    n_bits - 1    |
 * +------------------------------+------------------+
 *
 * The switch will reject actions for which ofs+n_bits is greater than the
 * width of 'dst', or in which any bits in 'value' with value 2**n_bits or
 * greater are set to 1, with error type OFPET_BAD_ACTION, code
 * OFPBAC_BAD_ARGUMENT.
 */
struct nx_action_reg_load {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* Length is 24. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_REG_LOAD. */
    ovs_be16 ofs_nbits;             /* (ofs << 6) | (n_bits - 1). */
    ovs_be32 dst;                   /* Destination register. */
    ovs_be64 value;                 /* Immediate value. */
};
OFP_ASSERT(sizeof(struct nx_action_reg_load) == 24);

/* Action structure for NXAST_REG_LOAD2.
 *
 * Compared to OFPAT_SET_FIELD, we can use this to set whole or partial fields
 * in any OpenFlow version.  Compared to NXAST_REG_LOAD, we can use this to set
 * OXM experimenter fields. */
struct nx_action_reg_load2 {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* At least 16. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_SET_FIELD. */

    /* Followed by:
     * - An NXM/OXM header, value, and optionally a mask.
     * - Enough 0-bytes to pad out to a multiple of 64 bits.
     *
     * The "pad" member is the beginning of the above. */
    uint8_t pad[6];
};
OFP_ASSERT(sizeof(struct nx_action_reg_load2) == 16);

static enum ofperr
decode_ofpat_set_field(const struct ofp12_action_set_field *oasf,
                       bool may_mask, struct ofpbuf *ofpacts)
{
    struct ofpact_set_field *sf;
    enum ofperr error;
    struct ofpbuf b;

    sf = ofpact_put_SET_FIELD(ofpacts);

    ofpbuf_use_const(&b, oasf, ntohs(oasf->len));
    ofpbuf_pull(&b, OBJECT_OFFSETOF(oasf, pad));
    error = nx_pull_entry(&b, &sf->field, &sf->value,
                          may_mask ? &sf->mask : NULL);
    if (error) {
        return (error == OFPERR_OFPBMC_BAD_MASK
                ? OFPERR_OFPBAC_BAD_SET_MASK
                : error);
    }
    if (!may_mask) {
        memset(&sf->mask, 0xff, sf->field->n_bytes);
    }

    if (!is_all_zeros(b.data, b.size)) {
        return OFPERR_OFPBAC_BAD_SET_ARGUMENT;
    }

    /* OpenFlow says specifically that one may not set OXM_OF_IN_PORT via
     * Set-Field. */
    if (sf->field->id == MFF_IN_PORT_OXM) {
        return OFPERR_OFPBAC_BAD_SET_ARGUMENT;
    }

    /* oxm_length is now validated to be compatible with mf_value. */
    if (!sf->field->writable) {
        VLOG_WARN_RL(&rl, "destination field %s is not writable",
                     sf->field->name);
        return OFPERR_OFPBAC_BAD_SET_ARGUMENT;
    }

    /* The value must be valid for match.  OpenFlow 1.5 also says,
     * "In an OXM_OF_VLAN_VID set-field action, the OFPVID_PRESENT bit must be
     * a 1-bit in oxm_value and in oxm_mask." */
    if (!mf_is_value_valid(sf->field, &sf->value)
        || (sf->field->id == MFF_VLAN_VID
            && (!(sf->mask.be16 & htons(OFPVID12_PRESENT))
                || !(sf->value.be16 & htons(OFPVID12_PRESENT))))) {
        struct ds ds = DS_EMPTY_INITIALIZER;
        mf_format(sf->field, &sf->value, NULL, &ds);
        VLOG_WARN_RL(&rl, "Invalid value for set field %s: %s",
                     sf->field->name, ds_cstr(&ds));
        ds_destroy(&ds);

        return OFPERR_OFPBAC_BAD_SET_ARGUMENT;
    }
    return 0;
}

static enum ofperr
decode_OFPAT_RAW12_SET_FIELD(const struct ofp12_action_set_field *oasf,
                             struct ofpbuf *ofpacts)
{
    return decode_ofpat_set_field(oasf, false, ofpacts);
}

static enum ofperr
decode_OFPAT_RAW15_SET_FIELD(const struct ofp12_action_set_field *oasf,
                             struct ofpbuf *ofpacts)
{
    return decode_ofpat_set_field(oasf, true, ofpacts);
}

static enum ofperr
decode_NXAST_RAW_REG_LOAD(const struct nx_action_reg_load *narl,
                          struct ofpbuf *out)
{
    struct ofpact_set_field *sf = ofpact_put_reg_load(out);
    struct mf_subfield dst;
    enum ofperr error;

    sf->ofpact.raw = NXAST_RAW_REG_LOAD;

    dst.field = mf_from_nxm_header(ntohl(narl->dst));
    dst.ofs = nxm_decode_ofs(narl->ofs_nbits);
    dst.n_bits = nxm_decode_n_bits(narl->ofs_nbits);
    error = mf_check_dst(&dst, NULL);
    if (error) {
        return error;
    }

    /* Reject 'narl' if a bit numbered 'n_bits' or higher is set to 1 in
     * narl->value. */
    if (dst.n_bits < 64 && ntohll(narl->value) >> dst.n_bits) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    sf->field = dst.field;
    bitwise_put(ntohll(narl->value),
                &sf->value, dst.field->n_bytes, dst.ofs,
                dst.n_bits);
    bitwise_put(UINT64_MAX,
                &sf->mask, dst.field->n_bytes, dst.ofs,
                dst.n_bits);

    return 0;
}

static enum ofperr
decode_NXAST_RAW_REG_LOAD2(const struct nx_action_reg_load2 *narl,
                           struct ofpbuf *out)
{
    struct ofpact_set_field *sf;
    enum ofperr error;
    struct ofpbuf b;

    sf = ofpact_put_SET_FIELD(out);
    sf->ofpact.raw = NXAST_RAW_REG_LOAD2;

    ofpbuf_use_const(&b, narl, ntohs(narl->len));
    ofpbuf_pull(&b, OBJECT_OFFSETOF(narl, pad));
    error = nx_pull_entry(&b, &sf->field, &sf->value, &sf->mask);
    if (error) {
        return error;
    }
    if (!is_all_zeros(b.data, b.size)) {
        return OFPERR_OFPBAC_BAD_SET_ARGUMENT;
    }

    if (!sf->field->writable) {
        VLOG_WARN_RL(&rl, "destination field %s is not writable",
                     sf->field->name);
        return OFPERR_OFPBAC_BAD_SET_ARGUMENT;
    }
    return 0;
}

static void
ofpact_put_set_field(struct ofpbuf *openflow, enum ofp_version ofp_version,
                     enum mf_field_id field, uint64_t value_)
{
    struct ofp12_action_set_field *oasf OVS_UNUSED;
    int n_bytes = mf_from_id(field)->n_bytes;
    size_t start_ofs = openflow->size;
    union mf_value value;

    value.be64 = htonll(value_ << (8 * (8 - n_bytes)));

    oasf = put_OFPAT12_SET_FIELD(openflow);
    openflow->size = openflow->size - sizeof oasf->pad;
    nx_put_entry(openflow, field, ofp_version, &value, NULL);
    pad_ofpat(openflow, start_ofs);
}

static bool
next_load_segment(const struct ofpact_set_field *sf,
                  struct mf_subfield *dst, uint64_t *value)
{
    int n_bits = sf->field->n_bits;
    int n_bytes = sf->field->n_bytes;
    int start = dst->ofs + dst->n_bits;

    if (start < n_bits) {
        dst->field = sf->field;
        dst->ofs = bitwise_scan(&sf->mask, n_bytes, 1, start, n_bits);
        if (dst->ofs < n_bits) {
            dst->n_bits = bitwise_scan(&sf->mask, n_bytes, 0, dst->ofs + 1,
                                       MIN(dst->ofs + 64, n_bits)) - dst->ofs;
            *value = bitwise_get(&sf->value, n_bytes, dst->ofs, dst->n_bits);
            return true;
        }
    }
    return false;
}

/* Convert 'sf' to a series of REG_LOADs. */
static void
set_field_to_nxast(const struct ofpact_set_field *sf, struct ofpbuf *openflow)
{
    /* If 'sf' cannot be encoded as NXAST_REG_LOAD because it requires an
     * experimenter OXM (or if it came in as NXAST_REG_LOAD2), encode as
     * NXAST_REG_LOAD2.  Otherwise use NXAST_REG_LOAD, which is backward
     * compatible. */
    if (sf->ofpact.raw == NXAST_RAW_REG_LOAD2
        || !mf_nxm_header(sf->field->id)) {
        struct nx_action_reg_load2 *narl OVS_UNUSED;
        size_t start_ofs = openflow->size;

        narl = put_NXAST_REG_LOAD2(openflow);
        openflow->size = openflow->size - sizeof narl->pad;
        nx_put_entry(openflow, sf->field->id, 0, &sf->value, &sf->mask);
        pad_ofpat(openflow, start_ofs);
    } else {
        struct mf_subfield dst;
        uint64_t value;

        dst.ofs = dst.n_bits = 0;
        while (next_load_segment(sf, &dst, &value)) {
            struct nx_action_reg_load *narl = put_NXAST_REG_LOAD(openflow);
            narl->ofs_nbits = nxm_encode_ofs_nbits(dst.ofs, dst.n_bits);
            narl->dst = htonl(mf_nxm_header(dst.field->id));
            narl->value = htonll(value);
        }
    }
}

/* Convert 'sf', which must set an entire field, to standard OpenFlow 1.0/1.1
 * actions, if we can, falling back to Nicira extensions if we must.
 *
 * We check only meta-flow types that can appear within set field actions and
 * that have a mapping to compatible action types.  These struct mf_field
 * definitions have a defined OXM or NXM header value and specify the field as
 * writable. */
static void
set_field_to_legacy_openflow(const struct ofpact_set_field *sf,
                             enum ofp_version ofp_version,
                             struct ofpbuf *out)
{
    switch ((int) sf->field->id) {
    case MFF_VLAN_TCI: {
        ovs_be16 tci = sf->value.be16;
        bool cfi = (tci & htons(VLAN_CFI)) != 0;
        uint16_t vid = vlan_tci_to_vid(tci);
        uint8_t pcp = vlan_tci_to_pcp(tci);

        if (ofp_version < OFP11_VERSION) {
            /* NXM_OF_VLAN_TCI to OpenFlow 1.0 mapping:
             *
             * If CFI=1, Add or modify VLAN VID & PCP.
             * If CFI=0, strip VLAN header, if any.
             */
            if (cfi) {
                put_OFPAT10_SET_VLAN_VID(out, vid);
                put_OFPAT10_SET_VLAN_PCP(out, pcp);
            } else {
                put_OFPAT10_STRIP_VLAN(out);
            }
        } else {
            /* NXM_OF_VLAN_TCI to OpenFlow 1.1 mapping:
             *
             * If CFI=1, Add or modify VLAN VID & PCP.
             *    OpenFlow 1.1 set actions only apply if the packet
             *    already has VLAN tags.  To be sure that is the case
             *    we have to push a VLAN header.  As we do not support
             *    multiple layers of VLANs, this is a no-op, if a VLAN
             *    header already exists.  This may backfire, however,
             *    when we start supporting multiple layers of VLANs.
             * If CFI=0, strip VLAN header, if any.
             */
            if (cfi) {
                /* Push a VLAN tag, if one was not seen at action validation
                 * time. */
                if (!sf->flow_has_vlan) {
                    put_OFPAT11_PUSH_VLAN(out, htons(ETH_TYPE_VLAN_8021Q));
                }
                put_OFPAT11_SET_VLAN_VID(out, vid);
                put_OFPAT11_SET_VLAN_PCP(out, pcp);
            } else {
                /* If the flow did not match on vlan, we have no way of
                 * knowing if the vlan tag exists, so we must POP just to be
                 * sure. */
                put_OFPAT11_POP_VLAN(out);
            }
        }
        break;
    }

    case MFF_VLAN_VID: {
        uint16_t vid = ntohs(sf->value.be16) & VLAN_VID_MASK;
        if (ofp_version == OFP10_VERSION) {
            put_OFPAT10_SET_VLAN_VID(out, vid);
        } else {
            put_OFPAT11_SET_VLAN_VID(out, vid);
        }
        break;
    }

    case MFF_VLAN_PCP:
        if (ofp_version == OFP10_VERSION) {
            put_OFPAT10_SET_VLAN_PCP(out, sf->value.u8);
        } else {
            put_OFPAT11_SET_VLAN_PCP(out, sf->value.u8);
        }
        break;

    case MFF_ETH_SRC:
        memcpy(put_OFPAT_SET_DL_SRC(out, ofp_version)->dl_addr,
               sf->value.mac, ETH_ADDR_LEN);
        break;

    case MFF_ETH_DST:
        memcpy(put_OFPAT_SET_DL_DST(out, ofp_version)->dl_addr,
               sf->value.mac, ETH_ADDR_LEN);
        break;

    case MFF_IPV4_SRC:
        put_OFPAT_SET_NW_SRC(out, ofp_version, sf->value.be32);
        break;

    case MFF_IPV4_DST:
        put_OFPAT_SET_NW_DST(out, ofp_version, sf->value.be32);
        break;

    case MFF_IP_DSCP:
        put_OFPAT_SET_NW_TOS(out, ofp_version, sf->value.u8);
        break;

    case MFF_IP_DSCP_SHIFTED:
        put_OFPAT_SET_NW_TOS(out, ofp_version, sf->value.u8 << 2);
        break;

    case MFF_TCP_SRC:
    case MFF_UDP_SRC:
        put_OFPAT_SET_TP_SRC(out, sf->value.be16);
        break;

    case MFF_TCP_DST:
    case MFF_UDP_DST:
        put_OFPAT_SET_TP_DST(out, sf->value.be16);
        break;

    default:
        set_field_to_nxast(sf, out);
        break;
    }
}

static void
set_field_to_set_field(const struct ofpact_set_field *sf,
                       enum ofp_version ofp_version, struct ofpbuf *out)
{
    struct ofp12_action_set_field *oasf OVS_UNUSED;
    size_t start_ofs = out->size;

    oasf = put_OFPAT12_SET_FIELD(out);
    out->size = out->size - sizeof oasf->pad;
    nx_put_entry(out, sf->field->id, ofp_version, &sf->value, &sf->mask);
    pad_ofpat(out, start_ofs);
}

static void
encode_SET_FIELD(const struct ofpact_set_field *sf,
                 enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version >= OFP15_VERSION) {
        /* OF1.5+ only has Set-Field (reg_load is redundant so we drop it
         * entirely). */
        set_field_to_set_field(sf, ofp_version, out);
    } else if (sf->ofpact.raw == NXAST_RAW_REG_LOAD ||
               sf->ofpact.raw == NXAST_RAW_REG_LOAD2) {
        /* It came in as reg_load, send it out the same way. */
        set_field_to_nxast(sf, out);
    } else if (ofp_version < OFP12_VERSION) {
        /* OpenFlow 1.0 and 1.1 don't have Set-Field. */
        set_field_to_legacy_openflow(sf, ofp_version, out);
    } else if (is_all_ones((const uint8_t *) &sf->mask, sf->field->n_bytes)) {
        /* We're encoding to OpenFlow 1.2, 1.3, or 1.4.  The action sets an
         * entire field, so encode it as OFPAT_SET_FIELD. */
        set_field_to_set_field(sf, ofp_version, out);
    } else {
        /* We're encoding to OpenFlow 1.2, 1.3, or 1.4.  The action cannot be
         * encoded as OFPAT_SET_FIELD because it does not set an entire field,
         * so encode it as reg_load. */
        set_field_to_nxast(sf, out);
    }
}

/* Parses a "set_field" action with argument 'arg', appending the parsed
 * action to 'ofpacts'.
 *
 * Returns NULL if successful, otherwise a malloc()'d string describing the
 * error.  The caller is responsible for freeing the returned string. */
static char * OVS_WARN_UNUSED_RESULT
set_field_parse__(char *arg, struct ofpbuf *ofpacts,
                  enum ofputil_protocol *usable_protocols)
{
    struct ofpact_set_field *sf = ofpact_put_SET_FIELD(ofpacts);
    char *value;
    char *delim;
    char *key;
    const struct mf_field *mf;
    char *error;

    value = arg;
    delim = strstr(arg, "->");
    if (!delim) {
        return xasprintf("%s: missing `->'", arg);
    }
    if (strlen(delim) <= strlen("->")) {
        return xasprintf("%s: missing field name following `->'", arg);
    }

    key = delim + strlen("->");
    mf = mf_from_name(key);
    if (!mf) {
        return xasprintf("%s is not a valid OXM field name", key);
    }
    if (!mf->writable) {
        return xasprintf("%s is read-only", key);
    }
    sf->field = mf;
    delim[0] = '\0';
    error = mf_parse(mf, value, &sf->value, &sf->mask);
    if (error) {
        return error;
    }

    if (!mf_is_value_valid(mf, &sf->value)) {
        return xasprintf("%s is not a valid value for field %s", value, key);
    }

    *usable_protocols &= mf->usable_protocols_exact;
    return NULL;
}

/* Parses 'arg' as the argument to a "set_field" action, and appends such an
 * action to 'ofpacts'.
 *
 * Returns NULL if successful, otherwise a malloc()'d string describing the
 * error.  The caller is responsible for freeing the returned string. */
static char * OVS_WARN_UNUSED_RESULT
parse_SET_FIELD(const char *arg, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols)
{
    char *copy = xstrdup(arg);
    char *error = set_field_parse__(copy, ofpacts, usable_protocols);
    free(copy);
    return error;
}

static char * OVS_WARN_UNUSED_RESULT
parse_reg_load(char *arg, struct ofpbuf *ofpacts)
{
    struct ofpact_set_field *sf = ofpact_put_reg_load(ofpacts);
    const char *full_arg = arg;
    uint64_t value = strtoull(arg, (char **) &arg, 0);
    struct mf_subfield dst;
    char *error;

    if (strncmp(arg, "->", 2)) {
        return xasprintf("%s: missing `->' following value", full_arg);
    }
    arg += 2;
    error = mf_parse_subfield(&dst, arg);
    if (error) {
        return error;
    }

    if (dst.n_bits < 64 && (value >> dst.n_bits) != 0) {
        return xasprintf("%s: value %"PRIu64" does not fit into %d bits",
                         full_arg, value, dst.n_bits);
    }

    sf->field = dst.field;
    memset(&sf->value, 0, sizeof sf->value);
    bitwise_put(value, &sf->value, dst.field->n_bytes, dst.ofs, dst.n_bits);
    bitwise_put(UINT64_MAX, &sf->mask,
                dst.field->n_bytes, dst.ofs, dst.n_bits);
    return NULL;
}

static void
format_SET_FIELD(const struct ofpact_set_field *a, struct ds *s)
{
    if (a->ofpact.raw == NXAST_RAW_REG_LOAD) {
        struct mf_subfield dst;
        uint64_t value;

        dst.ofs = dst.n_bits = 0;
        while (next_load_segment(a, &dst, &value)) {
            ds_put_format(s, "load:%#"PRIx64"->", value);
            mf_format_subfield(&dst, s);
            ds_put_char(s, ',');
        }
        ds_chomp(s, ',');
    } else {
        ds_put_cstr(s, "set_field:");
        mf_format(a->field, &a->value, &a->mask, s);
        ds_put_format(s, "->%s", a->field->name);
    }
}

/* Appends an OFPACT_SET_FIELD ofpact to 'ofpacts' and returns it.  The ofpact
 * is marked such that, if possible, it will be translated to OpenFlow as
 * NXAST_REG_LOAD extension actions rather than OFPAT_SET_FIELD, either because
 * that was the way that the action was expressed when it came into OVS or for
 * backward compatibility. */
struct ofpact_set_field *
ofpact_put_reg_load(struct ofpbuf *ofpacts)
{
    struct ofpact_set_field *sf = ofpact_put_SET_FIELD(ofpacts);
    sf->ofpact.raw = NXAST_RAW_REG_LOAD;
    return sf;
}

/* Action structure for NXAST_STACK_PUSH and NXAST_STACK_POP.
 *
 * Pushes (or pops) field[offset: offset + n_bits] to (or from)
 * top of the stack.
 */
struct nx_action_stack {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* Length is 16. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_STACK_PUSH or NXAST_STACK_POP. */
    ovs_be16 offset;                /* Bit offset into the field. */
    /* Followed by:
     * - OXM/NXM header for field to push or pop (4 or 8 bytes).
     * - ovs_be16 'n_bits', the number of bits to extract from the field.
     * - Enough 0-bytes to pad out the action to 24 bytes. */
    uint8_t pad[12];                /* See above. */
};
OFP_ASSERT(sizeof(struct nx_action_stack) == 24);

static enum ofperr
decode_stack_action(const struct nx_action_stack *nasp,
                    struct ofpact_stack *stack_action)
{
    enum ofperr error;
    struct ofpbuf b;

    stack_action->subfield.ofs = ntohs(nasp->offset);

    ofpbuf_use_const(&b, nasp, sizeof *nasp);
    ofpbuf_pull(&b, OBJECT_OFFSETOF(nasp, pad));
    error = nx_pull_header(&b, &stack_action->subfield.field, NULL);
    if (error) {
        return error;
    }
    stack_action->subfield.n_bits = ntohs(*(const ovs_be16 *) b.data);
    ofpbuf_pull(&b, 2);
    if (!is_all_zeros(b.data, b.size)) {
        return OFPERR_NXBRC_MUST_BE_ZERO;
    }

    return 0;
}

static enum ofperr
decode_NXAST_RAW_STACK_PUSH(const struct nx_action_stack *nasp,
                             struct ofpbuf *ofpacts)
{
    struct ofpact_stack *push = ofpact_put_STACK_PUSH(ofpacts);
    enum ofperr error = decode_stack_action(nasp, push);
    return error ? error : nxm_stack_push_check(push, NULL);
}

static enum ofperr
decode_NXAST_RAW_STACK_POP(const struct nx_action_stack *nasp,
                           struct ofpbuf *ofpacts)
{
    struct ofpact_stack *pop = ofpact_put_STACK_POP(ofpacts);
    enum ofperr error = decode_stack_action(nasp, pop);
    return error ? error : nxm_stack_pop_check(pop, NULL);
}

static void
encode_STACK_op(const struct ofpact_stack *stack_action,
                struct nx_action_stack *nasp)
{
    struct ofpbuf b;
    ovs_be16 n_bits;

    nasp->offset = htons(stack_action->subfield.ofs);

    ofpbuf_use_stack(&b, nasp, ntohs(nasp->len));
    ofpbuf_put_uninit(&b, OBJECT_OFFSETOF(nasp, pad));
    nx_put_header(&b, stack_action->subfield.field->id, 0, false);
    n_bits = htons(stack_action->subfield.n_bits);
    ofpbuf_put(&b, &n_bits, sizeof n_bits);
}

static void
encode_STACK_PUSH(const struct ofpact_stack *stack,
                  enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    encode_STACK_op(stack, put_NXAST_STACK_PUSH(out));
}

static void
encode_STACK_POP(const struct ofpact_stack *stack,
                 enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    encode_STACK_op(stack, put_NXAST_STACK_POP(out));
}

static char * OVS_WARN_UNUSED_RESULT
parse_STACK_PUSH(char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return nxm_parse_stack_action(ofpact_put_STACK_PUSH(ofpacts), arg);
}

static char * OVS_WARN_UNUSED_RESULT
parse_STACK_POP(char *arg, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return nxm_parse_stack_action(ofpact_put_STACK_POP(ofpacts), arg);
}

static void
format_STACK_PUSH(const struct ofpact_stack *a, struct ds *s)
{
    nxm_format_stack_push(a, s);
}

static void
format_STACK_POP(const struct ofpact_stack *a, struct ds *s)
{
    nxm_format_stack_pop(a, s);
}

/* Action structure for NXAST_DEC_TTL_CNT_IDS.
 *
 * If the packet is not IPv4 or IPv6, does nothing.  For IPv4 or IPv6, if the
 * TTL or hop limit is at least 2, decrements it by 1.  Otherwise, if TTL or
 * hop limit is 0 or 1, sends a packet-in to the controllers with each of the
 * 'n_controllers' controller IDs specified in 'cnt_ids'.
 *
 * (This differs from NXAST_DEC_TTL in that for NXAST_DEC_TTL the packet-in is
 * sent only to controllers with id 0.)
 */
struct nx_action_cnt_ids {
    ovs_be16 type;              /* OFPAT_VENDOR. */
    ovs_be16 len;               /* Length including slaves. */
    ovs_be32 vendor;            /* NX_VENDOR_ID. */
    ovs_be16 subtype;           /* NXAST_DEC_TTL_CNT_IDS. */

    ovs_be16 n_controllers;     /* Number of controllers. */
    uint8_t zeros[4];           /* Must be zero. */

    /* Followed by 1 or more controller ids.
     *
     * uint16_t cnt_ids[];        // Controller ids.
     * uint8_t pad[];           // Must be 0 to 8-byte align cnt_ids[].
     */
};
OFP_ASSERT(sizeof(struct nx_action_cnt_ids) == 16);

static enum ofperr
decode_OFPAT_RAW_DEC_NW_TTL(struct ofpbuf *out)
{
    uint16_t id = 0;
    struct ofpact_cnt_ids *ids;
    enum ofperr error = 0;

    ids = ofpact_put_DEC_TTL(out);
    ids->n_controllers = 1;
    ofpbuf_put(out, &id, sizeof id);
    ids = out->header;
    ofpact_update_len(out, &ids->ofpact);
    return error;
}

static enum ofperr
decode_NXAST_RAW_DEC_TTL_CNT_IDS(const struct nx_action_cnt_ids *nac_ids,
                                 struct ofpbuf *out)
{
    struct ofpact_cnt_ids *ids;
    size_t ids_size;
    int i;

    ids = ofpact_put_DEC_TTL(out);
    ids->ofpact.raw = NXAST_RAW_DEC_TTL_CNT_IDS;
    ids->n_controllers = ntohs(nac_ids->n_controllers);
    ids_size = ntohs(nac_ids->len) - sizeof *nac_ids;

    if (!is_all_zeros(nac_ids->zeros, sizeof nac_ids->zeros)) {
        return OFPERR_NXBRC_MUST_BE_ZERO;
    }

    if (ids_size < ids->n_controllers * sizeof(ovs_be16)) {
        VLOG_WARN_RL(&rl, "Nicira action dec_ttl_cnt_ids only has %"PRIuSIZE" "
                     "bytes allocated for controller ids.  %"PRIuSIZE" bytes "
                     "are required for %"PRIu16" controllers.",
                     ids_size, ids->n_controllers * sizeof(ovs_be16),
                     ids->n_controllers);
        return OFPERR_OFPBAC_BAD_LEN;
    }

    for (i = 0; i < ids->n_controllers; i++) {
        uint16_t id = ntohs(((ovs_be16 *)(nac_ids + 1))[i]);
        ofpbuf_put(out, &id, sizeof id);
        ids = out->header;
    }

    ofpact_update_len(out, &ids->ofpact);

    return 0;
}

static void
encode_DEC_TTL(const struct ofpact_cnt_ids *dec_ttl,
               enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (dec_ttl->ofpact.raw == NXAST_RAW_DEC_TTL_CNT_IDS
        || dec_ttl->n_controllers != 1
        || dec_ttl->cnt_ids[0] != 0) {
        struct nx_action_cnt_ids *nac_ids = put_NXAST_DEC_TTL_CNT_IDS(out);
        int ids_len = ROUND_UP(2 * dec_ttl->n_controllers, OFP_ACTION_ALIGN);
        ovs_be16 *ids;
        size_t i;

        nac_ids->len = htons(ntohs(nac_ids->len) + ids_len);
        nac_ids->n_controllers = htons(dec_ttl->n_controllers);

        ids = ofpbuf_put_zeros(out, ids_len);
        for (i = 0; i < dec_ttl->n_controllers; i++) {
            ids[i] = htons(dec_ttl->cnt_ids[i]);
        }
    } else {
        put_OFPAT_DEC_NW_TTL(out, ofp_version);
    }
}

static void
parse_noargs_dec_ttl(struct ofpbuf *ofpacts)
{
    struct ofpact_cnt_ids *ids;
    uint16_t id = 0;

    ofpact_put_DEC_TTL(ofpacts);
    ofpbuf_put(ofpacts, &id, sizeof id);
    ids = ofpacts->header;
    ids->n_controllers++;
    ofpact_update_len(ofpacts, &ids->ofpact);
}

static char * OVS_WARN_UNUSED_RESULT
parse_DEC_TTL(char *arg, struct ofpbuf *ofpacts,
              enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    if (*arg == '\0') {
        parse_noargs_dec_ttl(ofpacts);
    } else {
        struct ofpact_cnt_ids *ids;
        char *cntr;

        ids = ofpact_put_DEC_TTL(ofpacts);
        ids->ofpact.raw = NXAST_RAW_DEC_TTL_CNT_IDS;
        for (cntr = strtok_r(arg, ", ", &arg); cntr != NULL;
             cntr = strtok_r(NULL, ", ", &arg)) {
            uint16_t id = atoi(cntr);

            ofpbuf_put(ofpacts, &id, sizeof id);
            ids = ofpacts->header;
            ids->n_controllers++;
        }
        if (!ids->n_controllers) {
            return xstrdup("dec_ttl_cnt_ids: expected at least one controller "
                           "id.");
        }
        ofpact_update_len(ofpacts, &ids->ofpact);
    }
    return NULL;
}

static void
format_DEC_TTL(const struct ofpact_cnt_ids *a, struct ds *s)
{
    size_t i;

    ds_put_cstr(s, "dec_ttl");
    if (a->ofpact.raw == NXAST_RAW_DEC_TTL_CNT_IDS) {
        ds_put_cstr(s, "(");
        for (i = 0; i < a->n_controllers; i++) {
            if (i) {
                ds_put_cstr(s, ",");
            }
            ds_put_format(s, "%"PRIu16, a->cnt_ids[i]);
        }
        ds_put_cstr(s, ")");
    }
}

/* Set MPLS label actions. */

static enum ofperr
decode_OFPAT_RAW_SET_MPLS_LABEL(ovs_be32 label, struct ofpbuf *out)
{
    ofpact_put_SET_MPLS_LABEL(out)->label = label;
    return 0;
}

static void
encode_SET_MPLS_LABEL(const struct ofpact_mpls_label *label,
                      enum ofp_version ofp_version,
                                  struct ofpbuf *out)
{
    if (ofp_version < OFP12_VERSION) {
        put_OFPAT_SET_MPLS_LABEL(out, ofp_version, label->label);
    } else {
        ofpact_put_set_field(out, ofp_version, MFF_MPLS_LABEL,
                             ntohl(label->label));
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_MPLS_LABEL(char *arg, struct ofpbuf *ofpacts,
                     enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_mpls_label *mpls_label = ofpact_put_SET_MPLS_LABEL(ofpacts);
    if (*arg == '\0') {
        return xstrdup("set_mpls_label: expected label.");
    }

    mpls_label->label = htonl(atoi(arg));
    return NULL;
}

static void
format_SET_MPLS_LABEL(const struct ofpact_mpls_label *a, struct ds *s)
{
    ds_put_format(s, "set_mpls_label(%"PRIu32")", ntohl(a->label));
}

/* Set MPLS TC actions. */

static enum ofperr
decode_OFPAT_RAW_SET_MPLS_TC(uint8_t tc, struct ofpbuf *out)
{
    ofpact_put_SET_MPLS_TC(out)->tc = tc;
    return 0;
}

static void
encode_SET_MPLS_TC(const struct ofpact_mpls_tc *tc,
                   enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version < OFP12_VERSION) {
        put_OFPAT_SET_MPLS_TC(out, ofp_version, tc->tc);
    } else {
        ofpact_put_set_field(out, ofp_version, MFF_MPLS_TC, tc->tc);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_MPLS_TC(char *arg, struct ofpbuf *ofpacts,
                  enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_mpls_tc *mpls_tc = ofpact_put_SET_MPLS_TC(ofpacts);

    if (*arg == '\0') {
        return xstrdup("set_mpls_tc: expected tc.");
    }

    mpls_tc->tc = atoi(arg);
    return NULL;
}

static void
format_SET_MPLS_TC(const struct ofpact_mpls_tc *a, struct ds *s)
{
    ds_put_format(s, "set_mpls_ttl(%"PRIu8")", a->tc);
}

/* Set MPLS TTL actions. */

static enum ofperr
decode_OFPAT_RAW_SET_MPLS_TTL(uint8_t ttl, struct ofpbuf *out)
{
    ofpact_put_SET_MPLS_TTL(out)->ttl = ttl;
    return 0;
}

static void
encode_SET_MPLS_TTL(const struct ofpact_mpls_ttl *ttl,
                    enum ofp_version ofp_version, struct ofpbuf *out)
{
    put_OFPAT_SET_MPLS_TTL(out, ofp_version, ttl->ttl);
}

/* Parses 'arg' as the argument to a "set_mpls_ttl" action, and appends such an
 * action to 'ofpacts'.
 *
 * Returns NULL if successful, otherwise a malloc()'d string describing the
 * error.  The caller is responsible for freeing the returned string. */
static char * OVS_WARN_UNUSED_RESULT
parse_SET_MPLS_TTL(char *arg, struct ofpbuf *ofpacts,
                   enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_mpls_ttl *mpls_ttl = ofpact_put_SET_MPLS_TTL(ofpacts);

    if (*arg == '\0') {
        return xstrdup("set_mpls_ttl: expected ttl.");
    }

    mpls_ttl->ttl = atoi(arg);
    return NULL;
}

static void
format_SET_MPLS_TTL(const struct ofpact_mpls_ttl *a, struct ds *s)
{
    ds_put_format(s, "set_mpls_ttl(%"PRIu8")", a->ttl);
}

/* Decrement MPLS TTL actions. */

static enum ofperr
decode_OFPAT_RAW_DEC_MPLS_TTL(struct ofpbuf *out)
{
    ofpact_put_DEC_MPLS_TTL(out);
    return 0;
}

static void
encode_DEC_MPLS_TTL(const struct ofpact_null *null OVS_UNUSED,
                    enum ofp_version ofp_version, struct ofpbuf *out)
{
    put_OFPAT_DEC_MPLS_TTL(out, ofp_version);
}

static char * OVS_WARN_UNUSED_RESULT
parse_DEC_MPLS_TTL(char *arg OVS_UNUSED, struct ofpbuf *ofpacts,
                   enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    ofpact_put_DEC_MPLS_TTL(ofpacts);
    return NULL;
}

static void
format_DEC_MPLS_TTL(const struct ofpact_null *a OVS_UNUSED, struct ds *s)
{
    ds_put_cstr(s, "dec_mpls_ttl");
}

/* Push MPLS label action. */

static enum ofperr
decode_OFPAT_RAW_PUSH_MPLS(ovs_be16 ethertype, struct ofpbuf *out)
{
    struct ofpact_push_mpls *oam;

    if (!eth_type_mpls(ethertype)) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }
    oam = ofpact_put_PUSH_MPLS(out);
    oam->ethertype = ethertype;

    return 0;
}

static void
encode_PUSH_MPLS(const struct ofpact_push_mpls *push_mpls,
                 enum ofp_version ofp_version, struct ofpbuf *out)
{
    put_OFPAT_PUSH_MPLS(out, ofp_version, push_mpls->ethertype);
}

static char * OVS_WARN_UNUSED_RESULT
parse_PUSH_MPLS(char *arg, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    uint16_t ethertype;
    char *error;

    error = str_to_u16(arg, "push_mpls", &ethertype);
    if (!error) {
        ofpact_put_PUSH_MPLS(ofpacts)->ethertype = htons(ethertype);
    }
    return error;
}

static void
format_PUSH_MPLS(const struct ofpact_push_mpls *a, struct ds *s)
{
    ds_put_format(s, "push_mpls:0x%04"PRIx16, ntohs(a->ethertype));
}

/* Pop MPLS label action. */

static enum ofperr
decode_OFPAT_RAW_POP_MPLS(ovs_be16 ethertype, struct ofpbuf *out)
{
    ofpact_put_POP_MPLS(out)->ethertype = ethertype;
    return 0;
}

static void
encode_POP_MPLS(const struct ofpact_pop_mpls *pop_mpls,
                enum ofp_version ofp_version, struct ofpbuf *out)
{
    put_OFPAT_POP_MPLS(out, ofp_version, pop_mpls->ethertype);
}

static char * OVS_WARN_UNUSED_RESULT
parse_POP_MPLS(char *arg, struct ofpbuf *ofpacts,
                    enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    uint16_t ethertype;
    char *error;

    error = str_to_u16(arg, "pop_mpls", &ethertype);
    if (!error) {
        ofpact_put_POP_MPLS(ofpacts)->ethertype = htons(ethertype);
    }
    return error;
}

static void
format_POP_MPLS(const struct ofpact_pop_mpls *a, struct ds *s)
{
    ds_put_format(s, "pop_mpls:0x%04"PRIx16, ntohs(a->ethertype));
}

/* Set tunnel ID actions. */

static enum ofperr
decode_NXAST_RAW_SET_TUNNEL(uint32_t tun_id, struct ofpbuf *out)
{
    struct ofpact_tunnel *tunnel = ofpact_put_SET_TUNNEL(out);
    tunnel->ofpact.raw = NXAST_RAW_SET_TUNNEL;
    tunnel->tun_id = tun_id;
    return 0;
}

static enum ofperr
decode_NXAST_RAW_SET_TUNNEL64(uint64_t tun_id, struct ofpbuf *out)
{
    struct ofpact_tunnel *tunnel = ofpact_put_SET_TUNNEL(out);
    tunnel->ofpact.raw = NXAST_RAW_SET_TUNNEL64;
    tunnel->tun_id = tun_id;
    return 0;
}

static void
encode_SET_TUNNEL(const struct ofpact_tunnel *tunnel,
                  enum ofp_version ofp_version, struct ofpbuf *out)
{
    uint64_t tun_id = tunnel->tun_id;

    if (ofp_version < OFP12_VERSION) {
        if (tun_id <= UINT32_MAX
            && tunnel->ofpact.raw != NXAST_RAW_SET_TUNNEL64) {
            put_NXAST_SET_TUNNEL(out, tun_id);
        } else {
            put_NXAST_SET_TUNNEL64(out, tun_id);
        }
    } else {
        ofpact_put_set_field(out, ofp_version, MFF_TUN_ID, tun_id);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_set_tunnel(char *arg, struct ofpbuf *ofpacts,
                 enum ofp_raw_action_type raw)
{
    struct ofpact_tunnel *tunnel;

    tunnel = ofpact_put_SET_TUNNEL(ofpacts);
    tunnel->ofpact.raw = raw;
    return str_to_u64(arg, &tunnel->tun_id);
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_TUNNEL(char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return parse_set_tunnel(arg, ofpacts, NXAST_RAW_SET_TUNNEL);
}

static void
format_SET_TUNNEL(const struct ofpact_tunnel *a, struct ds *s)
{
    ds_put_format(s, "set_tunnel%s:%#"PRIx64,
                  (a->tun_id > UINT32_MAX
                   || a->ofpact.raw == NXAST_RAW_SET_TUNNEL64 ? "64" : ""),
                  a->tun_id);
}

/* Set queue action. */

static enum ofperr
decode_OFPAT_RAW_SET_QUEUE(uint32_t queue_id, struct ofpbuf *out)
{
    ofpact_put_SET_QUEUE(out)->queue_id = queue_id;
    return 0;
}

static void
encode_SET_QUEUE(const struct ofpact_queue *queue,
                 enum ofp_version ofp_version, struct ofpbuf *out)
{
    put_OFPAT_SET_QUEUE(out, ofp_version, queue->queue_id);
}

static char * OVS_WARN_UNUSED_RESULT
parse_SET_QUEUE(char *arg, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return str_to_u32(arg, &ofpact_put_SET_QUEUE(ofpacts)->queue_id);
}

static void
format_SET_QUEUE(const struct ofpact_queue *a, struct ds *s)
{
    ds_put_format(s, "set_queue:%"PRIu32, a->queue_id);
}

/* Pop queue action. */

static enum ofperr
decode_NXAST_RAW_POP_QUEUE(struct ofpbuf *out)
{
    ofpact_put_POP_QUEUE(out);
    return 0;
}

static void
encode_POP_QUEUE(const struct ofpact_null *null OVS_UNUSED,
                 enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    put_NXAST_POP_QUEUE(out);
}

static char * OVS_WARN_UNUSED_RESULT
parse_POP_QUEUE(const char *arg OVS_UNUSED, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    ofpact_put_POP_QUEUE(ofpacts);
    return NULL;
}

static void
format_POP_QUEUE(const struct ofpact_null *a OVS_UNUSED, struct ds *s)
{
    ds_put_cstr(s, "pop_queue");
}

/* Action structure for NXAST_FIN_TIMEOUT.
 *
 * This action changes the idle timeout or hard timeout, or both, of this
 * OpenFlow rule when the rule matches a TCP packet with the FIN or RST flag.
 * When such a packet is observed, the action reduces the rule's idle timeout
 * to 'fin_idle_timeout' and its hard timeout to 'fin_hard_timeout'.  This
 * action has no effect on an existing timeout that is already shorter than the
 * one that the action specifies.  A 'fin_idle_timeout' or 'fin_hard_timeout'
 * of zero has no effect on the respective timeout.
 *
 * 'fin_idle_timeout' and 'fin_hard_timeout' are measured in seconds.
 * 'fin_hard_timeout' specifies time since the flow's creation, not since the
 * receipt of the FIN or RST.
 *
 * This is useful for quickly discarding learned TCP flows that otherwise will
 * take a long time to expire.
 *
 * This action is intended for use with an OpenFlow rule that matches only a
 * single TCP flow.  If the rule matches multiple TCP flows (e.g. it wildcards
 * all TCP traffic, or all TCP traffic to a particular port), then any FIN or
 * RST in any of those flows will cause the entire OpenFlow rule to expire
 * early, which is not normally desirable.
 */
struct nx_action_fin_timeout {
    ovs_be16 type;              /* OFPAT_VENDOR. */
    ovs_be16 len;               /* 16. */
    ovs_be32 vendor;            /* NX_VENDOR_ID. */
    ovs_be16 subtype;           /* NXAST_FIN_TIMEOUT. */
    ovs_be16 fin_idle_timeout;  /* New idle timeout, if nonzero. */
    ovs_be16 fin_hard_timeout;  /* New hard timeout, if nonzero. */
    ovs_be16 pad;               /* Must be zero. */
};
OFP_ASSERT(sizeof(struct nx_action_fin_timeout) == 16);

static enum ofperr
decode_NXAST_RAW_FIN_TIMEOUT(const struct nx_action_fin_timeout *naft,
                             struct ofpbuf *out)
{
    struct ofpact_fin_timeout *oft;

    oft = ofpact_put_FIN_TIMEOUT(out);
    oft->fin_idle_timeout = ntohs(naft->fin_idle_timeout);
    oft->fin_hard_timeout = ntohs(naft->fin_hard_timeout);
    return 0;
}

static void
encode_FIN_TIMEOUT(const struct ofpact_fin_timeout *fin_timeout,
                   enum ofp_version ofp_version OVS_UNUSED,
                   struct ofpbuf *out)
{
    struct nx_action_fin_timeout *naft = put_NXAST_FIN_TIMEOUT(out);
    naft->fin_idle_timeout = htons(fin_timeout->fin_idle_timeout);
    naft->fin_hard_timeout = htons(fin_timeout->fin_hard_timeout);
}

static char * OVS_WARN_UNUSED_RESULT
parse_FIN_TIMEOUT(char *arg, struct ofpbuf *ofpacts,
                  enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_fin_timeout *oft = ofpact_put_FIN_TIMEOUT(ofpacts);
    char *key, *value;

    while (ofputil_parse_key_value(&arg, &key, &value)) {
        char *error;

        if (!strcmp(key, "idle_timeout")) {
            error =  str_to_u16(value, key, &oft->fin_idle_timeout);
        } else if (!strcmp(key, "hard_timeout")) {
            error = str_to_u16(value, key, &oft->fin_hard_timeout);
        } else {
            error = xasprintf("invalid key '%s' in 'fin_timeout' argument",
                              key);
        }

        if (error) {
            return error;
        }
    }
    return NULL;
}

static void
format_FIN_TIMEOUT(const struct ofpact_fin_timeout *a, struct ds *s)
{
    ds_put_cstr(s, "fin_timeout(");
    if (a->fin_idle_timeout) {
        ds_put_format(s, "idle_timeout=%"PRIu16",", a->fin_idle_timeout);
    }
    if (a->fin_hard_timeout) {
        ds_put_format(s, "hard_timeout=%"PRIu16",", a->fin_hard_timeout);
    }
    ds_chomp(s, ',');
    ds_put_char(s, ')');
}

/* Action structures for NXAST_RESUBMIT and NXAST_RESUBMIT_TABLE.
 *
 * These actions search one of the switch's flow tables:
 *
 *    - For NXAST_RESUBMIT_TABLE only, if the 'table' member is not 255, then
 *      it specifies the table to search.
 *
 *    - Otherwise (for NXAST_RESUBMIT_TABLE with a 'table' of 255, or for
 *      NXAST_RESUBMIT regardless of 'table'), it searches the current flow
 *      table, that is, the OpenFlow flow table that contains the flow from
 *      which this action was obtained.  If this action did not come from a
 *      flow table (e.g. it came from an OFPT_PACKET_OUT message), then table 0
 *      is the current table.
 *
 * The flow table lookup uses a flow that may be slightly modified from the
 * original lookup:
 *
 *    - For NXAST_RESUBMIT, the 'in_port' member of struct nx_action_resubmit
 *      is used as the flow's in_port.
 *
 *    - For NXAST_RESUBMIT_TABLE, if the 'in_port' member is not OFPP_IN_PORT,
 *      then its value is used as the flow's in_port.  Otherwise, the original
 *      in_port is used.
 *
 *    - If actions that modify the flow (e.g. OFPAT_SET_VLAN_VID) precede the
 *      resubmit action, then the flow is updated with the new values.
 *
 * Following the lookup, the original in_port is restored.
 *
 * If the modified flow matched in the flow table, then the corresponding
 * actions are executed.  Afterward, actions following the resubmit in the
 * original set of actions, if any, are executed; any changes made to the
 * packet (e.g. changes to VLAN) by secondary actions persist when those
 * actions are executed, although the original in_port is restored.
 *
 * Resubmit actions may be used any number of times within a set of actions.
 *
 * Resubmit actions may nest to an implementation-defined depth.  Beyond this
 * implementation-defined depth, further resubmit actions are simply ignored.
 *
 * NXAST_RESUBMIT ignores 'table' and 'pad'.  NXAST_RESUBMIT_TABLE requires
 * 'pad' to be all-bits-zero.
 *
 * Open vSwitch 1.0.1 and earlier did not support recursion.  Open vSwitch
 * before 1.2.90 did not support NXAST_RESUBMIT_TABLE.
 */
struct nx_action_resubmit {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* Length is 16. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_RESUBMIT. */
    ovs_be16 in_port;               /* New in_port for checking flow table. */
    uint8_t table;                  /* NXAST_RESUBMIT_TABLE: table to use. */
    uint8_t pad[3];
};
OFP_ASSERT(sizeof(struct nx_action_resubmit) == 16);

static enum ofperr
decode_NXAST_RAW_RESUBMIT(uint16_t port, struct ofpbuf *out)
{
    struct ofpact_resubmit *resubmit;

    resubmit = ofpact_put_RESUBMIT(out);
    resubmit->ofpact.raw = NXAST_RAW_RESUBMIT;
    resubmit->in_port = u16_to_ofp(port);
    resubmit->table_id = 0xff;
    return 0;
}

static enum ofperr
decode_NXAST_RAW_RESUBMIT_TABLE(const struct nx_action_resubmit *nar,
                                struct ofpbuf *out)
{
    struct ofpact_resubmit *resubmit;

    if (nar->pad[0] || nar->pad[1] || nar->pad[2]) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    resubmit = ofpact_put_RESUBMIT(out);
    resubmit->ofpact.raw = NXAST_RAW_RESUBMIT_TABLE;
    resubmit->in_port = u16_to_ofp(ntohs(nar->in_port));
    resubmit->table_id = nar->table;
    return 0;
}

static void
encode_RESUBMIT(const struct ofpact_resubmit *resubmit,
                enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    uint16_t in_port = ofp_to_u16(resubmit->in_port);

    if (resubmit->table_id == 0xff
        && resubmit->ofpact.raw != NXAST_RAW_RESUBMIT_TABLE) {
        put_NXAST_RESUBMIT(out, in_port);
    } else {
        struct nx_action_resubmit *nar = put_NXAST_RESUBMIT_TABLE(out);
        nar->table = resubmit->table_id;
        nar->in_port = htons(in_port);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_RESUBMIT(char *arg, struct ofpbuf *ofpacts,
               enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_resubmit *resubmit;
    char *in_port_s, *table_s;

    resubmit = ofpact_put_RESUBMIT(ofpacts);

    in_port_s = strsep(&arg, ",");
    if (in_port_s && in_port_s[0]) {
        if (!ofputil_port_from_string(in_port_s, &resubmit->in_port)) {
            return xasprintf("%s: resubmit to unknown port", in_port_s);
        }
    } else {
        resubmit->in_port = OFPP_IN_PORT;
    }

    table_s = strsep(&arg, ",");
    if (table_s && table_s[0]) {
        uint32_t table_id = 0;
        char *error;

        error = str_to_u32(table_s, &table_id);
        if (error) {
            return error;
        }
        resubmit->table_id = table_id;
    } else {
        resubmit->table_id = 255;
    }

    if (resubmit->in_port == OFPP_IN_PORT && resubmit->table_id == 255) {
        return xstrdup("at least one \"in_port\" or \"table\" must be "
                       "specified  on resubmit");
    }
    return NULL;
}

static void
format_RESUBMIT(const struct ofpact_resubmit *a, struct ds *s)
{
    if (a->in_port != OFPP_IN_PORT && a->table_id == 255) {
        ds_put_cstr(s, "resubmit:");
        ofputil_format_port(a->in_port, s);
    } else {
        ds_put_format(s, "resubmit(");
        if (a->in_port != OFPP_IN_PORT) {
            ofputil_format_port(a->in_port, s);
        }
        ds_put_char(s, ',');
        if (a->table_id != 255) {
            ds_put_format(s, "%"PRIu8, a->table_id);
        }
        ds_put_char(s, ')');
    }
}

/* Action structure for NXAST_LEARN.
 *
 * This action adds or modifies a flow in an OpenFlow table, similar to
 * OFPT_FLOW_MOD with OFPFC_MODIFY_STRICT as 'command'.  The new flow has the
 * specified idle timeout, hard timeout, priority, cookie, and flags.  The new
 * flow's match criteria and actions are built by applying each of the series
 * of flow_mod_spec elements included as part of the action.
 *
 * A flow_mod_spec starts with a 16-bit header.  A header that is all-bits-0 is
 * a no-op used for padding the action as a whole to a multiple of 8 bytes in
 * length.  Otherwise, the flow_mod_spec can be thought of as copying 'n_bits'
 * bits from a source to a destination.  In this case, the header contains
 * multiple fields:
 *
 *  15  14  13 12  11 10                              0
 * +------+---+------+---------------------------------+
 * |   0  |src|  dst |             n_bits              |
 * +------+---+------+---------------------------------+
 *
 * The meaning and format of a flow_mod_spec depends on 'src' and 'dst'.  The
 * following table summarizes the meaning of each possible combination.
 * Details follow the table:
 *
 *   src dst  meaning
 *   --- ---  ----------------------------------------------------------
 *    0   0   Add match criteria based on value in a field.
 *    1   0   Add match criteria based on an immediate value.
 *    0   1   Add NXAST_REG_LOAD action to copy field into a different field.
 *    1   1   Add NXAST_REG_LOAD action to load immediate value into a field.
 *    0   2   Add OFPAT_OUTPUT action to output to port from specified field.
 *   All other combinations are undefined and not allowed.
 *
 * The flow_mod_spec header is followed by a source specification and a
 * destination specification.  The format and meaning of the source
 * specification depends on 'src':
 *
 *   - If 'src' is 0, the source bits are taken from a field in the flow to
 *     which this action is attached.  (This should be a wildcarded field.  If
 *     its value is fully specified then the source bits being copied have
 *     constant values.)
 *
 *     The source specification is an ovs_be32 'field' and an ovs_be16 'ofs'.
 *     'field' is an nxm_header with nxm_hasmask=0, and 'ofs' the starting bit
 *     offset within that field.  The source bits are field[ofs:ofs+n_bits-1].
 *     'field' and 'ofs' are subject to the same restrictions as the source
 *     field in NXAST_REG_MOVE.
 *
 *   - If 'src' is 1, the source bits are a constant value.  The source
 *     specification is (n_bits+15)/16*2 bytes long.  Taking those bytes as a
 *     number in network order, the source bits are the 'n_bits'
 *     least-significant bits.  The switch will report an error if other bits
 *     in the constant are nonzero.
 *
 * The flow_mod_spec destination specification, for 'dst' of 0 or 1, is an
 * ovs_be32 'field' and an ovs_be16 'ofs'.  'field' is an nxm_header with
 * nxm_hasmask=0 and 'ofs' is a starting bit offset within that field.  The
 * meaning of the flow_mod_spec depends on 'dst':
 *
 *   - If 'dst' is 0, the flow_mod_spec specifies match criteria for the new
 *     flow.  The new flow matches only if bits field[ofs:ofs+n_bits-1] in a
 *     packet equal the source bits.  'field' may be any nxm_header with
 *     nxm_hasmask=0 that is allowed in NXT_FLOW_MOD.
 *
 *     Order is significant.  Earlier flow_mod_specs must satisfy any
 *     prerequisites for matching fields specified later, by copying constant
 *     values into prerequisite fields.
 *
 *     The switch will reject flow_mod_specs that do not satisfy NXM masking
 *     restrictions.
 *
 *   - If 'dst' is 1, the flow_mod_spec specifies an NXAST_REG_LOAD action for
 *     the new flow.  The new flow copies the source bits into
 *     field[ofs:ofs+n_bits-1].  Actions are executed in the same order as the
 *     flow_mod_specs.
 *
 *     A single NXAST_REG_LOAD action writes no more than 64 bits, so n_bits
 *     greater than 64 yields multiple NXAST_REG_LOAD actions.
 *
 * The flow_mod_spec destination spec for 'dst' of 2 (when 'src' is 0) is
 * empty.  It has the following meaning:
 *
 *   - The flow_mod_spec specifies an OFPAT_OUTPUT action for the new flow.
 *     The new flow outputs to the OpenFlow port specified by the source field.
 *     Of the special output ports with value OFPP_MAX or larger, OFPP_IN_PORT,
 *     OFPP_FLOOD, OFPP_LOCAL, and OFPP_ALL are supported.  Other special ports
 *     may not be used.
 *
 * Resource Management
 * -------------------
 *
 * A switch has a finite amount of flow table space available for learning.
 * When this space is exhausted, no new learning table entries will be learned
 * until some existing flow table entries expire.  The controller should be
 * prepared to handle this by flooding (which can be implemented as a
 * low-priority flow).
 *
 * If a learned flow matches a single TCP stream with a relatively long
 * timeout, one may make the best of resource constraints by setting
 * 'fin_idle_timeout' or 'fin_hard_timeout' (both measured in seconds), or
 * both, to shorter timeouts.  When either of these is specified as a nonzero
 * value, OVS adds a NXAST_FIN_TIMEOUT action, with the specified timeouts, to
 * the learned flow.
 *
 * Examples
 * --------
 *
 * The following examples give a prose description of the flow_mod_specs along
 * with informal notation for how those would be represented and a hex dump of
 * the bytes that would be required.
 *
 * These examples could work with various nx_action_learn parameters.  Typical
 * values would be idle_timeout=OFP_FLOW_PERMANENT, hard_timeout=60,
 * priority=OFP_DEFAULT_PRIORITY, flags=0, table_id=10.
 *
 * 1. Learn input port based on the source MAC, with lookup into
 *    NXM_NX_REG1[16:31] by resubmit to in_port=99:
 *
 *    Match on in_port=99:
 *       ovs_be16(src=1, dst=0, n_bits=16),               20 10
 *       ovs_be16(99),                                    00 63
 *       ovs_be32(NXM_OF_IN_PORT), ovs_be16(0)            00 00 00 02 00 00
 *
 *    Match Ethernet destination on Ethernet source from packet:
 *       ovs_be16(src=0, dst=0, n_bits=48),               00 30
 *       ovs_be32(NXM_OF_ETH_SRC), ovs_be16(0)            00 00 04 06 00 00
 *       ovs_be32(NXM_OF_ETH_DST), ovs_be16(0)            00 00 02 06 00 00
 *
 *    Set NXM_NX_REG1[16:31] to the packet's input port:
 *       ovs_be16(src=0, dst=1, n_bits=16),               08 10
 *       ovs_be32(NXM_OF_IN_PORT), ovs_be16(0)            00 00 00 02 00 00
 *       ovs_be32(NXM_NX_REG1), ovs_be16(16)              00 01 02 04 00 10
 *
 *    Given a packet that arrived on port A with Ethernet source address B,
 *    this would set up the flow "in_port=99, dl_dst=B,
 *    actions=load:A->NXM_NX_REG1[16..31]".
 *
 *    In syntax accepted by ovs-ofctl, this action is: learn(in_port=99,
 *    NXM_OF_ETH_DST[]=NXM_OF_ETH_SRC[],
 *    load:NXM_OF_IN_PORT[]->NXM_NX_REG1[16..31])
 *
 * 2. Output to input port based on the source MAC and VLAN VID, with lookup
 *    into NXM_NX_REG1[16:31]:
 *
 *    Match on same VLAN ID as packet:
 *       ovs_be16(src=0, dst=0, n_bits=12),               00 0c
 *       ovs_be32(NXM_OF_VLAN_TCI), ovs_be16(0)           00 00 08 02 00 00
 *       ovs_be32(NXM_OF_VLAN_TCI), ovs_be16(0)           00 00 08 02 00 00
 *
 *    Match Ethernet destination on Ethernet source from packet:
 *       ovs_be16(src=0, dst=0, n_bits=48),               00 30
 *       ovs_be32(NXM_OF_ETH_SRC), ovs_be16(0)            00 00 04 06 00 00
 *       ovs_be32(NXM_OF_ETH_DST), ovs_be16(0)            00 00 02 06 00 00
 *
 *    Output to the packet's input port:
 *       ovs_be16(src=0, dst=2, n_bits=16),               10 10
 *       ovs_be32(NXM_OF_IN_PORT), ovs_be16(0)            00 00 00 02 00 00
 *
 *    Given a packet that arrived on port A with Ethernet source address B in
 *    VLAN C, this would set up the flow "dl_dst=B, vlan_vid=C,
 *    actions=output:A".
 *
 *    In syntax accepted by ovs-ofctl, this action is:
 *    learn(NXM_OF_VLAN_TCI[0..11], NXM_OF_ETH_DST[]=NXM_OF_ETH_SRC[],
 *    output:NXM_OF_IN_PORT[])
 *
 * 3. Here's a recipe for a very simple-minded MAC learning switch.  It uses a
 *    10-second MAC expiration time to make it easier to see what's going on
 *
 *      ovs-vsctl del-controller br0
 *      ovs-ofctl del-flows br0
 *      ovs-ofctl add-flow br0 "table=0 actions=learn(table=1, \
          hard_timeout=10, NXM_OF_VLAN_TCI[0..11],             \
          NXM_OF_ETH_DST[]=NXM_OF_ETH_SRC[],                   \
          output:NXM_OF_IN_PORT[]), resubmit(,1)"
 *      ovs-ofctl add-flow br0 "table=1 priority=0 actions=flood"
 *
 *    You can then dump the MAC learning table with:
 *
 *      ovs-ofctl dump-flows br0 table=1
 *
 * Usage Advice
 * ------------
 *
 * For best performance, segregate learned flows into a table that is not used
 * for any other flows except possibly for a lowest-priority "catch-all" flow
 * (a flow with no match criteria).  If different learning actions specify
 * different match criteria, use different tables for the learned flows.
 *
 * The meaning of 'hard_timeout' and 'idle_timeout' can be counterintuitive.
 * These timeouts apply to the flow that is added, which means that a flow with
 * an idle timeout will expire when no traffic has been sent *to* the learned
 * address.  This is not usually the intent in MAC learning; instead, we want
 * the MAC learn entry to expire when no traffic has been sent *from* the
 * learned address.  Use a hard timeout for that.
 */
struct nx_action_learn {
    ovs_be16 type;              /* OFPAT_VENDOR. */
    ovs_be16 len;               /* At least 24. */
    ovs_be32 vendor;            /* NX_VENDOR_ID. */
    ovs_be16 subtype;           /* NXAST_LEARN. */
    ovs_be16 idle_timeout;      /* Idle time before discarding (seconds). */
    ovs_be16 hard_timeout;      /* Max time before discarding (seconds). */
    ovs_be16 priority;          /* Priority level of flow entry. */
    ovs_be64 cookie;            /* Cookie for new flow. */
    ovs_be16 flags;             /* NX_LEARN_F_*. */
    uint8_t table_id;           /* Table to insert flow entry. */
    uint8_t pad;                /* Must be zero. */
    ovs_be16 fin_idle_timeout;  /* Idle timeout after FIN, if nonzero. */
    ovs_be16 fin_hard_timeout;  /* Hard timeout after FIN, if nonzero. */
    /* Followed by a sequence of flow_mod_spec elements, as described above,
     * until the end of the action is reached. */
};
OFP_ASSERT(sizeof(struct nx_action_learn) == 32);

static ovs_be16
get_be16(const void **pp)
{
    const ovs_be16 *p = *pp;
    ovs_be16 value = *p;
    *pp = p + 1;
    return value;
}

static ovs_be32
get_be32(const void **pp)
{
    const ovs_be32 *p = *pp;
    ovs_be32 value = get_unaligned_be32(p);
    *pp = p + 1;
    return value;
}

static void
get_subfield(int n_bits, const void **p, struct mf_subfield *sf)
{
    sf->field = mf_from_nxm_header(ntohl(get_be32(p)));
    sf->ofs = ntohs(get_be16(p));
    sf->n_bits = n_bits;
}

static unsigned int
learn_min_len(uint16_t header)
{
    int n_bits = header & NX_LEARN_N_BITS_MASK;
    int src_type = header & NX_LEARN_SRC_MASK;
    int dst_type = header & NX_LEARN_DST_MASK;
    unsigned int min_len;

    min_len = 0;
    if (src_type == NX_LEARN_SRC_FIELD) {
        min_len += sizeof(ovs_be32); /* src_field */
        min_len += sizeof(ovs_be16); /* src_ofs */
    } else {
        min_len += DIV_ROUND_UP(n_bits, 16);
    }
    if (dst_type == NX_LEARN_DST_MATCH ||
        dst_type == NX_LEARN_DST_LOAD) {
        min_len += sizeof(ovs_be32); /* dst_field */
        min_len += sizeof(ovs_be16); /* dst_ofs */
    }
    return min_len;
}

/* Converts 'nal' into a "struct ofpact_learn" and appends that struct to
 * 'ofpacts'.  Returns 0 if successful, otherwise an OFPERR_*. */
static enum ofperr
decode_NXAST_RAW_LEARN(const struct nx_action_learn *nal,
                       struct ofpbuf *ofpacts)
{
    struct ofpact_learn *learn;
    const void *p, *end;

    if (nal->pad) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    learn = ofpact_put_LEARN(ofpacts);

    learn->idle_timeout = ntohs(nal->idle_timeout);
    learn->hard_timeout = ntohs(nal->hard_timeout);
    learn->priority = ntohs(nal->priority);
    learn->cookie = nal->cookie;
    learn->table_id = nal->table_id;
    learn->fin_idle_timeout = ntohs(nal->fin_idle_timeout);
    learn->fin_hard_timeout = ntohs(nal->fin_hard_timeout);

    learn->flags = ntohs(nal->flags);
    if (learn->flags & ~(NX_LEARN_F_SEND_FLOW_REM |
                         NX_LEARN_F_DELETE_LEARNED)) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    if (learn->table_id == 0xff) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    end = (char *) nal + ntohs(nal->len);
    for (p = nal + 1; p != end; ) {
        struct ofpact_learn_spec *spec;
        uint16_t header = ntohs(get_be16(&p));

        if (!header) {
            break;
        }

        spec = ofpbuf_put_zeros(ofpacts, sizeof *spec);
        learn = ofpacts->header;
        learn->n_specs++;

        spec->src_type = header & NX_LEARN_SRC_MASK;
        spec->dst_type = header & NX_LEARN_DST_MASK;
        spec->n_bits = header & NX_LEARN_N_BITS_MASK;

        /* Check for valid src and dst type combination. */
        if (spec->dst_type == NX_LEARN_DST_MATCH ||
            spec->dst_type == NX_LEARN_DST_LOAD ||
            (spec->dst_type == NX_LEARN_DST_OUTPUT &&
             spec->src_type == NX_LEARN_SRC_FIELD)) {
            /* OK. */
        } else {
            return OFPERR_OFPBAC_BAD_ARGUMENT;
        }

        /* Check that the arguments don't overrun the end of the action. */
        if ((char *) end - (char *) p < learn_min_len(header)) {
            return OFPERR_OFPBAC_BAD_LEN;
        }

        /* Get the source. */
        if (spec->src_type == NX_LEARN_SRC_FIELD) {
            get_subfield(spec->n_bits, &p, &spec->src);
        } else {
            int p_bytes = 2 * DIV_ROUND_UP(spec->n_bits, 16);

            bitwise_copy(p, p_bytes, 0,
                         &spec->src_imm, sizeof spec->src_imm, 0,
                         spec->n_bits);
            p = (const uint8_t *) p + p_bytes;
        }

        /* Get the destination. */
        if (spec->dst_type == NX_LEARN_DST_MATCH ||
            spec->dst_type == NX_LEARN_DST_LOAD) {
            get_subfield(spec->n_bits, &p, &spec->dst);
        }
    }
    ofpact_update_len(ofpacts, &learn->ofpact);

    if (!is_all_zeros(p, (char *) end - (char *) p)) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    return 0;
}

static void
put_be16(struct ofpbuf *b, ovs_be16 x)
{
    ofpbuf_put(b, &x, sizeof x);
}

static void
put_be32(struct ofpbuf *b, ovs_be32 x)
{
    ofpbuf_put(b, &x, sizeof x);
}

static void
put_u16(struct ofpbuf *b, uint16_t x)
{
    put_be16(b, htons(x));
}

static void
put_u32(struct ofpbuf *b, uint32_t x)
{
    put_be32(b, htonl(x));
}

static void
encode_LEARN(const struct ofpact_learn *learn,
             enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    const struct ofpact_learn_spec *spec;
    struct nx_action_learn *nal;
    size_t start_ofs;

    start_ofs = out->size;
    nal = put_NXAST_LEARN(out);
    nal->idle_timeout = htons(learn->idle_timeout);
    nal->hard_timeout = htons(learn->hard_timeout);
    nal->fin_idle_timeout = htons(learn->fin_idle_timeout);
    nal->fin_hard_timeout = htons(learn->fin_hard_timeout);
    nal->priority = htons(learn->priority);
    nal->cookie = learn->cookie;
    nal->flags = htons(learn->flags);
    nal->table_id = learn->table_id;

    for (spec = learn->specs; spec < &learn->specs[learn->n_specs]; spec++) {
        put_u16(out, spec->n_bits | spec->dst_type | spec->src_type);

        if (spec->src_type == NX_LEARN_SRC_FIELD) {
            put_u32(out, mf_nxm_header(spec->src.field->id));
            put_u16(out, spec->src.ofs);
        } else {
            size_t n_dst_bytes = 2 * DIV_ROUND_UP(spec->n_bits, 16);
            uint8_t *bits = ofpbuf_put_zeros(out, n_dst_bytes);
            bitwise_copy(&spec->src_imm, sizeof spec->src_imm, 0,
                         bits, n_dst_bytes, 0,
                         spec->n_bits);
        }

        if (spec->dst_type == NX_LEARN_DST_MATCH ||
            spec->dst_type == NX_LEARN_DST_LOAD) {
            put_u32(out, mf_nxm_header(spec->dst.field->id));
            put_u16(out, spec->dst.ofs);
        }
    }

    pad_ofpat(out, start_ofs);
}

static char * OVS_WARN_UNUSED_RESULT
parse_LEARN(char *arg, struct ofpbuf *ofpacts,
            enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return learn_parse(arg, ofpacts);
}

static void
format_LEARN(const struct ofpact_learn *a, struct ds *s)
{
    learn_format(a, s);
}

/* Action structure for NXAST_CONJUNCTION. */
struct nx_action_conjunction {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* At least 16. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* See enum ofp_raw_action_type. */
    uint8_t clause;
    uint8_t n_clauses;
    ovs_be32 id;
};
OFP_ASSERT(sizeof(struct nx_action_conjunction) == 16);

static void
add_conjunction(struct ofpbuf *out,
                uint32_t id, uint8_t clause, uint8_t n_clauses)
{
    struct ofpact_conjunction *oc;

    oc = ofpact_put_CONJUNCTION(out);
    oc->id = id;
    oc->clause = clause;
    oc->n_clauses = n_clauses;
}

static enum ofperr
decode_NXAST_RAW_CONJUNCTION(const struct nx_action_conjunction *nac,
                             struct ofpbuf *out)
{
    if (nac->n_clauses < 2 || nac->n_clauses > 64
        || nac->clause >= nac->n_clauses) {
        return OFPERR_NXBAC_BAD_CONJUNCTION;
    } else {
        add_conjunction(out, ntohl(nac->id), nac->clause, nac->n_clauses);
        return 0;
    }
}

static void
encode_CONJUNCTION(const struct ofpact_conjunction *oc,
                   enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    struct nx_action_conjunction *nac = put_NXAST_CONJUNCTION(out);
    nac->clause = oc->clause;
    nac->n_clauses = oc->n_clauses;
    nac->id = htonl(oc->id);
}

static void
format_CONJUNCTION(const struct ofpact_conjunction *oc, struct ds *s)
{
    ds_put_format(s, "conjunction(%"PRIu32",%"PRIu8"/%"PRIu8")",
                  oc->id, oc->clause + 1, oc->n_clauses);
}

static char * OVS_WARN_UNUSED_RESULT
parse_CONJUNCTION(const char *arg, struct ofpbuf *ofpacts,
                  enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    uint8_t n_clauses;
    uint8_t clause;
    uint32_t id;
    int n;

    if (!ovs_scan(arg, "%"SCNi32" , %"SCNu8" / %"SCNu8" %n",
                  &id, &clause, &n_clauses, &n) || n != strlen(arg)) {
        return xstrdup("\"conjunction\" syntax is \"conjunction(id,i/n)\"");
    }

    if (n_clauses < 2) {
        return xstrdup("conjunction must have at least 2 clauses");
    } else if (n_clauses > 64) {
        return xstrdup("conjunction must have at most 64 clauses");
    } else if (clause < 1) {
        return xstrdup("clause index must be positive");
    } else if (clause > n_clauses) {
        return xstrdup("clause index must be less than or equal to "
                       "number of clauses");
    }

    add_conjunction(ofpacts, id, clause - 1, n_clauses);
    return NULL;
}

/* Action structure for NXAST_MULTIPATH.
 *
 * This action performs the following steps in sequence:
 *
 *    1. Hashes the fields designated by 'fields', one of NX_HASH_FIELDS_*.
 *       Refer to the definition of "enum nx_mp_fields" for details.
 *
 *       The 'basis' value is used as a universal hash parameter, that is,
 *       different values of 'basis' yield different hash functions.  The
 *       particular universal hash function used is implementation-defined.
 *
 *       The hashed fields' values are drawn from the current state of the
 *       flow, including all modifications that have been made by actions up to
 *       this point.
 *
 *    2. Applies the multipath link choice algorithm specified by 'algorithm',
 *       one of NX_MP_ALG_*.  Refer to the definition of "enum nx_mp_algorithm"
 *       for details.
 *
 *       The output of the algorithm is 'link', an unsigned integer less than
 *       or equal to 'max_link'.
 *
 *       Some algorithms use 'arg' as an additional argument.
 *
 *    3. Stores 'link' in dst[ofs:ofs+n_bits].  The format and semantics of
 *       'dst' and 'ofs_nbits' are similar to those for the NXAST_REG_LOAD
 *       action.
 *
 * The switch will reject actions that have an unknown 'fields', or an unknown
 * 'algorithm', or in which ofs+n_bits is greater than the width of 'dst', or
 * in which 'max_link' is greater than or equal to 2**n_bits, with error type
 * OFPET_BAD_ACTION, code OFPBAC_BAD_ARGUMENT.
 */
struct nx_action_multipath {
    ovs_be16 type;              /* OFPAT_VENDOR. */
    ovs_be16 len;               /* Length is 32. */
    ovs_be32 vendor;            /* NX_VENDOR_ID. */
    ovs_be16 subtype;           /* NXAST_MULTIPATH. */

    /* What fields to hash and how. */
    ovs_be16 fields;            /* One of NX_HASH_FIELDS_*. */
    ovs_be16 basis;             /* Universal hash parameter. */
    ovs_be16 pad0;

    /* Multipath link choice algorithm to apply to hash value. */
    ovs_be16 algorithm;         /* One of NX_MP_ALG_*. */
    ovs_be16 max_link;          /* Number of output links, minus 1. */
    ovs_be32 arg;               /* Algorithm-specific argument. */
    ovs_be16 pad1;

    /* Where to store the result. */
    ovs_be16 ofs_nbits;         /* (ofs << 6) | (n_bits - 1). */
    ovs_be32 dst;               /* Destination. */
};
OFP_ASSERT(sizeof(struct nx_action_multipath) == 32);

static enum ofperr
decode_NXAST_RAW_MULTIPATH(const struct nx_action_multipath *nam,
                           struct ofpbuf *out)
{
    uint32_t n_links = ntohs(nam->max_link) + 1;
    size_t min_n_bits = log_2_ceil(n_links);
    struct ofpact_multipath *mp;

    mp = ofpact_put_MULTIPATH(out);
    mp->fields = ntohs(nam->fields);
    mp->basis = ntohs(nam->basis);
    mp->algorithm = ntohs(nam->algorithm);
    mp->max_link = ntohs(nam->max_link);
    mp->arg = ntohl(nam->arg);
    mp->dst.field = mf_from_nxm_header(ntohl(nam->dst));
    mp->dst.ofs = nxm_decode_ofs(nam->ofs_nbits);
    mp->dst.n_bits = nxm_decode_n_bits(nam->ofs_nbits);

    if (!flow_hash_fields_valid(mp->fields)) {
        VLOG_WARN_RL(&rl, "unsupported fields %d", (int) mp->fields);
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    } else if (mp->algorithm != NX_MP_ALG_MODULO_N
               && mp->algorithm != NX_MP_ALG_HASH_THRESHOLD
               && mp->algorithm != NX_MP_ALG_HRW
               && mp->algorithm != NX_MP_ALG_ITER_HASH) {
        VLOG_WARN_RL(&rl, "unsupported algorithm %d", (int) mp->algorithm);
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    } else if (mp->dst.n_bits < min_n_bits) {
        VLOG_WARN_RL(&rl, "multipath action requires at least %"PRIuSIZE" bits for "
                     "%"PRIu32" links", min_n_bits, n_links);
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    return multipath_check(mp, NULL);
}

static void
encode_MULTIPATH(const struct ofpact_multipath *mp,
                 enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    struct nx_action_multipath *nam = put_NXAST_MULTIPATH(out);

    nam->fields = htons(mp->fields);
    nam->basis = htons(mp->basis);
    nam->algorithm = htons(mp->algorithm);
    nam->max_link = htons(mp->max_link);
    nam->arg = htonl(mp->arg);
    nam->ofs_nbits = nxm_encode_ofs_nbits(mp->dst.ofs, mp->dst.n_bits);
    nam->dst = htonl(mf_nxm_header(mp->dst.field->id));
}

static char * OVS_WARN_UNUSED_RESULT
parse_MULTIPATH(const char *arg, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    return multipath_parse(ofpact_put_MULTIPATH(ofpacts), arg);
}

static void
format_MULTIPATH(const struct ofpact_multipath *a, struct ds *s)
{
    multipath_format(a, s);
}

/* Action structure for NXAST_NOTE.
 *
 * This action has no effect.  It is variable length.  The switch does not
 * attempt to interpret the user-defined 'note' data in any way.  A controller
 * can use this action to attach arbitrary metadata to a flow.
 *
 * This action might go away in the future.
 */
struct nx_action_note {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* A multiple of 8, but at least 16. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_NOTE. */
    uint8_t note[6];                /* Start of user-defined data. */
    /* Possibly followed by additional user-defined data. */
};
OFP_ASSERT(sizeof(struct nx_action_note) == 16);

static enum ofperr
decode_NXAST_RAW_NOTE(const struct nx_action_note *nan, struct ofpbuf *out)
{
    struct ofpact_note *note;
    unsigned int length;

    length = ntohs(nan->len) - offsetof(struct nx_action_note, note);
    note = ofpact_put(out, OFPACT_NOTE,
                      offsetof(struct ofpact_note, data) + length);
    note->length = length;
    memcpy(note->data, nan->note, length);

    return 0;
}

static void
encode_NOTE(const struct ofpact_note *note,
            enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    size_t start_ofs = out->size;
    struct nx_action_note *nan;
    unsigned int remainder;
    unsigned int len;

    put_NXAST_NOTE(out);
    out->size = out->size - sizeof nan->note;

    ofpbuf_put(out, note->data, note->length);

    len = out->size - start_ofs;
    remainder = len % OFP_ACTION_ALIGN;
    if (remainder) {
        ofpbuf_put_zeros(out, OFP_ACTION_ALIGN - remainder);
    }
    nan = ofpbuf_at(out, start_ofs, sizeof *nan);
    nan->len = htons(out->size - start_ofs);
}

static char * OVS_WARN_UNUSED_RESULT
parse_NOTE(const char *arg, struct ofpbuf *ofpacts,
           enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_note *note;

    note = ofpact_put_NOTE(ofpacts);
    while (*arg != '\0') {
        uint8_t byte;
        bool ok;

        if (*arg == '.') {
            arg++;
        }
        if (*arg == '\0') {
            break;
        }

        byte = hexits_value(arg, 2, &ok);
        if (!ok) {
            return xstrdup("bad hex digit in `note' argument");
        }
        ofpbuf_put(ofpacts, &byte, 1);

        note = ofpacts->header;
        note->length++;

        arg += 2;
    }
    ofpact_update_len(ofpacts, &note->ofpact);
    return NULL;
}

static void
format_NOTE(const struct ofpact_note *a, struct ds *s)
{
    size_t i;

    ds_put_cstr(s, "note:");
    for (i = 0; i < a->length; i++) {
        if (i) {
            ds_put_char(s, '.');
        }
        ds_put_format(s, "%02"PRIx8, a->data[i]);
    }
}

/* Exit action. */

static enum ofperr
decode_NXAST_RAW_EXIT(struct ofpbuf *out)
{
    ofpact_put_EXIT(out);
    return 0;
}

static void
encode_EXIT(const struct ofpact_null *null OVS_UNUSED,
            enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    put_NXAST_EXIT(out);
}

static char * OVS_WARN_UNUSED_RESULT
parse_EXIT(char *arg OVS_UNUSED, struct ofpbuf *ofpacts,
           enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    ofpact_put_EXIT(ofpacts);
    return NULL;
}

static void
format_EXIT(const struct ofpact_null *a OVS_UNUSED, struct ds *s)
{
    ds_put_cstr(s, "exit");
}

/* Unroll xlate action. */

static void
encode_UNROLL_XLATE(const struct ofpact_unroll_xlate *unroll OVS_UNUSED,
                    enum ofp_version ofp_version OVS_UNUSED,
                    struct ofpbuf *out OVS_UNUSED)
{
    OVS_NOT_REACHED();
}

static char * OVS_WARN_UNUSED_RESULT
parse_UNROLL_XLATE(char *arg OVS_UNUSED, struct ofpbuf *ofpacts OVS_UNUSED,
                   enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    OVS_NOT_REACHED();
    return NULL;
}

static void
format_UNROLL_XLATE(const struct ofpact_unroll_xlate *a OVS_UNUSED,
                    struct ds *s)
{
    ds_put_cstr(s, "unroll_xlate");
}

/* Action structure for NXAST_SAMPLE.
 *
 * Samples matching packets with the given probability and sends them
 * each to the set of collectors identified with the given ID.  The
 * probability is expressed as a number of packets to be sampled out
 * of USHRT_MAX packets, and must be >0.
 *
 * When sending packet samples to IPFIX collectors, the IPFIX flow
 * record sent for each sampled packet is associated with the given
 * observation domain ID and observation point ID.  Each IPFIX flow
 * record contain the sampled packet's headers when executing this
 * rule.  If a sampled packet's headers are modified by previous
 * actions in the flow, those modified headers are sent. */
struct nx_action_sample {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* Length is 24. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_SAMPLE. */
    ovs_be16 probability;           /* Fraction of packets to sample. */
    ovs_be32 collector_set_id;      /* ID of collector set in OVSDB. */
    ovs_be32 obs_domain_id;         /* ID of sampling observation domain. */
    ovs_be32 obs_point_id;          /* ID of sampling observation point. */
};
OFP_ASSERT(sizeof(struct nx_action_sample) == 24);

static enum ofperr
decode_NXAST_RAW_SAMPLE(const struct nx_action_sample *nas, struct ofpbuf *out)
{
    struct ofpact_sample *sample;

    sample = ofpact_put_SAMPLE(out);
    sample->probability = ntohs(nas->probability);
    sample->collector_set_id = ntohl(nas->collector_set_id);
    sample->obs_domain_id = ntohl(nas->obs_domain_id);
    sample->obs_point_id = ntohl(nas->obs_point_id);

    if (sample->probability == 0) {
        return OFPERR_OFPBAC_BAD_ARGUMENT;
    }

    return 0;
}

static void
encode_SAMPLE(const struct ofpact_sample *sample,
              enum ofp_version ofp_version OVS_UNUSED, struct ofpbuf *out)
{
    struct nx_action_sample *nas;

    nas = put_NXAST_SAMPLE(out);
    nas->probability = htons(sample->probability);
    nas->collector_set_id = htonl(sample->collector_set_id);
    nas->obs_domain_id = htonl(sample->obs_domain_id);
    nas->obs_point_id = htonl(sample->obs_point_id);
}

/* Parses 'arg' as the argument to a "sample" action, and appends such an
 * action to 'ofpacts'.
 *
 * Returns NULL if successful, otherwise a malloc()'d string describing the
 * error.  The caller is responsible for freeing the returned string. */
static char * OVS_WARN_UNUSED_RESULT
parse_SAMPLE(char *arg, struct ofpbuf *ofpacts,
             enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_sample *os = ofpact_put_SAMPLE(ofpacts);
    char *key, *value;

    while (ofputil_parse_key_value(&arg, &key, &value)) {
        char *error = NULL;

        if (!strcmp(key, "probability")) {
            error = str_to_u16(value, "probability", &os->probability);
            if (!error && os->probability == 0) {
                error = xasprintf("invalid probability value \"%s\"", value);
            }
        } else if (!strcmp(key, "collector_set_id")) {
            error = str_to_u32(value, &os->collector_set_id);
        } else if (!strcmp(key, "obs_domain_id")) {
            error = str_to_u32(value, &os->obs_domain_id);
        } else if (!strcmp(key, "obs_point_id")) {
            error = str_to_u32(value, &os->obs_point_id);
        } else {
            error = xasprintf("invalid key \"%s\" in \"sample\" argument",
                              key);
        }
        if (error) {
            return error;
        }
    }
    if (os->probability == 0) {
        return xstrdup("non-zero \"probability\" must be specified on sample");
    }
    return NULL;
}

static void
format_SAMPLE(const struct ofpact_sample *a, struct ds *s)
{
    ds_put_format(s, "sample(probability=%"PRIu16",collector_set_id=%"PRIu32
                  ",obs_domain_id=%"PRIu32",obs_point_id=%"PRIu32")",
                  a->probability, a->collector_set_id,
                  a->obs_domain_id, a->obs_point_id);
}

/* Meter instruction. */

static void
encode_METER(const struct ofpact_meter *meter,
             enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version >= OFP13_VERSION) {
        instruction_put_OFPIT13_METER(out)->meter_id = htonl(meter->meter_id);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_METER(char *arg, struct ofpbuf *ofpacts,
            enum ofputil_protocol *usable_protocols)
{
    *usable_protocols &= OFPUTIL_P_OF13_UP;
    return str_to_u32(arg, &ofpact_put_METER(ofpacts)->meter_id);
}

static void
format_METER(const struct ofpact_meter *a, struct ds *s)
{
    ds_put_format(s, "meter:%"PRIu32, a->meter_id);
}

/* Clear-Actions instruction. */

static void
encode_CLEAR_ACTIONS(const struct ofpact_null *null OVS_UNUSED,
                     enum ofp_version ofp_version OVS_UNUSED,
                     struct ofpbuf *out OVS_UNUSED)
{
    if (ofp_version > OFP10_VERSION) {
        instruction_put_OFPIT11_CLEAR_ACTIONS(out);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_CLEAR_ACTIONS(char *arg OVS_UNUSED, struct ofpbuf *ofpacts,
                    enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    ofpact_put_CLEAR_ACTIONS(ofpacts);
    return NULL;
}

static void
format_CLEAR_ACTIONS(const struct ofpact_null *a OVS_UNUSED, struct ds *s)
{
    ds_put_cstr(s, "clear_actions");
}

/* Write-Actions instruction. */

static void
encode_WRITE_ACTIONS(const struct ofpact_nest *actions,
                     enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version > OFP10_VERSION) {
        const size_t ofs = out->size;

        instruction_put_OFPIT11_WRITE_ACTIONS(out);
        ofpacts_put_openflow_actions(actions->actions,
                                     ofpact_nest_get_action_len(actions),
                                     out, ofp_version);
        ofpacts_update_instruction_actions(out, ofs);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_WRITE_ACTIONS(char *arg, struct ofpbuf *ofpacts,
                    enum ofputil_protocol *usable_protocols)
{
    struct ofpact_nest *on;
    char *error;
    size_t ofs;

    /* Pull off existing actions or instructions. */
    ofpact_pad(ofpacts);
    ofs = ofpacts->size;
    ofpbuf_pull(ofpacts, ofs);

    /* Add a Write-Actions instruction and then pull it off. */
    ofpact_put(ofpacts, OFPACT_WRITE_ACTIONS, sizeof *on);
    ofpbuf_pull(ofpacts, sizeof *on);

    /* Parse nested actions.
     *
     * We pulled off "write-actions" and the previous actions because the
     * OFPACT_WRITE_ACTIONS is only partially constructed: its length is such
     * that it doesn't actually include the nested actions.  That means that
     * ofpacts_parse() would reject them as being part of an Apply-Actions that
     * follows a Write-Actions, which is an invalid order.  */
    error = ofpacts_parse(arg, ofpacts, usable_protocols, false);

    /* Put the Write-Actions back on and update its length. */
    on = ofpbuf_push_uninit(ofpacts, sizeof *on);
    on->ofpact.len = ofpacts->size;

    /* Put any previous actions or instructions back on. */
    ofpbuf_push_uninit(ofpacts, ofs);

    return error;
}

static void
format_WRITE_ACTIONS(const struct ofpact_nest *a, struct ds *s)
{
    ds_put_cstr(s, "write_actions(");
    ofpacts_format(a->actions, ofpact_nest_get_action_len(a), s);
    ds_put_char(s, ')');
}

/* Action structure for NXAST_WRITE_METADATA.
 *
 * Modifies the 'mask' bits of the metadata value. */
struct nx_action_write_metadata {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* Length is 32. */
    ovs_be32 vendor;                /* NX_VENDOR_ID. */
    ovs_be16 subtype;               /* NXAST_WRITE_METADATA. */
    uint8_t zeros[6];               /* Must be zero. */
    ovs_be64 metadata;              /* Metadata register. */
    ovs_be64 mask;                  /* Metadata mask. */
};
OFP_ASSERT(sizeof(struct nx_action_write_metadata) == 32);

static enum ofperr
decode_NXAST_RAW_WRITE_METADATA(const struct nx_action_write_metadata *nawm,
                                struct ofpbuf *out)
{
    struct ofpact_metadata *om;

    if (!is_all_zeros(nawm->zeros, sizeof nawm->zeros)) {
        return OFPERR_NXBRC_MUST_BE_ZERO;
    }

    om = ofpact_put_WRITE_METADATA(out);
    om->metadata = nawm->metadata;
    om->mask = nawm->mask;

    return 0;
}

static void
encode_WRITE_METADATA(const struct ofpact_metadata *metadata,
                      enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version == OFP10_VERSION) {
        struct nx_action_write_metadata *nawm;

        nawm = put_NXAST_WRITE_METADATA(out);
        nawm->metadata = metadata->metadata;
        nawm->mask = metadata->mask;
    } else {
        struct ofp11_instruction_write_metadata *oiwm;

        oiwm = instruction_put_OFPIT11_WRITE_METADATA(out);
        oiwm->metadata = metadata->metadata;
        oiwm->metadata_mask = metadata->mask;
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_WRITE_METADATA(char *arg, struct ofpbuf *ofpacts,
                     enum ofputil_protocol *usable_protocols)
{
    struct ofpact_metadata *om;
    char *mask = strchr(arg, '/');

    *usable_protocols &= OFPUTIL_P_NXM_OF11_UP;

    om = ofpact_put_WRITE_METADATA(ofpacts);
    if (mask) {
        char *error;

        *mask = '\0';
        error = str_to_be64(mask + 1, &om->mask);
        if (error) {
            return error;
        }
    } else {
        om->mask = OVS_BE64_MAX;
    }

    return str_to_be64(arg, &om->metadata);
}

static void
format_WRITE_METADATA(const struct ofpact_metadata *a, struct ds *s)
{
    ds_put_format(s, "write_metadata:%#"PRIx64, ntohll(a->metadata));
    if (a->mask != OVS_BE64_MAX) {
        ds_put_format(s, "/%#"PRIx64, ntohll(a->mask));
    }
}

/* Goto-Table instruction. */

static void
encode_GOTO_TABLE(const struct ofpact_goto_table *goto_table,
                  enum ofp_version ofp_version, struct ofpbuf *out)
{
    if (ofp_version == OFP10_VERSION) {
        struct nx_action_resubmit *nar;

        nar = put_NXAST_RESUBMIT_TABLE(out);
        nar->table = goto_table->table_id;
        nar->in_port = htons(ofp_to_u16(OFPP_IN_PORT));
    } else {
        struct ofp11_instruction_goto_table *oigt;

        oigt = instruction_put_OFPIT11_GOTO_TABLE(out);
        oigt->table_id = goto_table->table_id;
        memset(oigt->pad, 0, sizeof oigt->pad);
    }
}

static char * OVS_WARN_UNUSED_RESULT
parse_GOTO_TABLE(char *arg, struct ofpbuf *ofpacts,
                 enum ofputil_protocol *usable_protocols OVS_UNUSED)
{
    struct ofpact_goto_table *ogt = ofpact_put_GOTO_TABLE(ofpacts);
    char *table_s = strsep(&arg, ",");
    if (!table_s || !table_s[0]) {
        return xstrdup("instruction goto-table needs table id");
    }
    return str_to_u8(table_s, "table", &ogt->table_id);
}

static void
format_GOTO_TABLE(const struct ofpact_goto_table *a, struct ds *s)
{
    ds_put_format(s, "goto_table:%"PRIu8, a->table_id);
}

static void
log_bad_action(const struct ofp_action_header *actions, size_t actions_len,
               const struct ofp_action_header *bad_action, enum ofperr error)
{
    if (!VLOG_DROP_WARN(&rl)) {
        struct ds s;

        ds_init(&s);
        ds_put_hex_dump(&s, actions, actions_len, 0, false);
        VLOG_WARN("bad action at offset %#"PRIxPTR" (%s):\n%s",
                  (char *)bad_action - (char *)actions,
                  ofperr_get_name(error), ds_cstr(&s));
        ds_destroy(&s);
    }
}

static enum ofperr
ofpacts_decode(const void *actions, size_t actions_len,
               enum ofp_version ofp_version, struct ofpbuf *ofpacts)
{
    struct ofpbuf openflow;

    ofpbuf_use_const(&openflow, actions, actions_len);
    while (openflow.size) {
        const struct ofp_action_header *action = openflow.data;
        enum ofp_raw_action_type raw;
        enum ofperr error;
        uint64_t arg;

        error = ofpact_pull_raw(&openflow, ofp_version, &raw, &arg);
        if (!error) {
            error = ofpact_decode(action, raw, arg, ofpacts);
        }

        if (error) {
            log_bad_action(actions, actions_len, action, error);
            return error;
        }
    }

    ofpact_pad(ofpacts);
    return 0;
}

static enum ofperr
ofpacts_pull_openflow_actions__(struct ofpbuf *openflow,
                                unsigned int actions_len,
                                enum ofp_version version,
                                uint32_t allowed_ovsinsts,
                                struct ofpbuf *ofpacts)
{
    const struct ofp_action_header *actions;
    enum ofperr error;

    ofpbuf_clear(ofpacts);

    if (actions_len % OFP_ACTION_ALIGN != 0) {
        VLOG_WARN_RL(&rl, "OpenFlow message actions length %u is not a "
                     "multiple of %d", actions_len, OFP_ACTION_ALIGN);
        return OFPERR_OFPBRC_BAD_LEN;
    }

    actions = ofpbuf_try_pull(openflow, actions_len);
    if (actions == NULL) {
        VLOG_WARN_RL(&rl, "OpenFlow message actions length %u exceeds "
                     "remaining message length (%"PRIu32")",
                     actions_len, openflow->size);
        return OFPERR_OFPBRC_BAD_LEN;
    }

    error = ofpacts_decode(actions, actions_len, version, ofpacts);
    if (error) {
        ofpbuf_clear(ofpacts);
        return error;
    }

    error = ofpacts_verify(ofpacts->data, ofpacts->size,
                           allowed_ovsinsts);
    if (error) {
        ofpbuf_clear(ofpacts);
    }
    return error;
}

/* Attempts to convert 'actions_len' bytes of OpenFlow actions from the
 * front of 'openflow' into ofpacts.  On success, replaces any existing content
 * in 'ofpacts' by the converted ofpacts; on failure, clears 'ofpacts'.
 * Returns 0 if successful, otherwise an OpenFlow error.
 *
 * Actions are processed according to their OpenFlow version which
 * is provided in the 'version' parameter.
 *
 * In most places in OpenFlow, actions appear encapsulated in instructions, so
 * you should call ofpacts_pull_openflow_instructions() instead of this
 * function.
 *
 * The parsed actions are valid generically, but they may not be valid in a
 * specific context.  For example, port numbers up to OFPP_MAX are valid
 * generically, but specific datapaths may only support port numbers in a
 * smaller range.  Use ofpacts_check() to additional check whether actions are
 * valid in a specific context. */
enum ofperr
ofpacts_pull_openflow_actions(struct ofpbuf *openflow,
                              unsigned int actions_len,
                              enum ofp_version version,
                              struct ofpbuf *ofpacts)
{
    return ofpacts_pull_openflow_actions__(openflow, actions_len, version,
                                           1u << OVSINST_OFPIT11_APPLY_ACTIONS,
                                           ofpacts);
}

/* OpenFlow 1.1 actions. */


/* True if an action sets the value of a field
 * in a way that is compatibile with the action set.
 * The field can be set via either a set or a move action.
 * False otherwise. */
static bool
ofpact_is_set_or_move_action(const struct ofpact *a)
{
    switch (a->type) {
    case OFPACT_SET_FIELD:
    case OFPACT_REG_MOVE:
    case OFPACT_SET_ETH_DST:
    case OFPACT_SET_ETH_SRC:
    case OFPACT_SET_IP_DSCP:
    case OFPACT_SET_IP_ECN:
    case OFPACT_SET_IP_TTL:
    case OFPACT_SET_IPV4_DST:
    case OFPACT_SET_IPV4_SRC:
    case OFPACT_SET_L4_DST_PORT:
    case OFPACT_SET_L4_SRC_PORT:
    case OFPACT_SET_MPLS_LABEL:
    case OFPACT_SET_MPLS_TC:
    case OFPACT_SET_MPLS_TTL:
    case OFPACT_SET_QUEUE:
    case OFPACT_SET_TUNNEL:
    case OFPACT_SET_VLAN_PCP:
    case OFPACT_SET_VLAN_VID:
        return true;
    case OFPACT_BUNDLE:
    case OFPACT_CLEAR_ACTIONS:
    case OFPACT_CONTROLLER:
    case OFPACT_DEC_MPLS_TTL:
    case OFPACT_DEC_TTL:
    case OFPACT_ENQUEUE:
    case OFPACT_EXIT:
    case OFPACT_UNROLL_XLATE:
    case OFPACT_FIN_TIMEOUT:
    case OFPACT_GOTO_TABLE:
    case OFPACT_GROUP:
    case OFPACT_LEARN:
    case OFPACT_CONJUNCTION:
    case OFPACT_METER:
    case OFPACT_MULTIPATH:
    case OFPACT_NOTE:
    case OFPACT_OUTPUT:
    case OFPACT_OUTPUT_REG:
    case OFPACT_POP_MPLS:
    case OFPACT_POP_QUEUE:
    case OFPACT_PUSH_MPLS:
    case OFPACT_PUSH_VLAN:
    case OFPACT_RESUBMIT:
    case OFPACT_SAMPLE:
    case OFPACT_STACK_POP:
    case OFPACT_STACK_PUSH:
    case OFPACT_STRIP_VLAN:
    case OFPACT_WRITE_ACTIONS:
    case OFPACT_WRITE_METADATA:
        return false;
    default:
        OVS_NOT_REACHED();
    }
}

/* True if an action is allowed in the action set.
 * False otherwise. */
static bool
ofpact_is_allowed_in_actions_set(const struct ofpact *a)
{
    switch (a->type) {
    case OFPACT_DEC_MPLS_TTL:
    case OFPACT_DEC_TTL:
    case OFPACT_GROUP:
    case OFPACT_OUTPUT:
    case OFPACT_POP_MPLS:
    case OFPACT_PUSH_MPLS:
    case OFPACT_PUSH_VLAN:
    case OFPACT_REG_MOVE:
    case OFPACT_SET_FIELD:
    case OFPACT_SET_ETH_DST:
    case OFPACT_SET_ETH_SRC:
    case OFPACT_SET_IP_DSCP:
    case OFPACT_SET_IP_ECN:
    case OFPACT_SET_IP_TTL:
    case OFPACT_SET_IPV4_DST:
    case OFPACT_SET_IPV4_SRC:
    case OFPACT_SET_L4_DST_PORT:
    case OFPACT_SET_L4_SRC_PORT:
    case OFPACT_SET_MPLS_LABEL:
    case OFPACT_SET_MPLS_TC:
    case OFPACT_SET_MPLS_TTL:
    case OFPACT_SET_QUEUE:
    case OFPACT_SET_TUNNEL:
    case OFPACT_SET_VLAN_PCP:
    case OFPACT_SET_VLAN_VID:
    case OFPACT_STRIP_VLAN:
        return true;

    /* In general these actions are excluded because they are not part of
     * the OpenFlow specification nor map to actions that are defined in
     * the specification.  Thus the order in which they should be applied
     * in the action set is undefined. */
    case OFPACT_BUNDLE:
    case OFPACT_CONTROLLER:
    case OFPACT_ENQUEUE:
    case OFPACT_EXIT:
    case OFPACT_UNROLL_XLATE:
    case OFPACT_FIN_TIMEOUT:
    case OFPACT_LEARN:
    case OFPACT_CONJUNCTION:
    case OFPACT_MULTIPATH:
    case OFPACT_NOTE:
    case OFPACT_OUTPUT_REG:
    case OFPACT_POP_QUEUE:
    case OFPACT_RESUBMIT:
    case OFPACT_SAMPLE:
    case OFPACT_STACK_POP:
    case OFPACT_STACK_PUSH:

    /* The action set may only include actions and thus
     * may not include any instructions */
    case OFPACT_CLEAR_ACTIONS:
    case OFPACT_GOTO_TABLE:
    case OFPACT_METER:
    case OFPACT_WRITE_ACTIONS:
    case OFPACT_WRITE_METADATA:
        return false;
    default:
        OVS_NOT_REACHED();
    }
}

/* Append ofpact 'a' onto the tail of 'out' */
static void
ofpact_copy(struct ofpbuf *out, const struct ofpact *a)
{
    ofpbuf_put(out, a, OFPACT_ALIGN(a->len));
}

/* Copies the last ofpact whose type is 'filter' from 'in' to 'out'. */
static bool
ofpacts_copy_last(struct ofpbuf *out, const struct ofpbuf *in,
                  enum ofpact_type filter)
{
    const struct ofpact *target;
    const struct ofpact *a;

    target = NULL;
    OFPACT_FOR_EACH (a, in->data, in->size) {
        if (a->type == filter) {
            target = a;
        }
    }
    if (target) {
        ofpact_copy(out, target);
    }
    return target != NULL;
}

/* Append all ofpacts, for which 'filter' returns true, from 'in' to 'out'.
 * The order of appended ofpacts is preserved between 'in' and 'out' */
static void
ofpacts_copy_all(struct ofpbuf *out, const struct ofpbuf *in,
                 bool (*filter)(const struct ofpact *))
{
    const struct ofpact *a;

    OFPACT_FOR_EACH (a, in->data, in->size) {
        if (filter(a)) {
            ofpact_copy(out, a);
        }
    }
}

/* Reads 'action_set', which contains ofpacts accumulated by
 * OFPACT_WRITE_ACTIONS instructions, and writes equivalent actions to be
 * executed directly into 'action_list'.  (These names correspond to the
 * "Action Set" and "Action List" terms used in OpenFlow 1.1+.)
 *
 * In general this involves appending the last instance of each action that is
 * admissible in the action set in the order described in the OpenFlow
 * specification.
 *
 * Exceptions:
 * + output action is only appended if no group action was present in 'in'.
 * + As a simplification all set actions are copied in the order the are
 *   provided in 'in' as many set actions applied to a field has the same
 *   affect as only applying the last action that sets a field and
 *   duplicates are removed by do_xlate_actions().
 *   This has an unwanted side-effect of compsoting multiple
 *   LOAD_REG actions that touch different regions of the same field. */
void
ofpacts_execute_action_set(struct ofpbuf *action_list,
                           const struct ofpbuf *action_set)
{
    /* The OpenFlow spec "Action Set" section specifies this order. */
    ofpacts_copy_last(action_list, action_set, OFPACT_STRIP_VLAN);
    ofpacts_copy_last(action_list, action_set, OFPACT_POP_MPLS);
    ofpacts_copy_last(action_list, action_set, OFPACT_PUSH_MPLS);
    ofpacts_copy_last(action_list, action_set, OFPACT_PUSH_VLAN);
    ofpacts_copy_last(action_list, action_set, OFPACT_DEC_TTL);
    ofpacts_copy_last(action_list, action_set, OFPACT_DEC_MPLS_TTL);
    ofpacts_copy_all(action_list, action_set, ofpact_is_set_or_move_action);
    ofpacts_copy_last(action_list, action_set, OFPACT_SET_QUEUE);

    /* If both OFPACT_GROUP and OFPACT_OUTPUT are present, OpenFlow says that
     * we should execute only OFPACT_GROUP.
     *
     * If neither OFPACT_GROUP nor OFPACT_OUTPUT is present, then we can drop
     * all the actions because there's no point in modifying a packet that will
     * not be sent anywhere. */
    if (!ofpacts_copy_last(action_list, action_set, OFPACT_GROUP) &&
        !ofpacts_copy_last(action_list, action_set, OFPACT_OUTPUT) &&
        !ofpacts_copy_last(action_list, action_set, OFPACT_RESUBMIT)) {
        ofpbuf_clear(action_list);
    }
}


static enum ofperr
ofpacts_decode_for_action_set(const struct ofp_action_header *in,
                              size_t n_in, enum ofp_version version,
                              struct ofpbuf *out)
{
    enum ofperr error;
    struct ofpact *a;
    size_t start = out->size;

    error = ofpacts_decode(in, n_in, version, out);

    if (error) {
        return error;
    }

    OFPACT_FOR_EACH (a, ofpact_end(out->data, start), out->size - start) {
        if (!ofpact_is_allowed_in_actions_set(a)) {
            VLOG_WARN_RL(&rl, "disallowed action in action set");
            return OFPERR_OFPBAC_BAD_TYPE;
        }
    }

    return 0;
}

/* OpenFlow 1.1 instructions. */

struct instruction_type_info {
    enum ovs_instruction_type type;
    const char *name;
};

static const struct instruction_type_info inst_info[] = {
#define DEFINE_INST(ENUM, STRUCT, EXTENSIBLE, NAME)    {OVSINST_##ENUM, NAME},
OVS_INSTRUCTIONS
#undef DEFINE_INST
};

const char *
ovs_instruction_name_from_type(enum ovs_instruction_type type)
{
    return inst_info[type].name;
}

int
ovs_instruction_type_from_name(const char *name)
{
    const struct instruction_type_info *p;
    for (p = inst_info; p < &inst_info[ARRAY_SIZE(inst_info)]; p++) {
        if (!strcasecmp(name, p->name)) {
            return p->type;
        }
    }
    return -1;
}

enum ovs_instruction_type
ovs_instruction_type_from_ofpact_type(enum ofpact_type type)
{
    switch (type) {
    case OFPACT_METER:
        return OVSINST_OFPIT13_METER;
    case OFPACT_CLEAR_ACTIONS:
        return OVSINST_OFPIT11_CLEAR_ACTIONS;
    case OFPACT_WRITE_ACTIONS:
        return OVSINST_OFPIT11_WRITE_ACTIONS;
    case OFPACT_WRITE_METADATA:
        return OVSINST_OFPIT11_WRITE_METADATA;
    case OFPACT_GOTO_TABLE:
        return OVSINST_OFPIT11_GOTO_TABLE;
    case OFPACT_OUTPUT:
    case OFPACT_GROUP:
    case OFPACT_CONTROLLER:
    case OFPACT_ENQUEUE:
    case OFPACT_OUTPUT_REG:
    case OFPACT_BUNDLE:
    case OFPACT_SET_VLAN_VID:
    case OFPACT_SET_VLAN_PCP:
    case OFPACT_STRIP_VLAN:
    case OFPACT_PUSH_VLAN:
    case OFPACT_SET_ETH_SRC:
    case OFPACT_SET_ETH_DST:
    case OFPACT_SET_IPV4_SRC:
    case OFPACT_SET_IPV4_DST:
    case OFPACT_SET_IP_DSCP:
    case OFPACT_SET_IP_ECN:
    case OFPACT_SET_IP_TTL:
    case OFPACT_SET_L4_SRC_PORT:
    case OFPACT_SET_L4_DST_PORT:
    case OFPACT_REG_MOVE:
    case OFPACT_SET_FIELD:
    case OFPACT_STACK_PUSH:
    case OFPACT_STACK_POP:
    case OFPACT_DEC_TTL:
    case OFPACT_SET_MPLS_LABEL:
    case OFPACT_SET_MPLS_TC:
    case OFPACT_SET_MPLS_TTL:
    case OFPACT_DEC_MPLS_TTL:
    case OFPACT_PUSH_MPLS:
    case OFPACT_POP_MPLS:
    case OFPACT_SET_TUNNEL:
    case OFPACT_SET_QUEUE:
    case OFPACT_POP_QUEUE:
    case OFPACT_FIN_TIMEOUT:
    case OFPACT_RESUBMIT:
    case OFPACT_LEARN:
    case OFPACT_CONJUNCTION:
    case OFPACT_MULTIPATH:
    case OFPACT_NOTE:
    case OFPACT_EXIT:
    case OFPACT_UNROLL_XLATE:
    case OFPACT_SAMPLE:
    default:
        return OVSINST_OFPIT11_APPLY_ACTIONS;
    }
}

enum ofperr
ovs_instruction_type_from_inst_type(enum ovs_instruction_type *instruction_type,
                                    const uint16_t inst_type)
{
    switch (inst_type) {

#define DEFINE_INST(ENUM, STRUCT, EXTENSIBLE, NAME) \
    case ENUM:                                      \
        *instruction_type = OVSINST_##ENUM;         \
        return 0;
OVS_INSTRUCTIONS
#undef DEFINE_INST

    default:
        return OFPERR_OFPBIC_UNKNOWN_INST;
    }
}

/* Two-way translation between OVS's internal "OVSINST_*" representation of
 * instructions and the "OFPIT_*" representation used in OpenFlow. */
struct ovsinst_map {
    enum ovs_instruction_type ovsinst; /* Internal name for instruction. */
    int ofpit;                         /* OFPIT_* number from OpenFlow spec. */
};

static const struct ovsinst_map *
get_ovsinst_map(enum ofp_version version)
{
    /* OpenFlow 1.1 and 1.2 instructions. */
    static const struct ovsinst_map of11[] = {
        { OVSINST_OFPIT11_GOTO_TABLE, 1 },
        { OVSINST_OFPIT11_WRITE_METADATA, 2 },
        { OVSINST_OFPIT11_WRITE_ACTIONS, 3 },
        { OVSINST_OFPIT11_APPLY_ACTIONS, 4 },
        { OVSINST_OFPIT11_CLEAR_ACTIONS, 5 },
        { 0, -1 },
    };

    /* OpenFlow 1.3+ instructions. */
    static const struct ovsinst_map of13[] = {
        { OVSINST_OFPIT11_GOTO_TABLE, 1 },
        { OVSINST_OFPIT11_WRITE_METADATA, 2 },
        { OVSINST_OFPIT11_WRITE_ACTIONS, 3 },
        { OVSINST_OFPIT11_APPLY_ACTIONS, 4 },
        { OVSINST_OFPIT11_CLEAR_ACTIONS, 5 },
        { OVSINST_OFPIT13_METER, 6 },
        { 0, -1 },
    };

    return version < OFP13_VERSION ? of11 : of13;
}

/* Converts 'ovsinst_bitmap', a bitmap whose bits correspond to OVSINST_*
 * values, into a bitmap of instructions suitable for OpenFlow 'version'
 * (OFP11_VERSION or later), and returns the result. */
ovs_be32
ovsinst_bitmap_to_openflow(uint32_t ovsinst_bitmap, enum ofp_version version)
{
    uint32_t ofpit_bitmap = 0;
    const struct ovsinst_map *x;

    for (x = get_ovsinst_map(version); x->ofpit >= 0; x++) {
        if (ovsinst_bitmap & (1u << x->ovsinst)) {
            ofpit_bitmap |= 1u << x->ofpit;
        }
    }
    return htonl(ofpit_bitmap);
}

/* Converts 'ofpit_bitmap', a bitmap of instructions from an OpenFlow message
 * with the given 'version' (OFP11_VERSION or later) into a bitmap whose bits
 * correspond to OVSINST_* values, and returns the result. */
uint32_t
ovsinst_bitmap_from_openflow(ovs_be32 ofpit_bitmap, enum ofp_version version)
{
    uint32_t ovsinst_bitmap = 0;
    const struct ovsinst_map *x;

    for (x = get_ovsinst_map(version); x->ofpit >= 0; x++) {
        if (ofpit_bitmap & htonl(1u << x->ofpit)) {
            ovsinst_bitmap |= 1u << x->ovsinst;
        }
    }
    return ovsinst_bitmap;
}

static inline struct ofp11_instruction *
instruction_next(const struct ofp11_instruction *inst)
{
    return ((struct ofp11_instruction *) (void *)
            ((uint8_t *) inst + ntohs(inst->len)));
}

static inline bool
instruction_is_valid(const struct ofp11_instruction *inst,
                     size_t n_instructions)
{
    uint16_t len = ntohs(inst->len);
    return (!(len % OFP11_INSTRUCTION_ALIGN)
            && len >= sizeof *inst
            && len / sizeof *inst <= n_instructions);
}

/* This macro is careful to check for instructions with bad lengths. */
#define INSTRUCTION_FOR_EACH(ITER, LEFT, INSTRUCTIONS, N_INSTRUCTIONS)  \
    for ((ITER) = (INSTRUCTIONS), (LEFT) = (N_INSTRUCTIONS);            \
         (LEFT) > 0 && instruction_is_valid(ITER, LEFT);                \
         ((LEFT) -= (ntohs((ITER)->len)                                 \
                     / sizeof(struct ofp11_instruction)),               \
          (ITER) = instruction_next(ITER)))

static enum ofperr
decode_openflow11_instruction(const struct ofp11_instruction *inst,
                              enum ovs_instruction_type *type)
{
    uint16_t len = ntohs(inst->len);

    switch (inst->type) {
    case CONSTANT_HTONS(OFPIT11_EXPERIMENTER):
        return OFPERR_OFPBIC_BAD_EXPERIMENTER;

#define DEFINE_INST(ENUM, STRUCT, EXTENSIBLE, NAME)     \
        case CONSTANT_HTONS(ENUM):                      \
            if (EXTENSIBLE                              \
                ? len >= sizeof(struct STRUCT)          \
                : len == sizeof(struct STRUCT)) {       \
                *type = OVSINST_##ENUM;                 \
                return 0;                               \
            } else {                                    \
                return OFPERR_OFPBIC_BAD_LEN;           \
            }
OVS_INSTRUCTIONS
#undef DEFINE_INST

    default:
        return OFPERR_OFPBIC_UNKNOWN_INST;
    }
}

static enum ofperr
decode_openflow11_instructions(const struct ofp11_instruction insts[],
                               size_t n_insts,
                               const struct ofp11_instruction *out[])
{
    const struct ofp11_instruction *inst;
    size_t left;

    memset(out, 0, N_OVS_INSTRUCTIONS * sizeof *out);
    INSTRUCTION_FOR_EACH (inst, left, insts, n_insts) {
        enum ovs_instruction_type type;
        enum ofperr error;

        error = decode_openflow11_instruction(inst, &type);
        if (error) {
            return error;
        }

        if (out[type]) {
            return OFPERR_OFPBIC_DUP_INST;
        }
        out[type] = inst;
    }

    if (left) {
        VLOG_WARN_RL(&rl, "bad instruction format at offset %"PRIuSIZE,
                     (n_insts - left) * sizeof *inst);
        return OFPERR_OFPBIC_BAD_LEN;
    }
    return 0;
}

static void
get_actions_from_instruction(const struct ofp11_instruction *inst,
                             const struct ofp_action_header **actions,
                             size_t *actions_len)
{
    *actions = ALIGNED_CAST(const struct ofp_action_header *, inst + 1);
    *actions_len = ntohs(inst->len) - sizeof *inst;
}

enum ofperr
ofpacts_pull_openflow_instructions(struct ofpbuf *openflow,
                                   unsigned int instructions_len,
                                   enum ofp_version version,
                                   struct ofpbuf *ofpacts)
{
    const struct ofp11_instruction *instructions;
    const struct ofp11_instruction *insts[N_OVS_INSTRUCTIONS];
    enum ofperr error;

    if (version == OFP10_VERSION) {
        return ofpacts_pull_openflow_actions__(openflow, instructions_len,
                                               version,
                                               (1u << N_OVS_INSTRUCTIONS) - 1,
                                               ofpacts);
    }

    ofpbuf_clear(ofpacts);

    if (instructions_len % OFP11_INSTRUCTION_ALIGN != 0) {
        VLOG_WARN_RL(&rl, "OpenFlow message instructions length %u is not a "
                     "multiple of %d",
                     instructions_len, OFP11_INSTRUCTION_ALIGN);
        error = OFPERR_OFPBIC_BAD_LEN;
        goto exit;
    }

    instructions = ofpbuf_try_pull(openflow, instructions_len);
    if (instructions == NULL) {
        VLOG_WARN_RL(&rl, "OpenFlow message instructions length %u exceeds "
                     "remaining message length (%"PRIu32")",
                     instructions_len, openflow->size);
        error = OFPERR_OFPBIC_BAD_LEN;
        goto exit;
    }

    error = decode_openflow11_instructions(
        instructions, instructions_len / OFP11_INSTRUCTION_ALIGN,
        insts);
    if (error) {
        goto exit;
    }

    if (insts[OVSINST_OFPIT13_METER]) {
        const struct ofp13_instruction_meter *oim;
        struct ofpact_meter *om;

        oim = ALIGNED_CAST(const struct ofp13_instruction_meter *,
                           insts[OVSINST_OFPIT13_METER]);

        om = ofpact_put_METER(ofpacts);
        om->meter_id = ntohl(oim->meter_id);
    }
    if (insts[OVSINST_OFPIT11_APPLY_ACTIONS]) {
        const struct ofp_action_header *actions;
        size_t actions_len;

        get_actions_from_instruction(insts[OVSINST_OFPIT11_APPLY_ACTIONS],
                                     &actions, &actions_len);
        error = ofpacts_decode(actions, actions_len, version, ofpacts);
        if (error) {
            goto exit;
        }
    }
    if (insts[OVSINST_OFPIT11_CLEAR_ACTIONS]) {
        instruction_get_OFPIT11_CLEAR_ACTIONS(
            insts[OVSINST_OFPIT11_CLEAR_ACTIONS]);
        ofpact_put_CLEAR_ACTIONS(ofpacts);
    }
    if (insts[OVSINST_OFPIT11_WRITE_ACTIONS]) {
        struct ofpact_nest *on;
        const struct ofp_action_header *actions;
        size_t actions_len;
        size_t start;

        ofpact_pad(ofpacts);
        start = ofpacts->size;
        ofpact_put(ofpacts, OFPACT_WRITE_ACTIONS,
                   offsetof(struct ofpact_nest, actions));
        get_actions_from_instruction(insts[OVSINST_OFPIT11_WRITE_ACTIONS],
                                     &actions, &actions_len);
        error = ofpacts_decode_for_action_set(actions, actions_len,
                                              version, ofpacts);
        if (error) {
            goto exit;
        }
        on = ofpbuf_at_assert(ofpacts, start, sizeof *on);
        on->ofpact.len = ofpacts->size - start;
    }
    if (insts[OVSINST_OFPIT11_WRITE_METADATA]) {
        const struct ofp11_instruction_write_metadata *oiwm;
        struct ofpact_metadata *om;

        oiwm = ALIGNED_CAST(const struct ofp11_instruction_write_metadata *,
                            insts[OVSINST_OFPIT11_WRITE_METADATA]);

        om = ofpact_put_WRITE_METADATA(ofpacts);
        om->metadata = oiwm->metadata;
        om->mask = oiwm->metadata_mask;
    }
    if (insts[OVSINST_OFPIT11_GOTO_TABLE]) {
        const struct ofp11_instruction_goto_table *oigt;
        struct ofpact_goto_table *ogt;

        oigt = instruction_get_OFPIT11_GOTO_TABLE(
            insts[OVSINST_OFPIT11_GOTO_TABLE]);
        ogt = ofpact_put_GOTO_TABLE(ofpacts);
        ogt->table_id = oigt->table_id;
    }

    error = ofpacts_verify(ofpacts->data, ofpacts->size,
                           (1u << N_OVS_INSTRUCTIONS) - 1);
exit:
    if (error) {
        ofpbuf_clear(ofpacts);
    }
    return error;
}

/* Update the length of the instruction that begins at offset 'ofs' within
 * 'openflow' and contains nested actions that extend to the end of 'openflow'.
 * If the instruction contains no nested actions, deletes it entirely. */
static void
ofpacts_update_instruction_actions(struct ofpbuf *openflow, size_t ofs)
{
    struct ofp11_instruction_actions *oia;

    oia = ofpbuf_at_assert(openflow, ofs, sizeof *oia);
    if (openflow->size > ofs + sizeof *oia) {
        oia->len = htons(openflow->size - ofs);
    } else {
        openflow->size = ofs;
    }
}

/* Checks that 'port' is a valid output port for OFPACT_OUTPUT, given that the
 * switch will never have more than 'max_ports' ports.  Returns 0 if 'port' is
 * valid, otherwise an OpenFlow error code. */
enum ofperr
ofpact_check_output_port(ofp_port_t port, ofp_port_t max_ports)
{
    switch (port) {
    case OFPP_IN_PORT:
    case OFPP_TABLE:
    case OFPP_NORMAL:
    case OFPP_FLOOD:
    case OFPP_ALL:
    case OFPP_CONTROLLER:
    case OFPP_NONE:
    case OFPP_LOCAL:
        return 0;

    default:
        if (ofp_to_u16(port) < ofp_to_u16(max_ports)) {
            return 0;
        }
        return OFPERR_OFPBAC_BAD_OUT_PORT;
    }
}

/* Removes the protocols that require consistency between match and actions
 * (that's everything but OpenFlow 1.0) from '*usable_protocols'.
 *
 * (An example of an inconsistency between match and actions is a flow that
 * does not match on an MPLS Ethertype but has an action that pops an MPLS
 * label.) */
static void
inconsistent_match(enum ofputil_protocol *usable_protocols)
{
    *usable_protocols &= OFPUTIL_P_OF10_ANY;
}

/* May modify flow->dl_type, flow->nw_proto and flow->vlan_tci,
 * caller must restore them.
 *
 * Modifies some actions, filling in fields that could not be properly set
 * without context. */
static enum ofperr
ofpact_check__(enum ofputil_protocol *usable_protocols, struct ofpact *a,
               struct flow *flow, ofp_port_t max_ports,
               uint8_t table_id, uint8_t n_tables)
{
    const struct ofpact_enqueue *enqueue;
    const struct mf_field *mf;

    switch (a->type) {
    case OFPACT_OUTPUT:
        return ofpact_check_output_port(ofpact_get_OUTPUT(a)->port,
                                        max_ports);

    case OFPACT_CONTROLLER:
        return 0;

    case OFPACT_ENQUEUE:
        enqueue = ofpact_get_ENQUEUE(a);
        if (ofp_to_u16(enqueue->port) >= ofp_to_u16(max_ports)
            && enqueue->port != OFPP_IN_PORT
            && enqueue->port != OFPP_LOCAL) {
            return OFPERR_OFPBAC_BAD_OUT_PORT;
        }
        return 0;

    case OFPACT_OUTPUT_REG:
        return mf_check_src(&ofpact_get_OUTPUT_REG(a)->src, flow);

    case OFPACT_BUNDLE:
        return bundle_check(ofpact_get_BUNDLE(a), max_ports, flow);

    case OFPACT_SET_VLAN_VID:
        /* Remember if we saw a vlan tag in the flow to aid translating to
         * OpenFlow 1.1+ if need be. */
        ofpact_get_SET_VLAN_VID(a)->flow_has_vlan =
            (flow->vlan_tci & htons(VLAN_CFI)) == htons(VLAN_CFI);
        if (!(flow->vlan_tci & htons(VLAN_CFI)) &&
            !ofpact_get_SET_VLAN_VID(a)->push_vlan_if_needed) {
            inconsistent_match(usable_protocols);
        }
        /* Temporary mark that we have a vlan tag. */
        flow->vlan_tci |= htons(VLAN_CFI);
        return 0;

    case OFPACT_SET_VLAN_PCP:
        /* Remember if we saw a vlan tag in the flow to aid translating to
         * OpenFlow 1.1+ if need be. */
        ofpact_get_SET_VLAN_PCP(a)->flow_has_vlan =
            (flow->vlan_tci & htons(VLAN_CFI)) == htons(VLAN_CFI);
        if (!(flow->vlan_tci & htons(VLAN_CFI)) &&
            !ofpact_get_SET_VLAN_PCP(a)->push_vlan_if_needed) {
            inconsistent_match(usable_protocols);
        }
        /* Temporary mark that we have a vlan tag. */
        flow->vlan_tci |= htons(VLAN_CFI);
        return 0;

    case OFPACT_STRIP_VLAN:
        if (!(flow->vlan_tci & htons(VLAN_CFI))) {
            inconsistent_match(usable_protocols);
        }
        /* Temporary mark that we have no vlan tag. */
        flow->vlan_tci = htons(0);
        return 0;

    case OFPACT_PUSH_VLAN:
        if (flow->vlan_tci & htons(VLAN_CFI)) {
            /* Multiple VLAN headers not supported. */
            return OFPERR_OFPBAC_BAD_TAG;
        }
        /* Temporary mark that we have a vlan tag. */
        flow->vlan_tci |= htons(VLAN_CFI);
        return 0;

    case OFPACT_SET_ETH_SRC:
    case OFPACT_SET_ETH_DST:
        return 0;

    case OFPACT_SET_IPV4_SRC:
    case OFPACT_SET_IPV4_DST:
        if (flow->dl_type != htons(ETH_TYPE_IP)) {
            inconsistent_match(usable_protocols);
        }
        return 0;

    case OFPACT_SET_IP_DSCP:
    case OFPACT_SET_IP_ECN:
    case OFPACT_SET_IP_TTL:
    case OFPACT_DEC_TTL:
        if (!is_ip_any(flow)) {
            inconsistent_match(usable_protocols);
        }
        return 0;

    case OFPACT_SET_L4_SRC_PORT:
    case OFPACT_SET_L4_DST_PORT:
        if (!is_ip_any(flow) || (flow->nw_frag & FLOW_NW_FRAG_LATER) ||
            (flow->nw_proto != IPPROTO_TCP && flow->nw_proto != IPPROTO_UDP
             && flow->nw_proto != IPPROTO_SCTP)) {
            inconsistent_match(usable_protocols);
        }
        /* Note on which transport protocol the port numbers are set.
         * This allows this set action to be converted to an OF1.2 set field
         * action. */
        if (a->type == OFPACT_SET_L4_SRC_PORT) {
            ofpact_get_SET_L4_SRC_PORT(a)->flow_ip_proto = flow->nw_proto;
        } else {
            ofpact_get_SET_L4_DST_PORT(a)->flow_ip_proto = flow->nw_proto;
        }
        return 0;

    case OFPACT_REG_MOVE:
        return nxm_reg_move_check(ofpact_get_REG_MOVE(a), flow);

    case OFPACT_SET_FIELD:
        mf = ofpact_get_SET_FIELD(a)->field;
        /* Require OXM_OF_VLAN_VID to have an existing VLAN header. */
        if (!mf_are_prereqs_ok(mf, flow) ||
            (mf->id == MFF_VLAN_VID && !(flow->vlan_tci & htons(VLAN_CFI)))) {
            VLOG_WARN_RL(&rl, "set_field %s lacks correct prerequisities",
                         mf->name);
            return OFPERR_OFPBAC_MATCH_INCONSISTENT;
        }
        /* Remember if we saw a vlan tag in the flow to aid translating to
         * OpenFlow 1.1 if need be. */
        ofpact_get_SET_FIELD(a)->flow_has_vlan =
            (flow->vlan_tci & htons(VLAN_CFI)) == htons(VLAN_CFI);
        if (mf->id == MFF_VLAN_TCI) {
            /* The set field may add or remove the vlan tag,
             * Mark the status temporarily. */
            flow->vlan_tci = ofpact_get_SET_FIELD(a)->value.be16;
        }
        return 0;

    case OFPACT_STACK_PUSH:
        return nxm_stack_push_check(ofpact_get_STACK_PUSH(a), flow);

    case OFPACT_STACK_POP:
        return nxm_stack_pop_check(ofpact_get_STACK_POP(a), flow);

    case OFPACT_SET_MPLS_LABEL:
    case OFPACT_SET_MPLS_TC:
    case OFPACT_SET_MPLS_TTL:
    case OFPACT_DEC_MPLS_TTL:
        if (!eth_type_mpls(flow->dl_type)) {
            inconsistent_match(usable_protocols);
        }
        return 0;

    case OFPACT_SET_TUNNEL:
    case OFPACT_SET_QUEUE:
    case OFPACT_POP_QUEUE:
    case OFPACT_RESUBMIT:
        return 0;

    case OFPACT_FIN_TIMEOUT:
        if (flow->nw_proto != IPPROTO_TCP) {
            inconsistent_match(usable_protocols);
        }
        return 0;

    case OFPACT_LEARN:
        return learn_check(ofpact_get_LEARN(a), flow);

    case OFPACT_CONJUNCTION:
        return 0;

    case OFPACT_MULTIPATH:
        return multipath_check(ofpact_get_MULTIPATH(a), flow);

    case OFPACT_NOTE:
    case OFPACT_EXIT:
        return 0;

    case OFPACT_PUSH_MPLS:
        flow->dl_type = ofpact_get_PUSH_MPLS(a)->ethertype;
        /* The packet is now MPLS and the MPLS payload is opaque.
         * Thus nothing can be assumed about the network protocol.
         * Temporarily mark that we have no nw_proto. */
        flow->nw_proto = 0;
        return 0;

    case OFPACT_POP_MPLS:
        if (!eth_type_mpls(flow->dl_type)) {
            inconsistent_match(usable_protocols);
        }
        flow->dl_type = ofpact_get_POP_MPLS(a)->ethertype;
        return 0;

    case OFPACT_SAMPLE:
        return 0;

    case OFPACT_CLEAR_ACTIONS:
        return 0;

    case OFPACT_WRITE_ACTIONS: {
        /* Use a temporary copy of 'usable_protocols' because we can't check
         * consistency of an action set. */
        struct ofpact_nest *on = ofpact_get_WRITE_ACTIONS(a);
        enum ofputil_protocol p = *usable_protocols;
        return ofpacts_check(on->actions, ofpact_nest_get_action_len(on),
                             flow, max_ports, table_id, n_tables, &p);
    }

    case OFPACT_WRITE_METADATA:
        return 0;

    case OFPACT_METER: {
        uint32_t mid = ofpact_get_METER(a)->meter_id;
        if (mid == 0 || mid > OFPM13_MAX) {
            return OFPERR_OFPMMFC_INVALID_METER;
        }
        return 0;
    }

    case OFPACT_GOTO_TABLE: {
        uint8_t goto_table = ofpact_get_GOTO_TABLE(a)->table_id;
        if ((table_id != 255 && goto_table <= table_id)
            || (n_tables != 255 && goto_table >= n_tables)) {
            return OFPERR_OFPBIC_BAD_TABLE_ID;
        }
        return 0;
    }

    case OFPACT_GROUP:
        return 0;

    case OFPACT_UNROLL_XLATE:
        /* UNROLL is an internal action that should never be seen via
         * OpenFlow. */
        return OFPERR_OFPBAC_BAD_TYPE;

    default:
        OVS_NOT_REACHED();
    }
}

/* Checks that the 'ofpacts_len' bytes of actions in 'ofpacts' are
 * appropriate for a packet with the prerequisites satisfied by 'flow' in a
 * switch with no more than 'max_ports' ports.
 *
 * If 'ofpacts' and 'flow' are inconsistent with one another, un-sets in
 * '*usable_protocols' the protocols that forbid the inconsistency.  (An
 * example of an inconsistency between match and actions is a flow that does
 * not match on an MPLS Ethertype but has an action that pops an MPLS label.)
 *
 * May annotate ofpacts with information gathered from the 'flow'.
 *
 * May temporarily modify 'flow', but restores the changes before returning. */
enum ofperr
ofpacts_check(struct ofpact ofpacts[], size_t ofpacts_len,
              struct flow *flow, ofp_port_t max_ports,
              uint8_t table_id, uint8_t n_tables,
              enum ofputil_protocol *usable_protocols)
{
    struct ofpact *a;
    ovs_be16 dl_type = flow->dl_type;
    ovs_be16 vlan_tci = flow->vlan_tci;
    uint8_t nw_proto = flow->nw_proto;
    enum ofperr error = 0;

    OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
        error = ofpact_check__(usable_protocols, a, flow,
                               max_ports, table_id, n_tables);
        if (error) {
            break;
        }
    }
    /* Restore fields that may have been modified. */
    flow->dl_type = dl_type;
    flow->vlan_tci = vlan_tci;
    flow->nw_proto = nw_proto;
    return error;
}

/* Like ofpacts_check(), but reports inconsistencies as
 * OFPERR_OFPBAC_MATCH_INCONSISTENT rather than clearing bits. */
enum ofperr
ofpacts_check_consistency(struct ofpact ofpacts[], size_t ofpacts_len,
                          struct flow *flow, ofp_port_t max_ports,
                          uint8_t table_id, uint8_t n_tables,
                          enum ofputil_protocol usable_protocols)
{
    enum ofputil_protocol p = usable_protocols;
    enum ofperr error;

    error = ofpacts_check(ofpacts, ofpacts_len, flow, max_ports,
                          table_id, n_tables, &p);
    return (error ? error
            : p != usable_protocols ? OFPERR_OFPBAC_MATCH_INCONSISTENT
            : 0);
}

/* Verifies that the 'ofpacts_len' bytes of actions in 'ofpacts' are in the
 * appropriate order as defined by the OpenFlow spec and as required by Open
 * vSwitch.
 *
 * 'allowed_ovsinsts' is a bitmap of OVSINST_* values, in which 1-bits indicate
 * instructions that are allowed within 'ofpacts[]'. */
static enum ofperr
ofpacts_verify(const struct ofpact ofpacts[], size_t ofpacts_len,
               uint32_t allowed_ovsinsts)
{
    const struct ofpact *a;
    enum ovs_instruction_type inst;

    inst = OVSINST_OFPIT13_METER;
    OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
        enum ovs_instruction_type next;

        if (a->type == OFPACT_CONJUNCTION) {
            OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
                if (a->type != OFPACT_CONJUNCTION) {
                    VLOG_WARN("when conjunction action is present, it must be "
                              "the only kind of action used (saw '%s' action)",
                              ofpact_name(a->type));
                    return OFPERR_NXBAC_BAD_CONJUNCTION;
                }
            }
            return 0;
        }

        next = ovs_instruction_type_from_ofpact_type(a->type);
        if (a > ofpacts
            && (inst == OVSINST_OFPIT11_APPLY_ACTIONS
                ? next < inst
                : next <= inst)) {
            const char *name = ovs_instruction_name_from_type(inst);
            const char *next_name = ovs_instruction_name_from_type(next);

            if (next == inst) {
                VLOG_WARN("duplicate %s instruction not allowed, for OpenFlow "
                          "1.1+ compatibility", name);
            } else {
                VLOG_WARN("invalid instruction ordering: %s must appear "
                          "before %s, for OpenFlow 1.1+ compatibility",
                          next_name, name);
            }
            return OFPERR_OFPBAC_UNSUPPORTED_ORDER;
        }
        if (!((1u << next) & allowed_ovsinsts)) {
            const char *name = ovs_instruction_name_from_type(next);

            VLOG_WARN("%s instruction not allowed here", name);
            return OFPERR_OFPBIC_UNSUP_INST;
        }

        inst = next;
    }

    return 0;
}

/* Converting ofpacts to OpenFlow. */

static void
encode_ofpact(const struct ofpact *a, enum ofp_version ofp_version,
              struct ofpbuf *out)
{
    switch (a->type) {
#define OFPACT(ENUM, STRUCT, MEMBER, NAME)                              \
        case OFPACT_##ENUM:                                             \
            encode_##ENUM(ofpact_get_##ENUM(a), ofp_version, out);      \
            return;
        OFPACTS
#undef OFPACT
    default:
        OVS_NOT_REACHED();
    }
}

/* Converts the 'ofpacts_len' bytes of ofpacts in 'ofpacts' into OpenFlow
 * actions in 'openflow', appending the actions to any existing data in
 * 'openflow'. */
size_t
ofpacts_put_openflow_actions(const struct ofpact ofpacts[], size_t ofpacts_len,
                             struct ofpbuf *openflow,
                             enum ofp_version ofp_version)
{
    const struct ofpact *a;
    size_t start_size = openflow->size;

    OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
        encode_ofpact(a, ofp_version, openflow);
    }
    return openflow->size - start_size;
}

static enum ovs_instruction_type
ofpact_is_apply_actions(const struct ofpact *a)
{
    return (ovs_instruction_type_from_ofpact_type(a->type)
            == OVSINST_OFPIT11_APPLY_ACTIONS);
}

void
ofpacts_put_openflow_instructions(const struct ofpact ofpacts[],
                                  size_t ofpacts_len,
                                  struct ofpbuf *openflow,
                                  enum ofp_version ofp_version)
{
    const struct ofpact *end = ofpact_end(ofpacts, ofpacts_len);
    const struct ofpact *a;

    if (ofp_version == OFP10_VERSION) {
        ofpacts_put_openflow_actions(ofpacts, ofpacts_len, openflow,
                                     ofp_version);
        return;
    }

    a = ofpacts;
    while (a < end) {
        if (ofpact_is_apply_actions(a)) {
            size_t ofs = openflow->size;

            instruction_put_OFPIT11_APPLY_ACTIONS(openflow);
            do {
                encode_ofpact(a, ofp_version, openflow);
                a = ofpact_next(a);
            } while (a < end && ofpact_is_apply_actions(a));
            ofpacts_update_instruction_actions(openflow, ofs);
        } else {
            encode_ofpact(a, ofp_version, openflow);
            a = ofpact_next(a);
        }
    }
}

/* Sets of supported actions. */

/* Two-way translation between OVS's internal "OFPACT_*" representation of
 * actions and the "OFPAT_*" representation used in some OpenFlow version.
 * (OFPAT_* numbering varies from one OpenFlow version to another, so a given
 * instance is specific to one OpenFlow version.) */
struct ofpact_map {
    enum ofpact_type ofpact;    /* Internal name for action type. */
    int ofpat;                  /* OFPAT_* number from OpenFlow spec. */
};

static const struct ofpact_map *
get_ofpact_map(enum ofp_version version)
{
    /* OpenFlow 1.0 actions. */
    static const struct ofpact_map of10[] = {
        { OFPACT_OUTPUT, 0 },
        { OFPACT_SET_VLAN_VID, 1 },
        { OFPACT_SET_VLAN_PCP, 2 },
        { OFPACT_STRIP_VLAN, 3 },
        { OFPACT_SET_ETH_SRC, 4 },
        { OFPACT_SET_ETH_DST, 5 },
        { OFPACT_SET_IPV4_SRC, 6 },
        { OFPACT_SET_IPV4_DST, 7 },
        { OFPACT_SET_IP_DSCP, 8 },
        { OFPACT_SET_L4_SRC_PORT, 9 },
        { OFPACT_SET_L4_DST_PORT, 10 },
        { OFPACT_ENQUEUE, 11 },
        { 0, -1 },
    };

    /* OpenFlow 1.1 actions. */
    static const struct ofpact_map of11[] = {
        { OFPACT_OUTPUT, 0 },
        { OFPACT_SET_VLAN_VID, 1 },
        { OFPACT_SET_VLAN_PCP, 2 },
        { OFPACT_SET_ETH_SRC, 3 },
        { OFPACT_SET_ETH_DST, 4 },
        { OFPACT_SET_IPV4_SRC, 5 },
        { OFPACT_SET_IPV4_DST, 6 },
        { OFPACT_SET_IP_DSCP, 7 },
        { OFPACT_SET_IP_ECN, 8 },
        { OFPACT_SET_L4_SRC_PORT, 9 },
        { OFPACT_SET_L4_DST_PORT, 10 },
        /* OFPAT_COPY_TTL_OUT (11) not supported. */
        /* OFPAT_COPY_TTL_IN (12) not supported. */
        { OFPACT_SET_MPLS_LABEL, 13 },
        { OFPACT_SET_MPLS_TC, 14 },
        { OFPACT_SET_MPLS_TTL, 15 },
        { OFPACT_DEC_MPLS_TTL, 16 },
        { OFPACT_PUSH_VLAN, 17 },
        { OFPACT_STRIP_VLAN, 18 },
        { OFPACT_PUSH_MPLS, 19 },
        { OFPACT_POP_MPLS, 20 },
        { OFPACT_SET_QUEUE, 21 },
        { OFPACT_GROUP, 22 },
        { OFPACT_SET_IP_TTL, 23 },
        { OFPACT_DEC_TTL, 24 },
        { 0, -1 },
    };

    /* OpenFlow 1.2, 1.3, and 1.4 actions. */
    static const struct ofpact_map of12[] = {
        { OFPACT_OUTPUT, 0 },
        /* OFPAT_COPY_TTL_OUT (11) not supported. */
        /* OFPAT_COPY_TTL_IN (12) not supported. */
        { OFPACT_SET_MPLS_TTL, 15 },
        { OFPACT_DEC_MPLS_TTL, 16 },
        { OFPACT_PUSH_VLAN, 17 },
        { OFPACT_STRIP_VLAN, 18 },
        { OFPACT_PUSH_MPLS, 19 },
        { OFPACT_POP_MPLS, 20 },
        { OFPACT_SET_QUEUE, 21 },
        { OFPACT_GROUP, 22 },
        { OFPACT_SET_IP_TTL, 23 },
        { OFPACT_DEC_TTL, 24 },
        { OFPACT_SET_FIELD, 25 },
        /* OF1.3+ OFPAT_PUSH_PBB (26) not supported. */
        /* OF1.3+ OFPAT_POP_PBB (27) not supported. */
        { 0, -1 },
    };

    switch (version) {
    case OFP10_VERSION:
        return of10;

    case OFP11_VERSION:
        return of11;

    case OFP12_VERSION:
    case OFP13_VERSION:
    case OFP14_VERSION:
    case OFP15_VERSION:
    default:
        return of12;
    }
}

/* Converts 'ofpacts_bitmap', a bitmap whose bits correspond to OFPACT_*
 * values, into a bitmap of actions suitable for OpenFlow 'version', and
 * returns the result. */
ovs_be32
ofpact_bitmap_to_openflow(uint64_t ofpacts_bitmap, enum ofp_version version)
{
    uint32_t openflow_bitmap = 0;
    const struct ofpact_map *x;

    for (x = get_ofpact_map(version); x->ofpat >= 0; x++) {
        if (ofpacts_bitmap & (UINT64_C(1) << x->ofpact)) {
            openflow_bitmap |= 1u << x->ofpat;
        }
    }
    return htonl(openflow_bitmap);
}

/* Converts 'ofpat_bitmap', a bitmap of actions from an OpenFlow message with
 * the given 'version' into a bitmap whose bits correspond to OFPACT_* values,
 * and returns the result. */
uint64_t
ofpact_bitmap_from_openflow(ovs_be32 ofpat_bitmap, enum ofp_version version)
{
    uint64_t ofpact_bitmap = 0;
    const struct ofpact_map *x;

    for (x = get_ofpact_map(version); x->ofpat >= 0; x++) {
        if (ofpat_bitmap & htonl(1u << x->ofpat)) {
            ofpact_bitmap |= UINT64_C(1) << x->ofpact;
        }
    }
    return ofpact_bitmap;
}

/* Appends to 's' a string representation of the set of OFPACT_* represented
 * by 'ofpacts_bitmap'. */
void
ofpact_bitmap_format(uint64_t ofpacts_bitmap, struct ds *s)
{
    if (!ofpacts_bitmap) {
        ds_put_cstr(s, "<none>");
    } else {
        while (ofpacts_bitmap) {
            ds_put_format(s, "%s ",
                          ofpact_name(rightmost_1bit_idx(ofpacts_bitmap)));
            ofpacts_bitmap = zero_rightmost_1bit(ofpacts_bitmap);
        }
        ds_chomp(s, ' ');
    }
}

/* Returns true if 'action' outputs to 'port', false otherwise. */
static bool
ofpact_outputs_to_port(const struct ofpact *ofpact, ofp_port_t port)
{
    switch (ofpact->type) {
    case OFPACT_OUTPUT:
        return ofpact_get_OUTPUT(ofpact)->port == port;
    case OFPACT_ENQUEUE:
        return ofpact_get_ENQUEUE(ofpact)->port == port;
    case OFPACT_CONTROLLER:
        return port == OFPP_CONTROLLER;

    case OFPACT_OUTPUT_REG:
    case OFPACT_BUNDLE:
    case OFPACT_SET_VLAN_VID:
    case OFPACT_SET_VLAN_PCP:
    case OFPACT_STRIP_VLAN:
    case OFPACT_PUSH_VLAN:
    case OFPACT_SET_ETH_SRC:
    case OFPACT_SET_ETH_DST:
    case OFPACT_SET_IPV4_SRC:
    case OFPACT_SET_IPV4_DST:
    case OFPACT_SET_IP_DSCP:
    case OFPACT_SET_IP_ECN:
    case OFPACT_SET_IP_TTL:
    case OFPACT_SET_L4_SRC_PORT:
    case OFPACT_SET_L4_DST_PORT:
    case OFPACT_REG_MOVE:
    case OFPACT_SET_FIELD:
    case OFPACT_STACK_PUSH:
    case OFPACT_STACK_POP:
    case OFPACT_DEC_TTL:
    case OFPACT_SET_MPLS_LABEL:
    case OFPACT_SET_MPLS_TC:
    case OFPACT_SET_MPLS_TTL:
    case OFPACT_DEC_MPLS_TTL:
    case OFPACT_SET_TUNNEL:
    case OFPACT_WRITE_METADATA:
    case OFPACT_SET_QUEUE:
    case OFPACT_POP_QUEUE:
    case OFPACT_FIN_TIMEOUT:
    case OFPACT_RESUBMIT:
    case OFPACT_LEARN:
    case OFPACT_CONJUNCTION:
    case OFPACT_MULTIPATH:
    case OFPACT_NOTE:
    case OFPACT_EXIT:
    case OFPACT_UNROLL_XLATE:
    case OFPACT_PUSH_MPLS:
    case OFPACT_POP_MPLS:
    case OFPACT_SAMPLE:
    case OFPACT_CLEAR_ACTIONS:
    case OFPACT_WRITE_ACTIONS:
    case OFPACT_GOTO_TABLE:
    case OFPACT_METER:
    case OFPACT_GROUP:
    default:
        return false;
    }
}

/* Returns true if any action in the 'ofpacts_len' bytes of 'ofpacts' outputs
 * to 'port', false otherwise. */
bool
ofpacts_output_to_port(const struct ofpact *ofpacts, size_t ofpacts_len,
                       ofp_port_t port)
{
    const struct ofpact *a;

    OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
        if (ofpact_outputs_to_port(a, port)) {
            return true;
        }
    }

    return false;
}

/* Returns true if any action in the 'ofpacts_len' bytes of 'ofpacts' outputs
 * to 'group', false otherwise. */
bool
ofpacts_output_to_group(const struct ofpact *ofpacts, size_t ofpacts_len,
                        uint32_t group_id)
{
    const struct ofpact *a;

    OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
        if (a->type == OFPACT_GROUP
            && ofpact_get_GROUP(a)->group_id == group_id) {
            return true;
        }
    }

    return false;
}

bool
ofpacts_equal(const struct ofpact *a, size_t a_len,
              const struct ofpact *b, size_t b_len)
{
    return a_len == b_len && !memcmp(a, b, a_len);
}

/* Finds the OFPACT_METER action, if any, in the 'ofpacts_len' bytes of
 * 'ofpacts'.  If found, returns its meter ID; if not, returns 0.
 *
 * This function relies on the order of 'ofpacts' being correct (as checked by
 * ofpacts_verify()). */
uint32_t
ofpacts_get_meter(const struct ofpact ofpacts[], size_t ofpacts_len)
{
    const struct ofpact *a;

    OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
        enum ovs_instruction_type inst;

        inst = ovs_instruction_type_from_ofpact_type(a->type);
        if (a->type == OFPACT_METER) {
            return ofpact_get_METER(a)->meter_id;
        } else if (inst > OVSINST_OFPIT13_METER) {
            break;
        }
    }

    return 0;
}

/* Formatting ofpacts. */

static void
ofpact_format(const struct ofpact *a, struct ds *s)
{
    switch (a->type) {
#define OFPACT(ENUM, STRUCT, MEMBER, NAME)                              \
        case OFPACT_##ENUM:                                             \
            format_##ENUM(ALIGNED_CAST(const struct STRUCT *, a), s);   \
            break;
        OFPACTS
#undef OFPACT
    default:
        OVS_NOT_REACHED();
    }
}

/* Appends a string representing the 'ofpacts_len' bytes of ofpacts in
 * 'ofpacts' to 'string'. */
void
ofpacts_format(const struct ofpact *ofpacts, size_t ofpacts_len,
               struct ds *string)
{
    if (!ofpacts_len) {
        ds_put_cstr(string, "drop");
    } else {
        const struct ofpact *a;

        OFPACT_FOR_EACH (a, ofpacts, ofpacts_len) {
            if (a != ofpacts) {
                ds_put_cstr(string, ",");
            }

            /* XXX write-actions */
            ofpact_format(a, string);
        }
    }
}

/* Internal use by helpers. */

void *
ofpact_put(struct ofpbuf *ofpacts, enum ofpact_type type, size_t len)
{
    struct ofpact *ofpact;

    ofpact_pad(ofpacts);
    ofpacts->header = ofpbuf_put_uninit(ofpacts, len);
    ofpact = ofpacts->header;
    ofpact_init(ofpact, type, len);
    return ofpact;
}

void
ofpact_init(struct ofpact *ofpact, enum ofpact_type type, size_t len)
{
    memset(ofpact, 0, len);
    ofpact->type = type;
    ofpact->raw = -1;
    ofpact->len = len;
}

/* Updates 'ofpact->len' to the number of bytes in the tail of 'ofpacts'
 * starting at 'ofpact'.
 *
 * This is the correct way to update a variable-length ofpact's length after
 * adding the variable-length part of the payload.  (See the large comment
 * near the end of ofp-actions.h for more information.) */
void
ofpact_update_len(struct ofpbuf *ofpacts, struct ofpact *ofpact)
{
    ovs_assert(ofpact == ofpacts->header);
    ofpact->len = (char *) ofpbuf_tail(ofpacts) - (char *) ofpact;
}

/* Pads out 'ofpacts' to a multiple of OFPACT_ALIGNTO bytes in length.  Each
 * ofpact_put_<ENUM>() calls this function automatically beforehand, but the
 * client must call this itself after adding the final ofpact to an array of
 * them.
 *
 * (The consequences of failing to call this function are probably not dire.
 * OFPACT_FOR_EACH will calculate a pointer beyond the end of the ofpacts, but
 * not dereference it.  That's undefined behavior, technically, but it will not
 * cause a real problem on common systems.  Still, it seems better to call
 * it.) */
void
ofpact_pad(struct ofpbuf *ofpacts)
{
    unsigned int pad = PAD_SIZE(ofpacts->size, OFPACT_ALIGNTO);
    if (pad) {
        ofpbuf_put_zeros(ofpacts, pad);
    }
}




static char * OVS_WARN_UNUSED_RESULT
ofpact_parse(enum ofpact_type type, char *value, struct ofpbuf *ofpacts,
             enum ofputil_protocol *usable_protocols)
{
    switch (type) {
#define OFPACT(ENUM, STRUCT, MEMBER, NAME)                            \
        case OFPACT_##ENUM:                                             \
            return parse_##ENUM(value, ofpacts, usable_protocols);
        OFPACTS
#undef OFPACT
    default:
        OVS_NOT_REACHED();
    }
}

static bool
ofpact_type_from_name(const char *name, enum ofpact_type *type)
{
#define OFPACT(ENUM, STRUCT, MEMBER, NAME)                            \
    if (!strcasecmp(name, NAME)) {                                    \
        *type = OFPACT_##ENUM;                                          \
        return true;                                                    \
    }
    OFPACTS
#undef OFPACT

    return false;
}

/* Parses 'str' as a series of instructions, and appends them to 'ofpacts'.
 *
 * Returns NULL if successful, otherwise a malloc()'d string describing the
 * error.  The caller is responsible for freeing the returned string. */
static char * OVS_WARN_UNUSED_RESULT
ofpacts_parse__(char *str, struct ofpbuf *ofpacts,
                enum ofputil_protocol *usable_protocols,
                bool allow_instructions)
{
    int prev_inst = -1;
    enum ofperr retval;
    char *key, *value;
    bool drop = false;
    char *pos;

    pos = str;
    while (ofputil_parse_key_value(&pos, &key, &value)) {
        enum ovs_instruction_type inst = OVSINST_OFPIT11_APPLY_ACTIONS;
        enum ofpact_type type;
        char *error = NULL;
        ofp_port_t port;

        if (ofpact_type_from_name(key, &type)) {
            error = ofpact_parse(type, value, ofpacts, usable_protocols);
            inst = ovs_instruction_type_from_ofpact_type(type);
        } else if (!strcasecmp(key, "mod_vlan_vid")) {
            error = parse_set_vlan_vid(value, ofpacts, true);
        } else if (!strcasecmp(key, "mod_vlan_pcp")) {
            error = parse_set_vlan_pcp(value, ofpacts, true);
        } else if (!strcasecmp(key, "set_nw_ttl")) {
            error = parse_SET_IP_TTL(value, ofpacts, usable_protocols);
        } else if (!strcasecmp(key, "pop_vlan")) {
            error = parse_pop_vlan(ofpacts);
        } else if (!strcasecmp(key, "set_tunnel64")) {
            error = parse_set_tunnel(value, ofpacts,
                                     NXAST_RAW_SET_TUNNEL64);
        } else if (!strcasecmp(key, "load")) {
            error = parse_reg_load(value, ofpacts);
        } else if (!strcasecmp(key, "bundle_load")) {
            error = parse_bundle_load(value, ofpacts);
        } else if (!strcasecmp(key, "drop")) {
            drop = true;
        } else if (!strcasecmp(key, "apply_actions")) {
            return xstrdup("apply_actions is the default instruction");
        } else if (ofputil_port_from_string(key, &port)) {
            ofpact_put_OUTPUT(ofpacts)->port = port;
        } else {
            return xasprintf("unknown action %s", key);
        }
        if (error) {
            return error;
        }

        if (inst != OVSINST_OFPIT11_APPLY_ACTIONS) {
            if (!allow_instructions) {
                return xasprintf("only actions are allowed here (not "
                                 "instruction %s)",
                                 ovs_instruction_name_from_type(inst));
            }
            if (inst == prev_inst) {
                return xasprintf("instruction %s may be specified only once",
                                 ovs_instruction_name_from_type(inst));
            }
        }
        if (prev_inst != -1 && inst < prev_inst) {
            return xasprintf("instruction %s must be specified before %s",
                             ovs_instruction_name_from_type(inst),
                             ovs_instruction_name_from_type(prev_inst));
        }
        prev_inst = inst;
    }
    ofpact_pad(ofpacts);

    if (drop && ofpacts->size) {
        return xstrdup("\"drop\" must not be accompanied by any other action "
                       "or instruction");
    }

    retval = ofpacts_verify(ofpacts->data, ofpacts->size,
                            (allow_instructions
                             ? (1u << N_OVS_INSTRUCTIONS) - 1
                             : 1u << OVSINST_OFPIT11_APPLY_ACTIONS));
    if (retval) {
        return xstrdup("Incorrect instruction ordering");
    }

    return NULL;
}

static char * OVS_WARN_UNUSED_RESULT
ofpacts_parse(char *str, struct ofpbuf *ofpacts,
              enum ofputil_protocol *usable_protocols, bool allow_instructions)
{
    uint32_t orig_size = ofpacts->size;
    char *error = ofpacts_parse__(str, ofpacts, usable_protocols,
                                  allow_instructions);
    if (error) {
        ofpacts->size = orig_size;
    }
    return error;
}

static char * OVS_WARN_UNUSED_RESULT
ofpacts_parse_copy(const char *s_, struct ofpbuf *ofpacts,
                   enum ofputil_protocol *usable_protocols,
                   bool allow_instructions)
{
    char *error, *s;

    *usable_protocols = OFPUTIL_P_ANY;

    s = xstrdup(s_);
    error = ofpacts_parse(s, ofpacts, usable_protocols, allow_instructions);
    free(s);

    return error;
}

/* Parses 's' as a set of OpenFlow actions and appends the actions to
 * 'ofpacts'.
 *
 * Returns NULL if successful, otherwise a malloc()'d string describing the
 * error.  The caller is responsible for freeing the returned string. */
char * OVS_WARN_UNUSED_RESULT
ofpacts_parse_actions(const char *s, struct ofpbuf *ofpacts,
                      enum ofputil_protocol *usable_protocols)
{
    return ofpacts_parse_copy(s, ofpacts, usable_protocols, false);
}

/* Parses 's' as a set of OpenFlow instructions and appends the instructions to
 * 'ofpacts'.
 *
 * Returns NULL if successful, otherwise a malloc()'d string describing the
 * error.  The caller is responsible for freeing the returned string. */
char * OVS_WARN_UNUSED_RESULT
ofpacts_parse_instructions(const char *s, struct ofpbuf *ofpacts,
                           enum ofputil_protocol *usable_protocols)
{
    return ofpacts_parse_copy(s, ofpacts, usable_protocols, true);
}

const char *
ofpact_name(enum ofpact_type type)
{
    switch (type) {
#define OFPACT(ENUM, STRUCT, MEMBER, NAME) case OFPACT_##ENUM: return NAME;
        OFPACTS
#undef OFPACT
    }
    return "<unknown>";
}

/* Low-level action decoding and encoding functions. */

/* Everything needed to identify a particular OpenFlow action. */
struct ofpact_hdrs {
    uint32_t vendor;              /* 0 if standard, otherwise a vendor code. */
    uint16_t type;                /* Type if standard, otherwise subtype. */
    uint8_t ofp_version;          /* From ofp_header. */
};

/* Information about a particular OpenFlow action. */
struct ofpact_raw_instance {
    /* The action's identity. */
    struct ofpact_hdrs hdrs;
    enum ofp_raw_action_type raw;

    /* Looking up the action. */
    struct hmap_node decode_node; /* Based on 'hdrs'. */
    struct hmap_node encode_node; /* Based on 'raw' + 'hdrs.ofp_version'. */

    /* The action's encoded size.
     *
     * If this action is fixed-length, 'min_length' == 'max_length'.
     * If it is variable length, then 'max_length' is ROUND_DOWN(UINT16_MAX,
     * OFP_ACTION_ALIGN) == 65528. */
    unsigned short int min_length;
    unsigned short int max_length;

    /* For actions with a simple integer numeric argument, 'arg_ofs' is the
     * offset of that argument from the beginning of the action and 'arg_len'
     * its length, both in bytes.
     *
     * For actions that take other forms, these are both zero. */
    unsigned short int arg_ofs;
    unsigned short int arg_len;

    /* The name of the action, e.g. "OFPAT_OUTPUT" or "NXAST_RESUBMIT". */
    const char *name;

    /* If this action is deprecated, a human-readable string with a brief
     * explanation. */
    const char *deprecation;
};

/* Action header. */
struct ofp_action_header {
    /* The meaning of other values of 'type' generally depends on the OpenFlow
     * version (see enum ofp_raw_action_type).
     *
     * Across all OpenFlow versions, OFPAT_VENDOR indicates that 'vendor'
     * designates an OpenFlow vendor ID and that the remainder of the action
     * structure has a vendor-defined meaning.
     */
#define OFPAT_VENDOR 0xffff
    ovs_be16 type;

    /* Always a multiple of 8. */
    ovs_be16 len;

    /* For type == OFPAT_VENDOR only, this is a vendor ID, e.g. NX_VENDOR_ID or
     * ONF_VENDOR_ID.  Other 'type's use this space for some other purpose. */
    ovs_be32 vendor;
};
OFP_ASSERT(sizeof(struct ofp_action_header) == 8);

/* Header for Nicira-defined actions and for ONF vendor extensions.
 *
 * This cannot be used as an entirely generic vendor extension action header,
 * because OpenFlow does not specify the location or size of the action
 * subtype; it just happens that ONF extensions and Nicira extensions share
 * this format. */
struct ext_action_header {
    ovs_be16 type;                  /* OFPAT_VENDOR. */
    ovs_be16 len;                   /* At least 16. */
    ovs_be32 vendor;                /* NX_VENDOR_ID or ONF_VENDOR_ID. */
    ovs_be16 subtype;               /* See enum ofp_raw_action_type. */
    uint8_t pad[6];
};
OFP_ASSERT(sizeof(struct ext_action_header) == 16);

static bool
ofpact_hdrs_equal(const struct ofpact_hdrs *a,
                  const struct ofpact_hdrs *b)
{
    return (a->vendor == b->vendor
            && a->type == b->type
            && a->ofp_version == b->ofp_version);
}

static uint32_t
ofpact_hdrs_hash(const struct ofpact_hdrs *hdrs)
{
    return hash_2words(hdrs->vendor, (hdrs->type << 16) | hdrs->ofp_version);
}

#include "ofp-actions.inc2"

static struct hmap *
ofpact_decode_hmap(void)
{
    static struct ovsthread_once once = OVSTHREAD_ONCE_INITIALIZER;
    static struct hmap hmap;

    if (ovsthread_once_start(&once)) {
        struct ofpact_raw_instance *inst;

        hmap_init(&hmap);
        for (inst = all_raw_instances;
             inst < &all_raw_instances[ARRAY_SIZE(all_raw_instances)];
             inst++) {
            hmap_insert(&hmap, &inst->decode_node,
                        ofpact_hdrs_hash(&inst->hdrs));
        }
        ovsthread_once_done(&once);
    }
    return &hmap;
}

static struct hmap *
ofpact_encode_hmap(void)
{
    static struct ovsthread_once once = OVSTHREAD_ONCE_INITIALIZER;
    static struct hmap hmap;

    if (ovsthread_once_start(&once)) {
        struct ofpact_raw_instance *inst;

        hmap_init(&hmap);
        for (inst = all_raw_instances;
             inst < &all_raw_instances[ARRAY_SIZE(all_raw_instances)];
             inst++) {
            hmap_insert(&hmap, &inst->encode_node,
                        hash_2words(inst->raw, inst->hdrs.ofp_version));
        }
        ovsthread_once_done(&once);
    }
    return &hmap;
}

static enum ofperr
ofpact_decode_raw(enum ofp_version ofp_version,
                  const struct ofp_action_header *oah, size_t length,
                  const struct ofpact_raw_instance **instp)
{
    const struct ofpact_raw_instance *inst;
    struct ofpact_hdrs hdrs;

    *instp = NULL;
    if (length < sizeof *oah) {
        return OFPERR_OFPBAC_BAD_LEN;
    }

    /* Get base action type. */
    if (oah->type == htons(OFPAT_VENDOR)) {
        /* Get vendor. */
        hdrs.vendor = ntohl(oah->vendor);
        if (hdrs.vendor == NX_VENDOR_ID || hdrs.vendor == ONF_VENDOR_ID) {
            /* Get extension subtype. */
            const struct ext_action_header *nah;

            nah = ALIGNED_CAST(const struct ext_action_header *, oah);
            if (length < sizeof *nah) {
                return OFPERR_OFPBAC_BAD_LEN;
            }
            hdrs.type = ntohs(nah->subtype);
        } else {
            VLOG_WARN_RL(&rl, "OpenFlow action has unknown vendor %#"PRIx32,
                         hdrs.vendor);
            return OFPERR_OFPBAC_BAD_VENDOR;
        }
    } else {
        hdrs.vendor = 0;
        hdrs.type = ntohs(oah->type);
    }

    hdrs.ofp_version = ofp_version;
    HMAP_FOR_EACH_WITH_HASH (inst, decode_node, ofpact_hdrs_hash(&hdrs),
                             ofpact_decode_hmap()) {
        if (ofpact_hdrs_equal(&hdrs, &inst->hdrs)) {
            *instp = inst;
            return 0;
        }
    }

    return (hdrs.vendor
            ? OFPERR_OFPBAC_BAD_VENDOR_TYPE
            : OFPERR_OFPBAC_BAD_TYPE);
}

static enum ofperr
ofpact_pull_raw(struct ofpbuf *buf, enum ofp_version ofp_version,
                enum ofp_raw_action_type *raw, uint64_t *arg)
{
    const struct ofp_action_header *oah = buf->data;
    const struct ofpact_raw_instance *action;
    unsigned int length;
    enum ofperr error;

    *raw = *arg = 0;
    error = ofpact_decode_raw(ofp_version, oah, buf->size, &action);
    if (error) {
        return error;
    }

    if (action->deprecation) {
        VLOG_INFO_RL(&rl, "%s is deprecated in %s (%s)",
                     action->name, ofputil_version_to_string(ofp_version),
                     action->deprecation);
    }

    length = ntohs(oah->len);
    if (length > buf->size) {
        VLOG_WARN_RL(&rl, "OpenFlow action %s length %u exceeds action buffer "
                     "length %"PRIu32, action->name, length, buf->size);
        return OFPERR_OFPBAC_BAD_LEN;
    }
    if (length < action->min_length || length > action->max_length) {
        VLOG_WARN_RL(&rl, "OpenFlow action %s length %u not in valid range "
                     "[%hu,%hu]", action->name, length,
                     action->min_length, action->max_length);
        return OFPERR_OFPBAC_BAD_LEN;
    }
    if (length % 8) {
        VLOG_WARN_RL(&rl, "OpenFlow action %s length %u is not a multiple "
                     "of 8", action->name, length);
        return OFPERR_OFPBAC_BAD_LEN;
    }

    *raw = action->raw;
    *arg = 0;
    if (action->arg_len) {
        const uint8_t *p;
        int i;

        p = ofpbuf_at_assert(buf, action->arg_ofs, action->arg_len);
        for (i = 0; i < action->arg_len; i++) {
            *arg = (*arg << 8) | p[i];
        }
    }

    ofpbuf_pull(buf, length);

    return 0;
}

static const struct ofpact_raw_instance *
ofpact_raw_lookup(enum ofp_version ofp_version, enum ofp_raw_action_type raw)
{
    const struct ofpact_raw_instance *inst;

    HMAP_FOR_EACH_WITH_HASH (inst, encode_node, hash_2words(raw, ofp_version),
                             ofpact_encode_hmap()) {
        if (inst->raw == raw && inst->hdrs.ofp_version == ofp_version) {
            return inst;
        }
    }
    OVS_NOT_REACHED();
}

static void *
ofpact_put_raw(struct ofpbuf *buf, enum ofp_version ofp_version,
               enum ofp_raw_action_type raw, uint64_t arg)
{
    const struct ofpact_raw_instance *inst;
    struct ofp_action_header *oah;
    const struct ofpact_hdrs *hdrs;

    inst = ofpact_raw_lookup(ofp_version, raw);
    hdrs = &inst->hdrs;

    oah = ofpbuf_put_zeros(buf, inst->min_length);
    oah->type = htons(hdrs->vendor ? OFPAT_VENDOR : hdrs->type);
    oah->len = htons(inst->min_length);
    oah->vendor = htonl(hdrs->vendor);

    switch (hdrs->vendor) {
    case 0:
        break;

    case NX_VENDOR_ID:
    case ONF_VENDOR_ID: {
        struct ext_action_header *nah = (struct ext_action_header *) oah;
        nah->subtype = htons(hdrs->type);
        break;
    }

    default:
        OVS_NOT_REACHED();
    }

    if (inst->arg_len) {
        uint8_t *p = (uint8_t *) oah + inst->arg_ofs + inst->arg_len;
        int i;

        for (i = 0; i < inst->arg_len; i++) {
            *--p = arg;
            arg >>= 8;
        }
    } else {
        ovs_assert(!arg);
    }

    return oah;
}

static void
pad_ofpat(struct ofpbuf *openflow, size_t start_ofs)
{
    struct ofp_action_header *oah;

    ofpbuf_put_zeros(openflow, PAD_SIZE(openflow->size - start_ofs, 8));

    oah = ofpbuf_at_assert(openflow, start_ofs, sizeof *oah);
    oah->len = htons(openflow->size - start_ofs);
}

