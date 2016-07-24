/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 *  * @file print-gtpv2.c
 *   * @author Shaji Radhakrishnan (shaji.radhakrishnan@riftio.com)
 *    * @date 12/10/2014
 *     * @brief print-gtpv2.c
 *      *
 * Decoder for the GPRS V2 protocol
 *
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include "ip_var.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "addrtoname.h"
#define HAVE_STRDUP 1
#define HAVE_STRSEP 1
#include "interface.h"
#include "gtp.h"

#include <endian.h>
#define ntohll(n) ( (((uint64_t)ntohl((uint32_t)n)) << 32) + ntohl((uint32_t)(n >> 32)) )

#include "tcpdump_export.h"
/* GTPv2 stuff */
#define GTPV2_HDR_TEID    (1<<3)
#define GTPV2_HDR_PIGGY   (1<<4)
//#define GTPV2_C_PORT      2123

#define GTPv2_ULI_CGI_MASK          0x01
#define GTPv2_ULI_SAI_MASK          0x02
#define GTPv2_ULI_RAI_MASK          0x04
#define GTPv2_ULI_TAI_MASK          0x08
#define GTPv2_ULI_ECGI_MASK         0x10
#define GTPv2_ULI_LAI_MASK          0x20

#define GTPV2_CREATE_SESSION_REQUEST     32
#define GTPV2_CREATE_SESSION_RESPONSE    33
#define GTPV2_CONTEXT_RESPONSE          131
#define GTPV2_FORWARD_RELOCATION_REQ    133
#define GTPV2_FORWARD_CTX_NOTIFICATION  137
#define GTPV2_RAN_INFORMATION_RELAY     152

#define GTPV2_IE_RESERVED                 0
#define GTPV2_IE_IMSI                     1
#define GTPV2_IE_CAUSE                    2
#define GTPV2_REC_REST_CNT                3
#define GTPV2_IE_STN_SR                  51
#define GTPV2_IE_SRC_TGT_TRANS_CON       52
#define GTPV2_IE_TGT_SRC_TRANS_CON       53
#define GTPV2_IE_MM_CON_EUTRAN_SRVCC     54
#define GTPV2_IE_MM_CON_UTRAN_SRVCC      55
#define GTPV2_IE_SRVCC_CAUSE             56
#define GTPV2_IE_TGT_RNC_ID              57
#define GTPV2_IE_TGT_GLOGAL_CELL_ID      58
#define GTPV2_IE_TEID_C                  59
#define GTPV2_IE_SV_FLAGS                60
#define GTPV2_IE_SAI                     61
/* 61 - 70 for future sv interface use*/
#define GTPV2_APN                        71
#define GTPV2_AMBR                       72
#define GTPV2_EBI                        73
#define GTPV2_IP_ADDRESS                 74
#define GTPV2_MEI                        75
#define GTPV2_IE_MSISDN                  76
#define GTPV2_INDICATION                 77
#define GTPV2_PCO                        78
#define GTPV2_PAA                        79
#define GTPV2_BEARER_QOS                 80
#define GTPV2_IE_FLOW_QOS                81
#define GTPV2_IE_RAT_TYPE                82
#define GTPV2_IE_SERV_NET                83
#define GTPV2_IE_BEARER_TFT              84
#define GTPV2_IE_TAD                     85
#define GTPV2_IE_ULI                     86
#define GTPV2_IE_F_TEID                  87
#define GTPV2_IE_TMSI                    88
#define GTPV2_IE_GLOBAL_CNID             89
#define GTPV2_IE_S103PDF                 90
#define GTPV2_IE_S1UDF                   91
#define GTPV2_IE_DEL_VAL                 92
#define GTPV2_IE_BEARER_CTX              93
#define GTPV2_IE_CHAR_ID                 94
#define GTPV2_IE_CHAR_CHAR               95
#define GTPV2_IE_TRA_INFO                96
#define GTPV2_BEARER_FLAG                97
/* define GTPV2_IE_PAGING_CAUSE          98 (void) */
#define GTPV2_IE_PDN_TYPE                99
#define GTPV2_IE_PTI                    100
#define GTPV2_IE_DRX_PARAM              101
#define GTPV2_IE_UE_NET_CAPABILITY      102
#define GTPV2_IE_MM_CONTEXT_GSM_T       103
#define GTPV2_IE_MM_CONTEXT_UTMS_CQ     104
#define GTPV2_IE_MM_CONTEXT_GSM_CQ      105
#define GTPV2_IE_MM_CONTEXT_UTMS_Q      106
#define GTPV2_IE_MM_CONTEXT_EPS_QQ      107
#define GTPV2_IE_MM_CONTEXT_UTMS_QQ     108
#define GTPV2_IE_PDN_CONNECTION         109
#define GTPV2_IE_PDN_NUMBERS            110
#define GTPV2_IE_P_TMSI                 111
#define GTPV2_IE_P_TMSI_SIG             112
#define GTPV2_IE_HOP_COUNTER            113
#define GTPV2_IE_UE_TIME_ZONE           114
#define GTPV2_IE_TRACE_REFERENCE        115
#define GTPV2_IE_COMPLETE_REQUEST_MSG   116
#define GTPV2_IE_GUTI                   117
#define GTPV2_IE_F_CONTAINER            118
#define GTPV2_IE_F_CAUSE                119
#define GTPV2_IE_SEL_PLMN_ID            120
#define GTPV2_IE_TARGET_ID              121
/* GTPV2_IE_NSAPI                       122 */
#define GTPV2_IE_PKT_FLOW_ID            123
#define GTPV2_IE_RAB_CONTEXT            124
#define GTPV2_IE_S_RNC_PDCP_CTX_INFO    125
#define GTPV2_IE_UDP_S_PORT_NR          126
#define GTPV2_IE_APN_RESTRICTION        127
#define GTPV2_IE_SEL_MODE               128
#define GTPV2_IE_SOURCE_IDENT           129
#define GTPV2_IE_BEARER_CONTROL_MODE    130
#define GTPV2_IE_CNG_REP_ACT            131
#define GTPV2_IE_FQ_CSID                132
#define GTPV2_IE_CHANNEL_NEEDED         133
#define GTPV2_IE_EMLPP_PRI              134
#define GTPV2_IE_NODE_TYPE              135
#define GTPV2_IE_FQDN                   136
#define GTPV2_IE_TI                     137
#define GTPV2_IE_MBMS_SESSION_DURATION  138
#define GTPV2_IE_MBMS_SERVICE_AREA      139
#define GTPV2_IE_MBMS_SESSION_ID        140
#define GTPV2_IE_MBMS_FLOW_ID           141
#define GTPV2_IE_MBMS_IP_MC_DIST        142
#define GTPV2_IE_MBMS_DIST_ACK          143
#define GTPV2_IE_RFSP_INDEX             144
#define GTPV2_IE_UCI                    145
#define GTPV2_IE_CSG_INFO_REP_ACTION    146
#define GTPV2_IE_CSG_ID                 147
#define GTPV2_IE_CMI                    148
#define GTPV2_IE_SERVICE_INDICATOR      149
#define GTPV2_IE_DETACH_TYPE            150
#define GTPV2_IE_LDN                    151
#define GTPV2_IE_NODE_FEATURES          152
#define GTPV2_IE_MBMS_TIME_TO_DATA_XFER 153
#define GTPV2_IE_THROTTLING             154
#define GTPV2_IE_ARP                    155
#define GTPV2_IE_EPC_TIMER              156
#define GTPV2_IE_SIG_PRIO_IND           157
#define GTPV2_IE_TMGI                   158
#define GTPV2_IE_ADD_MM_CONT_FOR_SRVCC  159
#define GTPV2_IE_ADD_FLAGS_FOR_SRVCC    160
#define GTPV2_IE_MMBR                   161
#define GTPV2_IE_MDT_CONFIG             162
#define GTPV2_IE_APCO                   163
#define GTPV2_IE_ABS_MBMS_DATA_TF_TIME  164
#define GTPV2_IE_HENB_INFO_REPORT       165
#define GTPV2_IE_IP4CP                  166
#define GTPV2_IE_CHANGE_TO_REPORT_FLAGS 167
#define GTPV2_IE_ACTION_INDICATION      168
#define GTPV2_IE_TWAN_IDENTIFIER        169
#define GTPV2_IE_ULI_TIMESTAMP          170
#define GTPV2_IE_MBMS_FLAGS             171
#define GTPV2_IE_RAN_NAS_CAUSE          172
#define GTPV2_IE_CN_OP_SEL_ENT          173
#define GTPV2_IE_TRUST_WLAN_MODE_IND    174
#define GTPV2_IE_NODE_NUMBER            175
#define GTPV2_IE_NODE_IDENTIFIER        176
#define GTPV2_IE_PRES_REP_AREA_ACT  177
#define GTPV2_IE_PRES_REP_AREA_INF  178
#define GTPV2_IE_TWAN_ID_TS             179
#define GTPV2_IE_OVERLOAD_CONTROL_INF   180
#define GTPV2_IE_LOAD_CONTROL_INF       181
#define GTPV2_IE_METRIC                 182
#define GTPV2_IE_SEQ_NO                 183
#define GTPV2_IE_APN_AND_REL_CAP        184

