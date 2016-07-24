/*
 * Copyright (c) 2015 Avaya, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ovs-lldp.h"
#include "ovstest.h"

#define ETH_TYPE_LLDP   0x88cc

/* Dummy MAC addresses */
static uint8_t chassis_mac[ETH_ADDR_LEN] = { 0x5e, 0x10, 0x8e, 0xe7, 0x84, 0xad };
static uint8_t eth_src[ETH_ADDR_LEN] = { 0x5e, 0x10, 0x8e, 0xe7, 0x84, 0xad };

/* LLDP multicast address */
static const uint8_t eth_addr_lldp[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0e};

/* Count of tests run */
static int num_tests = 0;


/*
 * Helper function to validate port info
 */
static void
check_received_port(struct lldpd_port *sport,
                    struct lldpd_port *rport)
{
    assert(rport->p_id_subtype == sport->p_id_subtype);
    assert(rport->p_id_len == sport->p_id_len);
    assert(strncmp(rport->p_id, sport->p_id, sport->p_id_len) == 0);
    assert(strcmp(rport->p_descr, sport->p_descr) == 0);

    return;
}


/*
 * Helper function to validate chassis info
 */
static void
check_received_chassis(struct lldpd_chassis *schassis,
                       struct lldpd_chassis *rchassis)
{
    assert(rchassis->c_id_subtype == schassis->c_id_subtype);
    assert(rchassis->c_id_len == schassis->c_id_len);
    assert(memcmp(rchassis->c_id, schassis->c_id, schassis->c_id_len) == 0);
    assert(strcmp(rchassis->c_name, schassis->c_name) == 0);
    assert(strcmp(rchassis->c_descr, schassis->c_descr) == 0);
    assert(rchassis->c_cap_available == schassis->c_cap_available);
    assert(rchassis->c_cap_enabled == schassis->c_cap_enabled);

    return;
}


/*
 * Helper function to validate auto-attach info
 */
static void
check_received_aa(struct lldpd_port *sport,
                  struct lldpd_port *rport,
                  struct lldpd_aa_isid_vlan_maps_tlv *smap)
{
    struct lldpd_aa_isid_vlan_maps_tlv *received_map;
    int i = 0;

    assert(rport->p_element.type == sport->p_element.type);
    assert(rport->p_element.mgmt_vlan == sport->p_element.mgmt_vlan);
    assert(rport->p_element.system_id.system_mac[0] ==
           sport->p_element.system_id.system_mac[0]);
    assert(rport->p_element.system_id.system_mac[1] ==
           sport->p_element.system_id.system_mac[1]);
    assert(rport->p_element.system_id.system_mac[2] ==
           sport->p_element.system_id.system_mac[2]);
    assert(rport->p_element.system_id.system_mac[3] ==
           sport->p_element.system_id.system_mac[3]);
    assert(rport->p_element.system_id.system_mac[4] ==
           sport->p_element.system_id.system_mac[4]);
    assert(rport->p_element.system_id.system_mac[5] ==
           sport->p_element.system_id.system_mac[5]);
    assert(rport->p_element.system_id.conn_type ==
           sport->p_element.system_id.conn_type);
    assert(rport->p_element.system_id.rsvd ==
           sport->p_element.system_id.rsvd);
    assert(rport->p_element.system_id.rsvd2[0] ==
           sport->p_element.system_id.rsvd2[0]);
    assert(rport->p_element.system_id.rsvd2[1] ==
           sport->p_element.system_id.rsvd2[1]);

    /* Should receive 2 mappings */
    assert(!list_is_empty(&rport->p_isid_vlan_maps));

    /* For each received isid/vlan mapping */
    LIST_FOR_EACH (received_map, m_entries, &rport->p_isid_vlan_maps) {

        /* Validate against mapping sent */
        assert(smap[i].isid_vlan_data.status ==
               received_map->isid_vlan_data.status);
        assert(smap[i].isid_vlan_data.vlan ==
               received_map->isid_vlan_data.vlan);
        assert(smap[i].isid_vlan_data.isid ==
               received_map->isid_vlan_data.isid);

        /* Next mapping sent */
        i++;
    }
    assert(i == 2);