/* 169 to 254 reserved for future use */
#define GTPV2_IE_PRIVATE_EXT            255

#define SPARE                               0X0
#define CREATE_NEW_TFT                      0X20
#define DELETE_TFT                          0X40
#define ADD_PACKET_FILTERS_TFT              0X60
#define REPLACE_PACKET_FILTERS_TFT          0X80
#define DELETE_PACKET_FILTERS_TFT           0XA0
#define NO_TFT_OPERATION                    0XC0
#define RESERVED                            0XE0

#define GTPV2_PCO_PROT_LCP  0xC021
#define GTPV2_PCO_PROT_PAP  0xC023
#define GTPV2_PCO_PROT_CHAP 0xC223
#define GTPV2_PCO_PROT_IPCP 0x8021
#define GTPV2_ULI_CGI  0x01
#define GTPV2_ULI_SAI  0x02
#define GTPV2_ULI_RAI  0x04
#define GTPV2_ULI_TAI  0x08
#define GTPV2_ULI_ECGI 0x10
#define GTPV2_ULI_LAI  0x20

#define GTPV2_IE_F_TEID_V4     0x80
#define GTPV2_IE_F_TEID_V6     0x40
#define GTPV2_IE_F_TEID_IFMASK 0x3F

typedef struct gtp_v2_hdr {
  u_int8_t flags;
  u_int8_t msgtype;
  u_int16_t length;
} gtp_v2_hdr_t;


void gtp_v2_print(const u_char *cp, u_int length, u_short sport, u_short dport);
static int gtp_v2_print_tlv(register const u_char *cp, u_int value, int length);
static void gtp_v2_decode_ie(register const u_char *cp, int len);
static void gtp_v2_print_cause(register const u_char *cp, u_int len);
static void gtp_v2_print_tbcd(register const u_char *, u_int);
static void gtp_v2_print_apn(register const u_char *, u_int);
static int gtp_v2_decode_ie_brctx(register const u_char *cp, int length);

/* gtp v2 stuff */
static int gtp_proto = -1;

static struct tok gtp_v2_msgtype[] = {
    {  0, "Reserved"},
    {  1, "Echo Request"},
    {  2, "Echo Response"},
    {  3, "Version Not Supported Indication"},
    /* 4-24 Reserved for S101 interface TS 29.276 */
    {  4, "Node Alive Request"},
    {  5, "Node Alive Response"},
    {  6, "Redirection Request"},
    {  7, "Redirection Response"},
    /* 25-31 Reserved for SV interface TS 29.280 */
    { 25, "SRVCC PS to CS Request"},
    { 26, "SRVCC PS to CS Response"},
    { 27, "SRVCC PS to CS Complete Notification"},
    { 28, "SRVCC PS to CS Complete Acknowledge"},
    { 29, "SRVCC PS to CS Cancel Notification"},
    { 30, "SRVCC PS to CS Cancel Acknowledge"},
    { 31, "For Future Sv interface use"},
    { 32, "Create Session Request"},
    { 33, "Create Session Response"},
    { 34, "Modify Bearer Request"},
    { 35, "Modify Bearer Response"},
    { 36, "Delete Session Request"},
    { 37, "Delete Session Response"},
    { 38, "Change Notification Request"},
    { 39, "Change Notification Response"},
    /* 40-63 For future use */
    { 64, "Modify Bearer Command"},
    { 65, "Modify Bearer Failure Indication"},
    { 66, "Delete Bearer Command"},
    { 67, "Delete Bearer Failure Indication"},
    { 68, "Bearer Resource Command"},
    { 69, "Bearer Resource Failure Indication"},
    { 70, "Downlink Data Notification Failure Indication"},
    { 71, "Trace Session Activation"},
    { 72, "Trace Session Deactivation"},
    { 73, "Stop Paging Indication"},
    /* 74-94 For future use */
    { 95, "Create Bearer Request"},
    { 96, "Create Bearer Response"},
    { 97, "Update Bearer Request"},
    { 98, "Update Bearer Response"},
    { 99, "Delete Bearer Request"},
    {100, "Delete Bearer Response"},
    {101, "Delete PDN Connection Set Request"},
    {102, "Delete PDN Connection Set Response"},
    /* 103-127 For future use */
    {128, "Identification Request"},
    {129, "Identification Response"},
    {130, "Context Request"},
    {131, "Context Response"},
    {132, "Context Acknowledge"},
    {133, "Forward Relocation Request"},
    {134, "Forward Relocation Response"},
    {135, "Forward Relocation Complete Notification"},
    {136, "Forward Relocation Complete Acknowledge"},
    {137, "Forward Access Context Notification"},
    {138, "Forward Access Context Acknowledge"},
    {139, "Relocation Cancel Request"},
    {140, "Relocation Cancel Response"},
    {141, "Configuration Transfer Tunnel"},
    /* 142-148 For future use */
    {149, "Detach Notification"},
    {150, "Detach Acknowledge"},
    {151, "CS Paging Indication"},
    {152, "RAN Information Relay"},
    {153, "Alert MME Notification"},
    {154, "Alert MME Acknowledge"},
    {155, "UE Activity Notification"},
    {156, "UE Activity Acknowledge"},
    /* 157 to 159 For future use */
    {160, "Create Forwarding Tunnel Request"},
    {161, "Create Forwarding Tunnel Response"},
    {162, "Suspend Notification"},
    {163, "Suspend Acknowledge"},
    {164, "Resume Notification"},
    {165, "Resume Acknowledge"},
    {166, "Create Indirect Data Forwarding Tunnel Request"},
    {167, "Create Indirect Data Forwarding Tunnel Response"},
    {168, "Delete Indirect Data Forwarding Tunnel Request"},
    {169, "Delete Indirect Data Forwarding Tunnel Response"},
    {170, "Release Access Bearers Request"},
    {171, "Release Access Bearers Response"},
    /* 172-175 For future use */
    {176, "Downlink Data Notification"},
    {177, "Downlink Data Notification Acknowledgement"},
    {178, "Reserved. Allocated in earlier version of the specification."},
    {179, "PGW Restart Notification"},
    {180, "PGW Restart Notification Acknowledge"},
    /* 181-199 For future use */
    {200, "Update PDN Connection Set Request"},
    {201, "Update PDN Connection Set Response"},
    /* 202 to 210 For future use */
    {211, "Modify Access Bearers Request"},
    {212, "Modify Access Bearers Response"},
    /* 213 to 230 For future use */
    {231, "MBMS Session Start Request"},
    {232, "MBMS Session Start Response"},
    {233, "MBMS Session Update Request"},
    {234, "MBMS Session Update Response"},
    {235, "MBMS Session Stop Request"},
    {236, "MBMS Session Stop Response"},
    /* 237 to 239 For future use */
    {240, "SRVCC CS to PS Response"},
    {241, "SRVCC CS to PS Complete Notification"},
    {242, "SRVCC CS to PS Complete Acknowledge"},
    {243, "SRVCC CS to PS Cancel Notification"},
    {244, "SRVCC CS to PS Cancel Acknowledge"},
    /* 245 to 247       For future Sv interface use*/
    /* 248 to 255 For future use */
    {0, NULL}
};

static struct tok gtpv2_info_element_type[] = {
    {  0, "Reserved"},
    {  1, "International Mobile Subscriber Identity (IMSI)"},
    {  2, "Cause"}, 
    {  3, "Recovery (Restart Counter)"},
    /* 4-50 Reserved for S101 interface Extendable / See 3GPP TS 29.276 [14] */
    { 51, "STN-SR"},
    { 52, "Source to Target Transparent Container"},
    { 53, "Target to Source Transparent Container"},
    { 54, "MM Context for E-UTRAN SRVCC"},
    { 55, "MM Context for UTRAN SRVCC"},
    { 56, "SRVCC Cause"},
    { 57, "Target RNC ID"},
    { 58, "Target Global Cell ID"},
    { 59, "TEID-C"},
    { 60, "Sv Flags"},
    { 61, "Service Area Identifier"},
   /* 62-70 For future Sv interface use */
    { 71, "Access Point Name (APN)"},
    { 72, "Aggregate Maximum Bit Rate (AMBR)"},
    { 73, "EPS Bearer ID (EBI)"},
    { 74, "IP Address"},
    { 75, "Mobile Equipment Identity (MEI)"},
    { 76, "MSISDN"},
    { 77, "Indication"},
    { 78, "Protocol Configuration Options (PCO)"},
    { 79, "PDN Address Allocation (PAA)"},
    { 80, "Bearer Level Quality of Service (Bearer QoS)"},
    { 81, "Flow Quality of Service (Flow QoS)"},
    { 82, "RAT Type"},
    { 83, "Serving Network"},
    { 84, "EPS Bearer Level Traffic Flow Template (Bearer TFT)"},
    { 85, "Traffic Aggregation Description (TAD)"},
    { 86, "User Location Info (ULI)"},                            
    { 87, "Fully Qualified Tunnel Endpoint Identifier (F-TEID)"},
    { 88, "TMSI"},                                              
    { 89, "Global CN-Id"},                                     
    { 90, "S103 PDN Data Forwarding Info (S103PDF)"},         
    { 91, "S1-U Data Forwarding Info (S1UDF)"},              
    { 92, "Delay Value"},                                   
    { 93, "Bearer Context"},                               
    { 94, "Charging ID"},                                 
    { 95, "Charging Characteristics"},                   
    { 96, "Trace Information"},                         
    { 97, "Bearer Flags"},                             
    { 98, "Paging Cause"},                            
    { 99, "PDN Type"},                               
    {100, "Procedure Transaction ID"},              
    {101, "DRX Parameter"},                        
    {102, "UE Network Capability"},               
    {103, "MM Context (GSM Key and Triplets)"},  
    {104, "MM Context (UMTS Key, Used Cipher and Quintuplets)"},
    {105, "MM Context (GSM Key, Used Cipher and Quintuplets)"},
    {106, "MM Context (UMTS Key and Quintuplets)"},           
    {107, "MM Context (EPS Security Context, Quadruplets and Quintuplets)"},
    {108, "MM Context (UMTS Key, Quadruplets and Quintuplets)"},
    {109, "PDN Connection"},                                   
    {110, "PDU Numbers"},                                     
    {111, "P-TMSI"},                                         
    {112, "P-TMSI Signature"},                              
    {113, "Hop Counter"},                                  
    {114, "UE Time Zone"},                                
    {115, "Trace Reference"},                            
    {116, "Complete Request Message"},                  
    {117, "GUTI"},                                     
    {118, "F-Container"},                             
    {119, "F-Cause"},                                
    {120, "Selected PLMN ID"},                      
    {121, "Target Identification"},                
    {122, "NSAPI"},                               
    {123, "Packet Flow ID"},                     
    {124, "RAB Context"},                       
    {125, "Source RNC PDCP Context Info"},     
    {126, "UDP Source Port Number"},          
    {127, "APN Restriction"},                
    {128, "Selection Mode"},                
    {129, "Source Identification"},        
    {130, "Bearer Control Mode"},         
    {131, "Change Reporting Action"},    
    {132, "Fully Qualified PDN Connection Set Identifier (FQ-CSID)"},
    {133, "Channel needed"},                                        
    {134, "eMLPP Priority"},                                       
    {135, "Node Type"},                                           
    {136, "Fully Qualified Domain Name (FQDN)"},                 
    {137, "Transaction Identifier (TI)"},                       
    {138, "MBMS Session Duration"},                            
    {139, "MBMS Service Area"},                               
    {140, "MBMS Session Identifier"},                        
    {141, "MBMS Flow Identifier"},                          
    {142, "MBMS IP Multicast Distribution"},               
    {143, "MBMS Distribution Acknowledge"},               
    {144, "RFSP Index"},                                 
    {145, "User CSG Information (UCI)"},                
    {146, "CSG Information Reporting Action"},         
    {147, "CSG ID"},                                  
    {148, "CSG Membership Indication (CMI)"},        
    {149, "Service indicator"},                     
    {150, "Detach Type"},                          
    {151, "Local Distiguished Name (LDN)"},       
    {152, "Node Features"},                      
    {153, "MBMS Time to Data Transfer"},        
    {154, "Throttling"},                       
    {155, "Allocation/Retention Priority (ARP)"},   
    {156, "EPC Timer"},                            
    {157, "Signalling Priority Indication"},      
    {158, "Temporary Mobile Group Identity"},    
    {159, "Additional MM context for SRVCC"},   
    {160, "Additional flags for SRVCC"},       
    {161, "Max MBR/APN-AMBR (MMBR)"},         
    {162, "MDT Configuration"},              
    {163, "Additional Protocol Configuration Options (APCO)"},
    {164, "Absolute Time of MBMS Data Transfer"},            
    {165, "H(e)NB Information Reporting"},                  
    {166, "IPv4 Configuration Parameters (IP4CP)"},        
    {167, "Change to Report Flags"},                      
    {168, "Action Indication"},                          
    {169, "TWAN Identifier "},                          
    {170, "ULI Timestamp"},                            
    {171, "MBMS Flags"},                              
    {172, "RAN/NAS Cause"},                          
    {173, "CN Operator Selection Entity"},          
    {174, "Trusted WLAN Mode Indication"},         
    {175, "Node Number"},                         
    {176, "Node Identifier"},                    
    {177, "Presence Reporting Area Action"},    
    {178, "Presence Reporting Area Information"},
    {179, "TWAN Identifier Timestamp"},         
    {180, "Overload Control Information"},     
    {181, "Load Control Information"},        
    {182, "Metric"},                         
    {183, "Sequence Number"},               
    {184, "APN and Relative Capacity"},    
    /* 185 to 254    Spare. For future use.    */
    {255, "Private Extension"},          
    {0, NULL}
};