    return;
}


/*
 * Validate basic send/receive processing
 */
static int
test_aa_send(void)
{
    struct lldp           *lldp;
    struct lldpd_hardware hardware;
    struct lldpd_chassis  chassis;

    struct lldpd_chassis *nchassis = NULL;
    struct lldpd_port    *nport = NULL;

    struct lldpd_hardware *hw = NULL;
    struct lldpd_chassis  *ch = NULL;

    struct lldpd_aa_isid_vlan_maps_tlv map_init[2];
    struct lldpd_aa_isid_vlan_maps_tlv map[2];

    uint32_t      stub[512 / 4];
    struct dp_packet packet;

    int n;

    /* Prepare data used to construct and validate LLDPPDU */
    hardware.h_lport.p_id_subtype = LLDP_PORTID_SUBTYPE_IFNAME;
    hardware.h_lport.p_id = "FastEthernet 1/5";
    hardware.h_lport.p_id_len = strlen(hardware.h_lport.p_id);
    hardware.h_lport.p_descr = "Fake port description";
    hardware.h_lport.p_mfs = 1516;

    /* Auto attach element discovery info */
    hardware.h_lport.p_element.type =
        LLDP_TLV_AA_ELEM_TYPE_CLIENT_VIRTUAL_SWITCH;
    hardware.h_lport.p_element.mgmt_vlan = 0xCDC;
    hardware.h_lport.p_element.system_id.system_mac[0] = 0x1;
    hardware.h_lport.p_element.system_id.system_mac[1] = 0x2;
    hardware.h_lport.p_element.system_id.system_mac[2] = 0x3;
    hardware.h_lport.p_element.system_id.system_mac[3] = 0x4;
    hardware.h_lport.p_element.system_id.system_mac[4] = 0x5;
    hardware.h_lport.p_element.system_id.system_mac[5] = 0x6;

    hardware.h_lport.p_element.system_id.conn_type = 0x5;
    hardware.h_lport.p_element.system_id.rsvd = 0x3CC;
    hardware.h_lport.p_element.system_id.rsvd2[0] = 0xB;
    hardware.h_lport.p_element.system_id.rsvd2[1] = 0xE;

    /* Local chassis info */
    chassis.c_id_subtype = LLDP_CHASSISID_SUBTYPE_LLADDR;
    chassis.c_id = chassis_mac;
    chassis.c_id_len = ETH_ADDR_LEN;
    chassis.c_name = "Dummy chassis";
    chassis.c_descr = "Long dummy chassis description";
    chassis.c_cap_available = LLDP_CAP_BRIDGE;
    chassis.c_cap_enabled = LLDP_CAP_BRIDGE;

    /* ISID/VLAN mappings */
    map_init[0].isid_vlan_data.status  = 0xC;
    map_init[0].isid_vlan_data.vlan    = 0x64;
    map_init[0].isid_vlan_data.isid    = 0x010203;

    map_init[1].isid_vlan_data.status  = 0xD;
    map_init[1].isid_vlan_data.vlan    = 0xF;
    map_init[1].isid_vlan_data.isid    = 0x040506;

    /* Prepare an empty packet buffer */
    dp_packet_use_stub(&packet, stub, sizeof stub);
    dp_packet_clear(&packet);

    /* Create a dummy lldp instance */
    lldp = lldp_create_dummy();
    if ((lldp == NULL) ||
        (lldp->lldpd == NULL) ||
        list_is_empty(&lldp->lldpd->g_hardware)) {
        printf("Error: unable to create dummy lldp instance");
        return 1;
    }

    /* Populate instance with local chassis info */
    hw = lldpd_first_hardware(lldp->lldpd);
    ch = hw->h_lport.p_chassis;
    ch->c_id_subtype = chassis.c_id_subtype;
    ch->c_id = chassis.c_id;
    ch->c_id_len = chassis.c_id_len;
    ch->c_name = chassis.c_name;
    ch->c_descr = chassis.c_descr;
    ch->c_cap_available = chassis.c_cap_available;
    ch->c_cap_enabled = chassis.c_cap_enabled;

    /* Populate instance with local port info */
    hw->h_lport.p_id_subtype = hardware.h_lport.p_id_subtype;
    hw->h_lport.p_id = hardware.h_lport.p_id;
    hw->h_lport.p_id_len = strlen(hw->h_lport.p_id);
    hw->h_lport.p_descr = hardware.h_lport.p_descr;
    hw->h_lport.p_mfs = hardware.h_lport.p_mfs;

    /* Populate instance with auto attach element discovery info */

    hw->h_lport.p_element.type = hardware.h_lport.p_element.type;
    hw->h_lport.p_element.mgmt_vlan = hardware.h_lport.p_element.mgmt_vlan;
    hw->h_lport.p_element.system_id.system_mac[0] =
        hardware.h_lport.p_element.system_id.system_mac[0];
    hw->h_lport.p_element.system_id.system_mac[1] =
        hardware.h_lport.p_element.system_id.system_mac[1];
    hw->h_lport.p_element.system_id.system_mac[2] =
        hardware.h_lport.p_element.system_id.system_mac[2];
    hw->h_lport.p_element.system_id.system_mac[3] =
        hardware.h_lport.p_element.system_id.system_mac[3];
    hw->h_lport.p_element.system_id.system_mac[4] =
        hardware.h_lport.p_element.system_id.system_mac[4];
    hw->h_lport.p_element.system_id.system_mac[5] =
        hardware.h_lport.p_element.system_id.system_mac[5];

    hw->h_lport.p_element.system_id.conn_type =
        hardware.h_lport.p_element.system_id.conn_type;
    hw->h_lport.p_element.system_id.rsvd =
        hardware.h_lport.p_element.system_id.rsvd;
    hw->h_lport.p_element.system_id.rsvd2[0] =
        hardware.h_lport.p_element.system_id.rsvd2[0];
    hw->h_lport.p_element.system_id.rsvd2[1] =
        hardware.h_lport.p_element.system_id.rsvd2[1];

    /* Populate instance with two auto attach isid/vlan mappings */
    map[0].isid_vlan_data.status  = map_init[0].isid_vlan_data.status;
    map[0].isid_vlan_data.vlan    = map_init[0].isid_vlan_data.vlan;
    map[0].isid_vlan_data.isid    = map_init[0].isid_vlan_data.isid;

    map[1].isid_vlan_data.status  = map_init[1].isid_vlan_data.status;
    map[1].isid_vlan_data.vlan    = map_init[1].isid_vlan_data.vlan;
    map[1].isid_vlan_data.isid    = map_init[1].isid_vlan_data.isid;

    list_init(&hw->h_lport.p_isid_vlan_maps);
    list_push_back(&hw->h_lport.p_isid_vlan_maps, &map[0].m_entries);
    list_push_back(&hw->h_lport.p_isid_vlan_maps, &map[1].m_entries);

    /* Construct LLDPPDU (including Ethernet header) */
    eth_compose(&packet, eth_addr_lldp, eth_src, ETH_TYPE_LLDP, 0);
    n = lldp_send(lldp->lldpd, hw, &packet);

    if (n == 0) {
        printf("Error: unable to build packet\n");
        return 1;
    }

    /* Decode the constructed LLDPPDU */
    assert(lldp_decode(NULL, dp_packet_data(&packet), dp_packet_size(&packet), hw,
                       &nchassis, &nport) != -1);

    /* Expecting returned pointers to allocated structures */
    if (!nchassis || !nport) {
        printf("Error: unable to decode packet");
        return 1;
    }

    /* Verify chassis values */
    check_received_chassis(&chassis, nchassis);

    /* Verify port values */
    check_received_port(&hardware.h_lport, nport);

    /* Verify auto attach values */
    check_received_aa(&hardware.h_lport, nport, map_init);

    return 0;
}


static void
test_aa_main(int argc OVS_UNUSED, char *argv[] OVS_UNUSED)
{
    int num_errors = 0;

    /* Make sure we emit valid auto-attach LLDPPDUs */
    num_tests++;
    num_errors += test_aa_send();

    /* Add more tests here */

    printf("executed %d tests, %d errors\n", num_tests, num_errors);

    exit(num_errors != 0);
}

OVSTEST_REGISTER("test-aa", test_aa_main);