static struct tok gtpv2_cause[] = {
    {0, "Reserved"},
    /* Request / Initial message */
    {  1, "Reserved"},
    {  2, "Local Detach"},
    {  3, "Complete Detach"},
    {  4, "RAT changed from 3GPP to Non-3GPP"},
    {  5, "ISR deactivation"},
    {  6, "Error Indication received from RNC/eNodeB/S4-SGSN"},
    {  7, "IMSI Detach Only"},
    {  8, "Reactivation Requested"},
    {  9, "PDN reconnection to this APN disallowed"},
    { 10, "Access changed from Non-3GPP to 3GPP"},
    { 11, "PDN connection inactivity timer expires"},
    { 12, "PGW not responding"},
    { 13, "Network Failure"},
    { 14, "QoS parameter mismatch"},
    /* 15 Spare. This value range is reserved for Cause values in a request message */
    { 15, "Spare"},
    /* Acceptance in a Response / triggered message */
    { 16, "Request accepted"},
    { 17, "Request accepted partially"},
    { 18, "New PDN type due to network preference"},
    { 19, "New PDN type due to single address bearer only"},
    /* 20-63 Spare. This value range shall be used by Cause values in an acceptance response/triggered message */
    { 20, "Spare"},
    { 21, "Spare"},
    { 22, "Spare"},
    { 23, "Spare"},
    { 24, "Spare"},
    { 25, "Spare"},
    { 26, "Spare"},
    { 27, "Spare"},
    { 28, "Spare"},
    { 29, "Spare"},
    { 30, "Spare"},
    { 31, "Spare"},
    { 32, "Spare"},
    { 33, "Spare"},
    { 34, "Spare"},
    { 35, "Spare"},
    { 36, "Spare"},
    { 37, "Spare"},
    { 38, "Spare"},
    { 39, "Spare"},
    { 40, "Spare"},
    { 41, "Spare"},
    { 42, "Spare"},
    { 43, "Spare"},
    { 44, "Spare"},
    { 45, "Spare"},
    { 46, "Spare"},
    { 47, "Spare"},
    { 48, "Spare"},
    { 49, "Spare"},
    { 50, "Spare"},
    { 51, "Spare"},
    { 52, "Spare"},
    { 53, "Spare"},
    { 54, "Spare"},
    { 55, "Spare"},
    { 56, "Spare"},
    { 57, "Spare"},
    { 58, "Spare"},
    { 59, "Spare"},
    { 60, "Spare"},
    { 61, "Spare"},
    { 62, "Spare"},
    { 63, "Spare"},
    /* Rejection in a Response / triggered message */
    { 64, "Context Not Found"},
    { 65, "Invalid Message Format"},
    { 66, "Version not supported by next peer"},
    { 67, "Invalid length"},
    { 68, "Service not supported"},
    { 69, "Mandatory IE incorrect"},
    { 70, "Mandatory IE missing"},
    { 71, "Shall not be used"},
    { 72, "System failure"},
    { 73, "No resources available"},
    { 74, "Semantic error in the TFT operation"},
    { 75, "Syntactic error in the TFT operation"},
    { 76, "Semantic errors in packet filter(s)"},
    { 77, "Syntactic errors in packet filter(s)"},
    { 78, "Missing or unknown APN"},
    { 79, "Shall not be used"},
    { 80, "GRE key not found"},
    { 81, "Relocation failure"},
    { 82, "Denied in RAT"},
    { 83, "Preferred PDN type not supported"},
    { 84, "All dynamic addresses are occupied"},
    { 85, "UE context without TFT already activated"},
    { 86, "Protocol type not supported"},
    { 87, "UE not responding"},
    { 88, "UE refuses"},
    { 89, "Service denied"},
    { 90, "Unable to page UE"},
    { 91, "No memory available"},
    { 92, "User authentication failed"},
    { 93, "APN access denied - no subscription"},
    { 94, "Request rejected(reason not specified)"},
    { 95, "P-TMSI Signature mismatch"},
    { 96, "IMSI/IMEI not known"},
    { 97, "Semantic error in the TAD operation"},
    { 98, "Syntactic error in the TAD operation"},
    { 99, "Shall not be used"},
    {100, "Remote peer not responding"},
    {101, "Collision with network initiated request"},
    {102, "Unable to page UE due to Suspension"},
    {103, "Conditional IE missing"},
    {104, "APN Restriction type Incompatible with currently active PDN connection"},
    {105, "Invalid overall length of the triggered response message and a piggybacked initial message"},
    {106, "Data forwarding not supported"},
    {107, "Invalid reply from remote peer"},
    {108, "Fallback to GTPv1"},
    {109, "Invalid peer"},
    {110, "Temporarily rejected due to handover procedure in progress"},
    {111, "Modifications not limited to S1-U bearers"},
    {112, "Request rejected for a PMIPv6 reason "},
    {113, "APN Congestion"},
    {114, "Bearer handling not supported"},
    {115, "UE already re-attached"},
    {116, "Multiple PDN connections for a given APN not allowed"},
    /* 117-239 Spare. For future use in a triggered/response message  */
    /* 240-255 Spare. For future use in an initial/request message */
    {0, NULL}
};

static struct tok gtpv2_srvcc_cause[] = {
    {0, "Reserved"},
    {1, "Unspecified"},
    {2, "Handover/Relocation cancelled by source system "},
    {3, "Handover /Relocation Failure with Target system"},
    {4, "Handover/Relocation Target not allowed"},
    {5, "Unknown Target ID"},
    {6, "Target Cell not available"},
    {7, "No Radio Resources Available in Target Cell"},
    {8, "Failure in Radio Interface Procedure"},
    {9, "Permanent session leg establishment error"},
    {10, "Temporary session leg establishment error"},

    {0, NULL}
};

static struct tok gtpv2_rat_type[] = {
    {0, "Reserved"},
    {1, "UTRAN"},
    {2, "GERAN"},
    {3, "WLAN"},
    {4, "GAN"},
    {5, "HSPA Evolution"},
    {6, "EUTRAN"},
    {7, "Virtual"},
    {0, NULL}
};

static struct tok geographic_location_type[] = {
    {0,   "CGI"},
    {1,   "SAI"},
    {2,   "RAI"},
    {128, "TAI"},
    {129, "ECGI"},
    {130, "TAI and ECGI"},
    {0, NULL}
};


static struct tok gtpv2_f_teid_interface_type[] = {
    { 0, "S1-U eNodeB GTP-U interface"},
    { 1, "S1-U SGW GTP-U interface"},
    { 2, "S12 RNC GTP-U interface"},
    { 3, "S12 SGW GTP-U interface"},
    { 4, "S5/S8 SGW GTP-U interface"},
    { 5, "S5/S8 PGW GTP-U interface"},
    { 6, "S5/S8 SGW GTP-C interface"},
    { 7, "S5/S8 PGW GTP-C interface"},
    { 8, "S5/S8 SGW PMIPv6 interface"}, 
    { 9, "S5/S8 PGW PMIPv6 interface"}, 
    {10, "S11 MME GTP-C interface"},
    {11, "S11/S4 SGW GTP-C interface"},
    {12, "S10 MME GTP-C interface"},
    {13, "S3 MME GTP-C interface"},
    {14, "S3 SGSN GTP-C interface"},
    {15, "S4 SGSN GTP-U interface"},
    {16, "S4 SGW GTP-U interface"},
    {17, "S4 SGSN GTP-C interface"},
    {18, "S16 SGSN GTP-C interface"},
    {19, "eNodeB GTP-U interface for DL data forwarding"},
    {20, "eNodeB GTP-U interface for UL data forwarding"},
    {21, "RNC GTP-U interface for data forwarding"},
    {22, "SGSN GTP-U interface for data forwarding"},
    {23, "SGW GTP-U interface for data forwarding"},
    {24, "Sm MBMS GW GTP-C interface"},
    {25, "Sn MBMS GW GTP-C interface"},
    {26, "Sm MME GTP-C interface"},
    {27, "Sn SGSN GTP-C interface"},
    {28, "SGW GTP-U interface for UL data forwarding"},
    {29, "Sn SGSN GTP-U interface"},
    {30, "S2b ePDG GTP-C interface"},
    {31, "S2b-U ePDG GTP-U interface"},
    {32, "S2b PGW GTP-C interface"},
    {33, "S2b-U PGW GTP-U interface"},
    {34, "S2a TWAN GTP-U interface"},
    {35, "S2a TWAN GTP-C interface"},
    {36, "S2a PGW GTP-C interface"},
    {37, "S2a PGW GTP-U interface"},
    {0, NULL}
};

static struct tok gtpv2_mm_context_security_mode[] = {
    {0, "GSM Key and Triplets"},
    {1, "UMTS Key, Used Cipher and Quintuplets"},
    {2, "GSM Key, Used Cipher and Quintuplets"},
    {3, "UMTS Key and Quintuplets"},
    {4, "EPS Security Context, Quadruplets and Quintuplets" },
    {5, "UMTS Key, Quadruplets and Quintuplets"},
    {0, NULL }
};

static struct tok gtpv2_mm_context_unc[] = {
    {0, "No ciphering"},
    {1, "128-EEA1"},
    {2, "128-EEA2"},
    {3, "EEA3"},
    {4, "EEA4"  },
    {5, "EEA5"},
    {6, "EEA6"},
    {7, "EEA7"},
    {0, NULL}
};

static struct tok gtpv2_mm_context_used_cipher[] = {
    {0, "No ciphering"},
    {1, "GEA/1"},
    {2, "GEA/2"},
    {3, "GEA/3"},
    {4, "GEA/4" },
    {5, "GEA/5"},
    {6, "GEA/6"},
    {7, "GEA/7"},
    {0, NULL}
};

static struct tok gtpv2_mm_context_unipa[] = {
    {0, "No ciphering"},
    {1, "128-EEA1"},
    {2, "128-EEA2"},
    {3, "EEA3"},
    {4, "EEA4"  },
    {5, "EEA5"},
    {6, "EEA6"},
    {7, "EEA7"},
    {0, NULL}
};

static struct tok gtpv2_ue_time_zone_dst[] = {
    {0, "No Adjustments for Daylight Saving Time"},
    {1, "+1 Hour Adjustments for Daylight Saving Time"},
    {2, "+2 Hour Adjustments for Daylight Saving Time"},
    {3, "Spare"},
    {0, NULL}
};


static struct tok gtpv2_complete_req_msg_type[] = {
    {0, "Complete Attach Request Message"  },
    {1, "Complete TAU Request Message"     },
    {0, NULL                               }
};

static struct tok gtpv2_container_type[] = {
    {1, "UTRAN transparent container"},
    {2, "BSS container"},
    {3, "E-UTRAN transparent container"},
    {0, NULL}
};

static struct tok gtpv2_cause_type[] = {
    {0,  "Radio Network Layer"},
    {1,  "Transport Layer"},
    {2,  "NAS"},
    {3,  "Protocol"},
    {4,  "Miscellaneous"},
    {5,  "<spare>"},
    {6,  "<spare>"},
    {7,  "<spare>"},
    {8,  "<spare>"},
    {9,  "<spare>"},
    {10, "<spare>"},
    {11, "<spare>"},
    {12, "<spare>"},
    {13, "<spare>"},
    {14, "<spare>"},
    {15, "<spare>"},
    {0, NULL}
};

static struct tok gtpv2_target_type[] = {
    {0,  "RNC ID"},
    {1,  "Macro eNodeB ID"},
    {2,  "Cell Identifier"},
    {3,  "Home eNodeB ID"},
    {0, NULL}
};

static struct tok gtpv2_pdn_type[] = {
    {1, "IPv4"},
    {2, "IPv6"},
    {3, "IPv4/IPv6"},
    {0, NULL}
};

static struct tok gtpv2_selec_mode[] = {
    {0, "MS or network provided APN, subscribed verified"},
    {1, "MS provided APN, subscription not verified"},
    {2, "Network provided APN, subscription not verified"},
    {3, "Network provided APN, subscription not verified (Basically for Future use"},
    {0, NULL}
};

static struct tok gtpv2_bearer_control_mode[] = {
    {0, "Selected Bearer Control Mode-'MS_only'"},
    {1, "Selected Bearer Control Mode-'Network_only'"},
    {2, "Selected Bearer Control Mode-'MS/NW'"},
    {0, NULL}
};

static struct tok gtpv2_bearer_control_mode_short[] = {
    {0, "MS_only"},
    {1, "Network_only"},
    {2, "MS/NW"},
    {0, NULL}
};

static struct tok gtpv2_cng_rep_act[] = {
    {0, "Stop Reporting"},
    {1, "Start Reporting CGI/SAI"},
    {2, "Start Reporting RAI"},
    {3, "Start Reporting TAI"},
    {4, "Start Reporting ECGI"},
    {5, "Start Reporting CGI/SAI and RAI"},
    {6, "Start Reporting TAI and ECGI"},
    {0, NULL}
};

static struct tok gtpv2_node_type[] = {
    {0, "MME"},
    {1, "SGSN"},
    {0, NULL}
};

static struct tok gtpv2_mbms_hc_indicator[] = {
    {0, "Uncompressed header"},
    {1, "Compressed header"},
    {0, NULL}
};

static struct tok gtpv2_mbms_dist_indication[] = {
    {0, "No RNCs have accepted IP multicast distribution"},
    {1, "All RNCs have accepted IP multicast distribution"},
    {2, "Some RNCs have accepted IP multicast distribution"},
    {3, "Spare. For future use."},
    {0, NULL}
};

static struct tok gtpv2_timer_unit[] = {
    {0, "value is incremented in multiples of 2 seconds"},
    {1, "value is incremented in multiples of 1 minute"},
    {2, "value is incremented in multiples of 10 minutes"},
    {3, "value is incremented in multiples of 1 hour"},
    {4, "value is incremented in multiples of 1 hour"},
    {5, "Other values shall be interpreted as multiples of 1 minute(version 10.7.0)"},
    {6, "Other values shall be interpreted as multiples of 1 minute(version 10.7.0)"},
    {7, "value indicates that the timer is infinite"},
    {0, NULL}
};

static struct tok gtpv2_action_indication[] = {
    { 0, "No Action"},
    { 1, "Deactivation Indication"},
    { 2, "Paging Indication"},
    { 3, "Spare"},
    { 4, "Spare"},
    { 5, "Spare"},
    { 6, "Spare"},
    { 7, "Spare"},
    { 0, NULL}
};

static struct tok gtpv2_pres_rep_area_action[] = {
    { 1, "Start Reporting change"},
    { 2, "Stop Reporting change"},
    { 0, NULL}
};

static struct tok gtpv2_pco_proto[] = {
  {0xC021, "PPP LCP"},
  {0xC023, "PPP PAP"},
  {0xC223, "PPP CHAP"},
  {0x8021, "PPP IPCP"},
  {0,NULL}
};

static struct tok gtpv2_pco_ms_nw_proto[] = {
  {0x0001, "P-CSCF IPv6 Address Request"},
  {0x0002, "IM CN Subsystem Signaling Flag"},
  {0x0003, "DNS Server IPv6 Address Request"},
  {0x0004, "Not Supported"},
  {0x0005, "MS Support of Network Requested Bearer Control indicator"},
  {0x0006, "Reserved"},
  {0x0007, "DSMIPv6 Home Agent Address Request"},
  {0x0008, "DSMIPv6 Home Network Prefix Request"},
  {0x0009, "DSMIPv6 IPv4 Home Agent Address Request"},
  {0x000A, "IP address allocation via NAS signalling"},
  {0x000B, "IPv4 address allocation via DHCPv4"},
  {0x000C, "P-CSCF IPv4 Address Request"},
  {0x000D, "DNS Server IPv4 Address Request"},
  {0x000E, "MSISDN Request"},
  {0x000F, "IFOM-Support-Request"},
  {0x0010, "IPv4 Link MTU Request"},
  {0, NULL}
};

static struct tok gtpv2_pco_nw_ms_proto[] = {
  {0x0001, "P-CSCF IPv6 Address"},
  {0x0002, "IM CN Subsystem Signaling Flag"},
  {0x0003, "DNS Server IPv6 Address"},
  {0x0004, "Policy Control rejection code"},
  {0x0005, "Selected Bearer Control Mode"},
  {0x0006, "Reserved"},
  {0x0007, "DSMIPv6 Home Agent Address"},
  {0x0008, "DSMIPv6 Home Network Prefix"},
  {0x0009, "DSMIPv6 IPv4 Home Agent Address"},
  {0x000A, "Reserved"},
  {0x000B, "Reserved"},
  {0x000C, "P-CSCF IPv4 Address"},
  {0x000D, "DNS Server IPv4 Address"},
  {0x000E, "MSISDN"},
  {0x000F, "IFOM-Support"},
  {0x0010, "IPv4 Link MTU"},
  {0, NULL}
};

#if 0
static const true_false_string gtpv2_cause_cs_string = {
    "Originated by remote node",
    "Originated by node sending the message",
};

static const true_false_string gtpv2_f_teid_v4_vals = {
    "IPv4 address present",
    "IPv4 address not present",
};

static const true_false_string gtpv2_f_teid_v6_vals = {
    "IPv6 address present",
    "IPv6 address not present",
};

static const true_false_string gtpv2_nhi_vals = {
    "NH (Next Hop) and NCC (Next Hop Chaining Count) are both present",
    "NH (Next Hop) and NCC (Next Hop Chaining Count) not present",
};

static const true_false_string gtpv2_henb_info_report_fti_vals = {
    "Start reporting H(e)NB local IP address and UDP port number information change",
    "Stop reporting H(e)NB local IP address and UDP port number information change",
};
#endif


/*
 * Decoding for GTP version 2, which consists of GTPv2-C'.
 * cp - start point of the gtpv2 message
 * length - length of the gtp message
 */
void
gtp_v2_print(const u_char *cp, u_int length, u_short sport, u_short dport)
{
  gtp_v2_hdr_t *gh = (gtp_v2_hdr_t *)cp;
  int nexthdr, hlen;
  int ielen;
  u_char *p = (u_char *)cp;
  unsigned int teid;
  unsigned int seqno;

  gtp_proto = GTP_V2_CTRL_PROTO;

  //TCHECK(gh->flags);
  printf(" GTPv2-C ");
  printf("%s", tok2str(gtp_v2_msgtype, "Message Type %u", gh->msgtype));
  if (gh->flags & GTPV2_HDR_TEID) {
    printf(" T ");
  }
  if (gh->flags & GTPV2_HDR_PIGGY) {
    printf(" P ");
  }

  /* Decode GTP header. */
  //TCHECK(*gh);

  hlen = ntohs(gh->length);
  printf(" (len %u)", hlen);
  p += sizeof(gtp_v2_hdr_t);

  ielen = hlen;

  if (ielen <= 4)
    goto trunc;


  if (gh->flags & GTPV2_HDR_TEID) {
    teid = *(unsigned int *)p;
    printf(" (teid %u) ", ntohl(teid));
    p += sizeof(teid);
    ielen -= sizeof(teid);
    if (ielen <= 0)
      goto trunc;
  }

  seqno = *(unsigned int *)p;
  seqno = ntohl(seqno);
  seqno = seqno >> 8;
  printf(" [seq %ul]", seqno);

  ielen -= 4;
  p += 4;

  if (ielen <= 0)
    goto trunc;

  if (ielen > length)
    goto trunc;

  if (vflag)
    gtp_v2_decode_ie(p, ielen);

  return;

trunc:
  printf(" [|%s]", tok2str(gtp_type, "GTP", gtp_proto));
}

static void
gtp_v2_decode_ie(register const u_char *cp, int len)
{
  int val, ielen, iecount = 0;

  if (len <= 0)
    return;

  printf(" {");

  while (len > 0) {

    iecount++;
    if (iecount > 1)
      printf(" ");

    //TCHECK(cp[0]);
    val = (u_int)cp[0];
    cp++;

    printf("\n");
    printf("[");

    if (val == GTPV2_IE_BEARER_CTX) {
      ielen = gtp_v2_decode_ie_brctx(cp,len);
    }
    else {
      ielen = gtp_v2_print_tlv(cp, val,len);
    }

    printf("]");

    if (ielen < 0)
      goto trunc;

    len -= ielen;
    cp += ielen - 1;
  }

  if (iecount > 0)
    printf("}");

  return;

trunc:
  printf(" [|%s]", tok2str(gtp_type, "GTP", gtp_proto));
}

static void
gtp_v2_print_plmn(register const u_char *cp)
{
  if ((cp[1]& 0xf0) == 0xf0) {
    /* Two digit MNC */
    printf("MCC:<%u%u%u> MNC:<%u%u>",
           cp[0]&0x0f,((cp[0]&0xf0)>>4), cp[1]&0x0f,
           cp[2]&0x0f,((cp[2]&0xf0)>>4));
  } else {
    /* Three digit MNC */
    printf("MCC:<%u%u%u> MNC:<%u%u%u>",
           cp[0]&0x0f,((cp[0]&0xf0)>>4),cp[1]&0x0f,
           cp[2]&0x0f,((cp[2]&0xf0)>>4),((cp[1]&0xf0)>>4));
  }
  return;
}

static int
gtp_v2_print_tlv(register const u_char *cp, u_int value, int length)
{
  u_int8_t instance,spare;
  u_int16_t *lenp, *seqno, len;
  int ielen = -1;
  int i;
  u_int16_t flag;

  /* Get length of IE. */
  //TCHECK2(cp[0], 2);
  lenp = (u_int16_t *)cp;
  cp += 2;
  len = ntohs(*lenp);
  if (len > length)
    goto trunc;
  instance = cp[0] & 0x0f;
  spare = (cp[0] & 0xf0) >> 4;
  //TCHECK2(cp[0], len);
  cp += 1;
  ielen = len + 4;

  printf("<l:%2u s:%2x i:%2u> ",len,spare,instance);

  switch (value) {
  case GTPV2_IE_IMSI:
    printf("%s","IMSI: ");
    gtp_print_tbcd(cp, len);
    break;

  case GTPV2_IE_CAUSE:
    printf("%s","CAUSE: ");
    gtp_v2_print_cause(cp,len);
    break;

  case GTPV2_REC_REST_CNT:
    printf("%s","Recovery: ");
    printf(" %u ",cp[0]);
    break;

  case GTPV2_APN:
    printf("%s","AP Name: ");
    gtp_print_apn(cp, len);
    break;

  case GTPV2_IE_RAT_TYPE:
    printf("%s","RAT Type: ");
    printf("%s", tok2str(gtpv2_rat_type, "#%u", cp[0]));
    break;

  case GTPV2_IE_PDN_TYPE:
    printf("%s","PDN Type: ");
    printf("%s", tok2str(gtpv2_pdn_type, "#%u", cp[0]));
    break;

  case GTPV2_IE_SEL_MODE:
    printf("%s","Selection Mode: ");
    printf("%s", tok2str(gtpv2_selec_mode, "#%u", cp[0]));
    break;

  case GTPV2_PAA:
    printf("%s","PDN Address Allocation (PAA): ");
    if (len >= 1) {
      u_int8_t pdntype = cp[0] & 0x03;
      cp++;
      printf("%s","PDN Type: ");
      printf("%s", tok2str(gtpv2_pdn_type, "#%u", pdntype));
      printf(" <");
      if (pdntype == 1) { //IPv4
        struct in_addr tmpipaddr;
        tmpipaddr.s_addr = *(uint32_t *)cp;
        printf("%s",inet_ntoa(tmpipaddr));
      }
      else if (pdntype == 2 || pdntype == 3) {
        printf("V6PrefixLen<%u> V6Addr:<",cp[0]);
        cp++;
        for (i=0;i<16;i++) {
          printf("%02x",cp[i]);
        }
        cp+=16;
        printf(">");
        if (pdntype == 3) {
          struct in_addr tmpipaddr;
          tmpipaddr.s_addr = *(uint32_t *)cp;
          printf(" V4Addr:<%s>",inet_ntoa(tmpipaddr));
        }
      }
      printf("> ");
    }
    break;

  case GTPV2_IE_UE_TIME_ZONE:
    printf("%s","UE Time Zone: ");
    if (len >= 2) {
      printf(" Time Zone :%u", cp[0]); 
      printf(" Daylight Saving Time:%u", cp[1]&0x03); 
      for (i=2;i<len;i++) {
        printf("%02x",cp[i]);
      }
    }
    break;

  case GTPV2_PCO:
    printf("%s","Protocol Configuration Options (PCO):");
    cp++;
    len -= 1;
    for (i=0;i<len;) {
      u_int16_t proto = *(u_int16_t *)cp; 
      proto = ntohs(proto);
      cp += 2;
      i += 2;
      u_int8_t protlen = *cp;
      cp++;
      i += 1;
      if (proto == GTPV2_PCO_PROT_LCP || 
           proto == GTPV2_PCO_PROT_PAP ||
           proto == GTPV2_PCO_PROT_CHAP ||
           proto == GTPV2_PCO_PROT_IPCP) {
        printf("%s", tok2str(gtpv2_pco_proto, "#%u", proto));
      }
      else if (protlen) {
        printf("%s", tok2str(gtpv2_pco_nw_ms_proto, "#%u", proto));
      }
      else {
        printf("%s", tok2str(gtpv2_pco_ms_nw_proto, "#%u", proto));
      }
      i += protlen;
      printf(" <len(%u)> ",protlen);
      if (protlen > 0) {
        int j;
        printf(" data<");
        for (j=0;j<protlen;j++) {
          printf("%02x",cp[j]);
        }
        printf("> ");
      }
      cp += protlen;
    }
    break;

  case GTPV2_IE_ULI:
    printf("%s","User Location Info (ULI):");
    flag = *cp;
    cp++;
    if (flag & GTPV2_ULI_CGI) {
      printf(" CGI: ");
      gtp_v2_print_plmn(cp);
      printf("LAC:<%02x%02x>",cp[3],cp[4]);
      printf("CI:<%02x%02x>",cp[5],cp[6]);
      cp += 7;
    }
    if (flag & GTPV2_ULI_SAI) {
      printf(" SAI: ");
      gtp_v2_print_plmn(cp);
      printf("LAC:<%02x%02x>",cp[3],cp[4]);
      printf("SAC:<%02x%02x>",cp[5],cp[6]);
      cp += 7;
    }
    if (flag & GTPV2_ULI_RAI) {
      printf(" RAI: ");
      gtp_v2_print_plmn(cp);
      printf("LAC:<%02x%02x>",cp[3],cp[4]);
      printf("RAC:<%02x%02x>",cp[5],cp[6]);
      cp += 7;
    }
    if (flag & GTPV2_ULI_TAI) {
      printf(" TAI: ");
      gtp_v2_print_plmn(cp);
      printf("TAC:<%02x%02x>",cp[3],cp[4]);
      cp += 5;
    }
    if (flag & GTPV2_ULI_ECGI) {
      printf(" ECGI: ");
      gtp_v2_print_plmn(cp);
      printf("ECI:<%02x%02x>",cp[3]&0xF,cp[4]);
      printf("ECI:<%02x%02x>",cp[5],cp[6]);
      cp += 7;
    }
    if (flag & GTPV2_ULI_LAI) {
      printf(" LAI: ");
      gtp_v2_print_plmn(cp);
      printf("LAC:<%02x%02x>",cp[3],cp[4]);
      cp += 5;
    }
    break;

  case GTPV2_IE_F_TEID:
    printf("%s","Fully Qualified Tunnel Endpoint Identifier (F-TEID):");
    flag = *cp;
    u_int32_t ipv4;
    u_int8_t ipv6[16];
    u_int8_t teid[4];
    printf(" %s ", tok2str(gtpv2_f_teid_interface_type, "#%u", (flag&GTPV2_IE_F_TEID_IFMASK)));
    cp++;
    printf("TEID: <%02x%02x%02x%02x> ",cp[0],cp[1],cp[2],cp[3]);
    cp += 4;
    if (flag & GTPV2_IE_F_TEID_V4) {
     struct in_addr tmpipaddr;
     tmpipaddr.s_addr = *(uint32_t *)cp;
     printf("V4:<%s>",inet_ntoa(tmpipaddr));
     //printf("V4: <%d.%d.%d.%d> ",cp[0],cp[1],cp[2],cp[3]);
     cp += 4;
    }
    if (flag & GTPV2_IE_F_TEID_V6) {
     printf("V6: <");
     for(i=0;i<16;i++) {
       printf("%02x",cp[i]);
     }
     printf("> ");
    }
    break;

  case GTPV2_IE_SERV_NET:
    printf("%s","Serving Network:");
    if (len == 3) {
      gtp_v2_print_plmn(cp);
    }
    else {
      printf(" ");
      for (i=0;i<len;i++) {
        printf("%02x",cp[i]);
      }
      printf(" ");
    }
    break;

  case GTPV2_AMBR:
    printf("%s","AMBR Aggregate Maximum Bit Rate (AMBR):");
    u_int32_t uplink = *(u_int32_t *)cp;
    u_int32_t downlink = *(u_int32_t *)(cp+4);
    uplink = ntohl(uplink);
    downlink = ntohl(downlink);
    printf("UL <%u>, DL <%u>", uplink, downlink);
    break;

  case GTPV2_IE_BEARER_CTX:
    /* have special routine to print this */
    break;

  case GTPV2_BEARER_QOS:
    printf("%s","Bearer Level Quality of Service (Bearer QoS):");
    printf("ARP:<%u> ", cp[0]);
    cp++;
    printf("QCI:<%u> ", cp[0]);
    cp++;
    printf("UL MBR:<%02x%02x%02x%02x%02x> ",cp[0],cp[1],cp[2],cp[3],cp[4]);
    cp += 5;
    printf("DL MBR:<%02x%02x%02x%02x%02x> ",cp[0],cp[1],cp[2],cp[3],cp[4]);
    cp += 5;
    printf("UL GBR:<%02x%02x%02x%02x%02x> ",cp[0],cp[1],cp[2],cp[3],cp[4]);
    cp += 5;
    printf("DL GBR:<%02x%02x%02x%02x%02x> ",cp[0],cp[1],cp[2],cp[3],cp[4]);
    cp += 5;
    break;

  default:
    printf("%s: ", tok2str(gtpv2_info_element_type, "#%u", value));
    //printf("TLV %u (len %u)", value, len);
    printf(" ");
    for (i=0;i<len;i++) {
      printf("%02x",cp[i]);
    }
    printf(" ");
    break;
  }

  return ielen;

trunc:
  return -1;
}

static void
gtp_v2_print_cause(register const u_char *cp, u_int len)
{
  u_int8_t spare,pce,bce,cs,inst;
  int i;
  
  if (len != 2 && len != 6) {
    printf("(len %u)", len);
    return;
  }

  printf("%s", tok2str(gtpv2_cause, "Cause Value %u", cp[0]));
  cp++;
  spare = ((*cp) & 0xf8) >> 3;
  pce = ((*cp) & 0x04) >> 2;
  bce = ((*cp) & 0x02) >> 1;
  cs = (*cp) & 0x01;
  printf(" <Spare(%u) PCE(%u) BCE(%u) CS(%u)>",spare,pce,bce,cs);
  cp++;

  if (len == 6) {
    printf("Offending IE: %s", tok2str(gtpv2_info_element_type, "#%u", cp[0]));
    cp++;
    u_int16_t *lenp = (u_int16_t *)cp;
    cp += 2;
    u_int16_t olen = ntohs(*lenp);
    spare = ((*cp) & 0xf8) >> 4;
    inst = (*cp) & 0x0f;
    printf(" len(%u) spare(%u) instance (%u)",olen,spare,inst);
  }
  return;
}

static void 
gtp_v2_print_tbcd(register const u_char *cp, u_int len)
{ 
  u_int8_t *p, dat;
  int i;

  p = (u_int8_t *)cp;
  for (i = 0; i < len; i++) {
    dat = *p & 0xf;
    if (dat != 0xf)
      printf("%u", dat);
    dat = *p >> 4;
    if (dat != 0xf)
      printf("%u", dat);
    p++;
  }
}

static void 
gtp_v2_print_apn(register const u_char *cp, u_int len)
{
  u_char data[200];
  u_int8_t ln;

  if (len < 1 || len > 200)
    return;

  while (len > 0) {

    ln = (u_int8_t)cp[0];
    if (ln > 99)
      return;

    bcopy(cp + 1, data, ln);
    data[ln] = '\0';
    printf("%s", data);

    cp += ln + 1;
    len -= ln + 1;

    if (len > 0)
      printf(".");

  }
}

static int
gtp_v2_decode_ie_brctx(register const u_char *cp, int length)
{
  int val, ielen_br, iecount = 0;

  u_int8_t instance,spare;
  u_int16_t *lenp, *seqno, len;
  int ielen = -1;

  /* Get length of IE. */
  //TCHECK2(cp[0], 2);
  lenp = (u_int16_t *)cp;
  cp += 2;
  len = ntohs(*lenp);

  if (len > length) {
    return (length);
  }

  instance = cp[0] & 0x0f;
  spare = (cp[0] & 0xf0) >> 4;
  //TCHECK2(cp[0], len);
  cp += 1;
  ielen_br = len + 4;

  printf("<l:%2u s:%2x i:%2u> Bearer Context:",len,spare,instance);
  printf(" {");

  while (len > 0) {

    iecount++;
    if (iecount > 1)
      printf(" ");

    //TCHECK(cp[0]);
    val = (u_int)cp[0];
    cp++;

    printf("\n");
    printf("[");

    ielen = gtp_v2_print_tlv(cp, val,length);

    printf("]");

    if (ielen < 0) {
      break;
    }

    len -= ielen;
    cp += ielen - 1;
  }

  if (iecount > 0)
    printf("}");

  return (ielen_br);;
}
