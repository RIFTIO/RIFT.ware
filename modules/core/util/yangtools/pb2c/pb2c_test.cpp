
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/cdefs.h>
#ifndef ASSERT
#define ASSERT assert
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef RIFT_OMIT_PROTOC_RWMSG_BINDINGS
#error nope
#endif

#define ZERO_VARIABLE(v) memset((v),0,sizeof(*(v)))

#define MAX_DATA_SIZE 1000000


static void *
malloc0(size_t sz)
{
  void *p = malloc(sz);
  if (NULL != p) {
    memset(p,0,sz);
  }
  return p;
}



#define __PB2C_MARSHALLING_FUNCTIONS__
#include "ipsec_state.pb-c.h"
#include "ipsec_state.pb2c.h"
#include "ipsec_state_flat_cstructs.h"



#include "ex1.pb-c.h"
#include "ex1.pb2c.h"


#include "ex2_base1.pb-c.h"
#include "ex2_base3.pb-c.h"
#include "ex2_base2.pb-c.h"
#include "ex2_base4.pb-c.h"
#include "ex2.pb-c.h"
#include "ex2_base1.pb2c.h"
#include "ex2_base3.pb2c.h"
#include "ex2_base2.pb2c.h"
#include "ex2_base4.pb2c.h"
#include "ex2.pb2c.h"

#include "ex3.pb-c.h"

#define TIM_FLAT
#ifdef TIM_FLAT
#include "tim.pb-c.h"
#include "tim_flat3_cstructs.h"
#else
#include "tim.pb-c.h"
#include "tim.pb2c.h"
#endif

#include "sample1.pb-c.h"
// "normal" generated C types
#include "sample1.pb2c.h"
// "flattened" generated C types
#include "sample1_flat_cstructs.h"


#include "rw_xml.h"


/* Some tests using ex1.proto */
static void
ex1_tests(void)
{
  int i, rc;
  struct __msg1 msg1_c, cm, *pcm, *cm2;
  struct __msg2 *m2_c1, *m2_c2;
  struct __msg4 *m4_c1, *m4_c2, m4;

  uint8_t pkbuf[MAX_DATA_SIZE], pkbuf_tiny[6];
  size_t pkbuf_len = 0;
  ProtobufCBufferSimple sb = PROTOBUF_C_BUFFER_SIMPLE_INIT(NULL, pkbuf_tiny);

  ((void)pcm);
  ((void)cm2);
  
 // populate C variable
  ZERO_VARIABLE(&msg1_c);
  msg1_c.a = 1;
  msg1_c.b = 2;
  //FF  msg1_c.c = NULL;

  // C-struct -> PACK
  //Verify first way
  rc = PB2CM_pack_struct__msg1(&msg1_c,pkbuf,sizeof(pkbuf));
  ASSERT(rc > 0);
  pkbuf_len = rc;
  //Verify second way to pack a C-struct
  PB2CM_pack_to_buffer_struct__msg1(&msg1_c,&sb);
  // Verify result is the same
  ASSERT(sb.len == (unsigned) rc);
  ASSERT(!memcmp(pkbuf,sb.data,sb.len));
  // Free any dynamically allocated memory
  // NOTE: pkbuf_tiny[] was purposfully made short to force a dynamic allocation
  PROTOBUF_C_BUFFER_SIMPLE_CLEAR(&sb);
  
  
  // PACK -> C-struct
  ZERO_VARIABLE(&cm);
  pcm = PB2CM_unpack_struct__msg1(&cm,pkbuf,pkbuf_len);
  ASSERT(pcm == &cm);
  cm2 = PB2CM_unpack_struct__msg1(NULL,pkbuf,pkbuf_len);
  ASSERT(cm2);

  ASSERT(cm.a == 1);
  ASSERT(cm.b == 2);
  ASSERT(cm2);
  ASSERT(cm2->a == 1);
  ASSERT(cm2->b == 2);
  //FF  ASSERT(!strcmp(cm2->c,"ccc"));

  m2_c1 = (struct __msg2 *)malloc0(sizeof(*m2_c1));
  m2_c1->a = 123;
  m2_c1->b = 456;

#define N 1000

  m2_c1->c.vec_len = N;
  m2_c1->c.vec = (uint32_t *)malloc0(N * sizeof(m2_c1->c.vec[0]));
  for(i=0;i<N;i++) {
    m2_c1->c.vec[i] = i;
  }

  m2_c1->d.vec_len = N/2;
  m2_c1->d.vec = (enum __Tenum *) malloc0(N/2 * sizeof(m2_c1->d.vec[0]));
  for(i=0;i<N/2;i++) {
    m2_c1->d.vec[i] = __TENUM__EN2;
  }

  m2_c1->e.vec_len = N;
  m2_c1->e.vec = (char **)malloc0(N * sizeof(m2_c1->e.vec[0]));
  for(i=0;i<N;i++) {
    m2_c1->e.vec[i] = strdup("haha");
  }


  // C-struct -> PACK
  rc = PB2CM_pack_struct__msg2(m2_c1,pkbuf,sizeof(pkbuf));
  ASSERT(rc > 0);
  pkbuf_len = rc;

  // PACK -> C-struct
  m2_c2 = PB2CM_unpack_struct__msg2(NULL,pkbuf,pkbuf_len);
  ASSERT(m2_c2);

  ASSERT(m2_c1->a == m2_c2->a);
  ASSERT(m2_c2->a == 123);
  ASSERT(m2_c2->b == 456);
  ASSERT(m2_c2->c.vec_len == N);
  ASSERT(m2_c2->c.vec[12] == 12);
  ASSERT(m2_c2->d.vec_len == N/2);
  ASSERT(m2_c2->d.vec[0] == __TENUM__EN2);
  ASSERT(m2_c2->d.vec[N/2-1] == __TENUM__EN2);
  ASSERT(m2_c2->e.vec_len == N);
  ASSERT(!strcmp(m2_c2->e.vec[N-1],"haha"));

  // cleanup msg2
  PB2C_DEEP_FREE_struct__msg2(m2_c2,0/*skip top-level free*/);
  free(m2_c2); // Free top-level
  PB2C_DEEP_FREE_struct__msg2(m2_c1,1);

  m4_c1 = (struct __msg4 *)malloc0(sizeof(*m4_c1));
  m4_c1->i1 = 22;
  m4_c1->i3 = 3333;
  m4_c1->foo = (struct __msg3 *)malloc0(sizeof(*m4_c1->foo));
  m4_c1->foo->q.vec_len = 5;
  m4_c1->foo->q.vec = (__ByteVec_t *)malloc(5 * sizeof(m4_c1->foo->q.vec[0]));
  memset(m4_c1->foo->q.vec,0,5 * sizeof(m4_c1->foo->q.vec[0]));
  for(i=0;i<5;i++) {
    int sz = i*i+1;
    m4_c1->foo->q.vec[i].len = sz;
    m4_c1->foo->q.vec[i].data = (uint8_t *) malloc0(sz);
  }
  m4_c1->moo.i2 = 7777;

  // C-struct -> PACK
  rc = PB2CM_pack_struct__msg4(m4_c1,pkbuf,sizeof(pkbuf));
  ASSERT(rc > 0);
  pkbuf_len = rc;

  // PACK -> C-struct
  ZERO_VARIABLE(&m4);
  m4_c2 = PB2CM_unpack_struct__msg4(&m4,pkbuf,pkbuf_len);
  ASSERT(m4_c2 == &m4);

  ASSERT(m4_c1->i1 == m4_c2->i1);
  ASSERT(m4_c2->i3 == m4_c2->i3);
  ASSERT(m4_c2->foo->q.vec_len == 5);
  ASSERT(m4_c2->foo->q.vec[4].len == 17);
  ASSERT(m4_c2->foo->q.vec[4].data != NULL);
  ASSERT(m4_c2->moo.i2 = 7777);

  // cleanup msg2
  PB2C_DEEP_FREE_struct__msg4(m4_c2,0/*Do not free root of m4_c2 - it is local var m4*/);
  m4_c2 = NULL;
  PB2C_DEEP_FREE_struct__msg4(m4_c1,1);
  m4_c1 = NULL;
}





static void
ipsec_test(void)
{
  int i,rc;
  struct __ipsec_session_stateType *ssp, *ssp2;
  struct sockaddr_in *sin4;
  struct sockaddr_in6 *sin6;
  uint8_t pkbuf[MAX_DATA_SIZE];
  size_t pkbuf_len = 0;


  ssp = (struct __ipsec_session_stateType *)malloc0(sizeof(*ssp));
  ssp->callid = 0x1122334455667788;
  ssp->policy_objid = 0x8877665544332211;

  {
    ssp->ikesa = (struct __ikesaType *) malloc0(sizeof(*ssp->ikesa));
    ssp->ikesa->local_ipaddr = (struct sockaddr_storage *)malloc0(sizeof(*ssp->ikesa->local_ipaddr));
    ssp->ikesa->local_ipaddr->ss_family = AF_INET;
    sin4 = (struct sockaddr_in *)ssp->ikesa->local_ipaddr;
    rc = inet_pton(AF_INET,"1.2.3.4",&sin4->sin_addr);
    ASSERT(1 == rc);

    ssp->ikesa->remote_ipaddr = (struct sockaddr_storage *)malloc0(sizeof(*ssp->ikesa->remote_ipaddr));
    ssp->ikesa->remote_ipaddr->ss_family = AF_INET6;
    sin6 = (struct sockaddr_in6 *)ssp->ikesa->remote_ipaddr;
    rc = inet_pton(AF_INET6,"0010:0203:0405:0607::0809",&sin6->sin6_addr);
    ASSERT(1 == rc);

    ssp->ikesa->ike_cookie = 123456;
    ssp->ikesa->state = __IKESA_STATE__IKESA_STATE_ACTIVE;
  }
  
#define NSA 7
  ssp->ipsecsa.vec_len = NSA;
  ssp->ipsecsa.vec = (struct __ipsecsaType **)malloc0(NSA * sizeof(ssp->ipsecsa.vec[0]));
  for(i=0;i<NSA;i++) {
    ssp->ipsecsa.vec[i] = (struct __ipsecsaType *)malloc0(sizeof(*ssp->ipsecsa.vec[0]));
    ssp->ipsecsa.vec[i]->my_objid = i*10000;
    ssp->ipsecsa.vec[i]->local_ipaddr = (struct sockaddr_storage *)malloc0(sizeof(*ssp->ipsecsa.vec[i]->local_ipaddr));
    sin4 = (struct sockaddr_in *)ssp->ipsecsa.vec[i]->local_ipaddr;
    sin4->sin_family = AF_INET;
    rc = inet_pton(AF_INET,"5.6.7.8",&sin4->sin_addr);
    ASSERT(1 == rc);
    ssp->ipsecsa.vec[i]->remote_ipaddr = (struct sockaddr_storage *)malloc0(sizeof(*ssp->ipsecsa.vec[i]->remote_ipaddr));
    sin6 = (struct sockaddr_in6 *)ssp->ipsecsa.vec[i]->remote_ipaddr;
    sin6->sin6_family = AF_INET6;
    rc = inet_pton(AF_INET6,"1111:2222:3333:4444::9999",&sin6->sin6_addr);
    ASSERT(1 == rc);

    ssp->ipsecsa.vec[i]->ike_cookie = i;
    ssp->ipsecsa.vec[i]->mode = __IPSEC_MODE__IPSEC_MODE_TRANSPORT_MODE;
    ssp->ipsecsa.vec[i]->cipher = __IPSEC_CIPHER__IPSEC_CIPHER_AES256_CIPHER;
    ssp->ipsecsa.vec[i]->hmac = __IPSEC_HMAC__IPSEC_HMAC_SHA1_HMAC;
  }


  // C-struct -> PACK
  rc = PB2CM_pack_struct__ipsec_session_stateType(ssp,pkbuf,sizeof(pkbuf));
  ASSERT(rc > 0);
  pkbuf_len = rc;


  // PACK -> C-struct
  ssp2 = PB2CM_unpack_struct__ipsec_session_stateType(NULL,pkbuf,pkbuf_len);
  ASSERT(ssp2);

  ASSERT(ssp->ipsecsa.vec_len == ssp2->ipsecsa.vec_len);
  ASSERT(ssp->callid == ssp2->callid);
  ASSERT(ssp->policy_objid == ssp2->policy_objid);
  ASSERT(ssp->ipsecsa.vec_len == ssp2->ipsecsa.vec_len);

  // cleanup msg2
  PB2C_DEEP_FREE_struct__ipsec_session_stateType(ssp,1);
  PB2C_DEEP_FREE_struct__ipsec_session_stateType(ssp2,1);
}

void
reflection_test(void)
{
  const ProtobufCMessageDescriptor *pbmd = &msg2__descriptor;
  void *vmp; // Actually Msg2
  const ProtobufCFieldDescriptor *fd, *fd2;
  const ProtobufCEnumValue *ev;
  struct __msg2 *m2p;

  ((void)fd);
  ((void)fd2);
  ((void)ev);
  ((void)m2p);

  ASSERT(pbmd->sizeof_message == sizeof(Msg2));
  vmp = malloc0(pbmd->sizeof_message);  
  ASSERT(vmp);
#if 0
  ASSERT(pbmd->message_init == (void *)msg2__init);
#endif
  protobuf_c_message_init(pbmd, (ProtobufCMessage *)vmp);
  ASSERT(((Msg2 *)vmp)->base.descriptor == pbmd);
  fd = protobuf_c_message_descriptor_get_field_by_name(pbmd,"a");
  ASSERT(fd);
  ASSERT(fd->type == PROTOBUF_C_TYPE_INT32);
  ASSERT(fd->quantifier_offset == 0); // REQUIRED

  fd = protobuf_c_message_descriptor_get_field_by_name(pbmd,"b");
  ASSERT(fd);
  ASSERT(fd->type == PROTOBUF_C_TYPE_INT32);
  ASSERT(fd->quantifier_offset == offsetof(Msg2,has_b)); // OPTIONAL


  fd = protobuf_c_message_descriptor_get_field_by_name(pbmd,"d");
  ASSERT(fd);
  ASSERT(fd->type == PROTOBUF_C_TYPE_ENUM);
  ev = protobuf_c_enum_descriptor_get_value_by_name ((const ProtobufCEnumDescriptor    *)fd->descriptor,"EN1");
  ASSERT(ev);
  ASSERT(!strcmp(ev->name,"EN1") &&
	 !strcmp(ev->c_name,"EN1") && 
	 (ev->value == 1));
  fd = protobuf_c_message_descriptor_get_field_by_name(pbmd,"e");
  ASSERT(fd);
  ASSERT(fd->type == PROTOBUF_C_TYPE_STRING);
  ASSERT(fd->label == PROTOBUF_C_LABEL_REPEATED);
  ASSERT(fd->quantifier_offset == offsetof(Msg2,n_e)); // REPEATED

  fd2 = protobuf_c_message_descriptor_get_field(pbmd,5);
  ASSERT(fd2);
  ASSERT(fd == fd2);

  fd2 = protobuf_c_message_descriptor_get_field(pbmd,11);
  ASSERT(NULL == fd2); // No such field
  
  m2p = PB2CM_Msg2_TO_struct__msg2((Msg2 *)vmp,NULL);
  ASSERT(m2p);
  msg2__free_unpacked(&protobuf_c_default_instance,(Msg2 *)vmp);
  PB2C_DEEP_FREE_struct__msg2(m2p,1);
}
/*
 * Declare type to hold auto-generated #defines
 * that can be used to register Protobuf types
 * using the API xml2cstruct_addtype()
 */
typedef struct _xml2cstruct_tinfo {
  const ProtobufCMessageDescriptor *pb_desc; /* Protobuf descriptor */
  void *(*pb2c_fn)(void *, void *);          /* Protobuf to C-struct converter function */
  void (*pb_free_fn)(void *, void *);        /* Protobuf free fn */
  void *pb_free_fn_arg;
} xml2cstruct_tinfo_t;



/*
 * Table of top-level message types to register
 */
xml2cstruct_tinfo_t msg_defs[] =
  {
#ifdef TIM_FLAT
    PB2C_XML2CSTRUCT_DEF_FLAT_TIM_PROTO
#else
    PB2C_XML2CSTRUCT_DEF_TIM_PROTO
#endif
    PB2C_XML2CSTRUCT_DEF_EX1_PROTO
    PB2C_XML2CSTRUCT_DEF_FLAT_SAMPLE1_PROTO
  };



void
msgtype_test(void)
{
#if 0  
  uint32_t i;
  int rc;
  ProtobufCMessage *pbcm, *inner_pbcm;
  struct __msg3 *msg3_c;
  Msg3 *pb_msg3;
  struct __msg4 *msg4_c;
  Msg4 *pb_msg4;
  msg_type_t *msgtypehandle;
  xml2cstruct_instance_t *instance = xml2cstruct_instance_new();
  ASSERT(instance);

  ((void)rc);
  ((void)pb_msg3);
  ((void)pb_msg4);

  /*
   * Register ProtoBuf message types using the auto-generated #define(s)
   * stuffed into the array msg_defs[] defined above
   */
  for(i=0;i<sizeof(msg_defs)/sizeof(msg_defs[0]);i++) {
    rc = xml2cstruct_addtype(instance,
			     msg_defs[i].pb_desc,
			     msg_defs[i].pb2c_fn,
			     msg_defs[i].pb_free_fn,
			     &protobuf_c_default_instance);
    ASSERT(0 == rc);
  }

  // Test all basic types using msg3
  pbcm = xml2cstruct_pballoc(instance,(char *)"msg3",&msgtypehandle);
  ASSERT(pbcm);
  pb_msg3 = (Msg3 *)pbcm;
  ASSERT(NULL != pb_msg3);
  rc = xml2cstruct_set_attr_by_index(pbcm,1,(char *)"2.0");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_name(pbcm,(char *)"b",(char *)"10.0");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,3,(char *)"33");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,4,(char *)"-44");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,5,(char *)"555");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,6,(char *)"6666");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,7,(char *)"-77");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,8,(char *)"-888");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,9,(char *)"999");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,10,(char *)"1010");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_name(pbcm,(char *)"k",(char *)"1111");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,12,(char *)"-1212");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,13,(char *)"true");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,13,(char *)"false");
  ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,14,(char *)"this is a string");
  ASSERT(0 == rc);
  //bytes ZZZ
  //rc = xml2cstruct_set_attr_by_index(pbcm,15,(char *)"BYTES");
  //ASSERT(0 == rc);
  rc = xml2cstruct_set_attr_by_index(pbcm,16,(char *)"EN2");
  ASSERT(0 == rc);
  // Test some repeated types
  {
    int i;
    char str[32];
    for(i=1;i<14;i++) {
      sprintf(str,"%d",i);
      rc = xml2cstruct_set_attr_by_name(pbcm,(char *)"r",str); //int32
      ASSERT(0 == rc);
      rc = xml2cstruct_set_attr_by_name(pbcm,(char *)"s",str); //int64
      ASSERT(0 == rc);
      rc = xml2cstruct_set_attr_by_name(pbcm,(char *)"t",str); //string
      ASSERT(0 == rc);
    }
  }
  // Convert Protobuf to C-struct and release the ProtoBuf
  msg3_c = (struct __msg3 *) xml2cstruct_pb_to_c(pbcm,msgtypehandle);
  ASSERT(NULL != msg3_c);
  // Free the C-struct
  PB2C_DEEP_FREE_struct__msg3(msg3_c,1);


  // Test sub-messages using msg4 from ex1.proto
  pbcm = xml2cstruct_pballoc(instance,(char *)"msg4",&msgtypehandle);
  ASSERT(pbcm);
  pb_msg4 = (Msg4 *)pbcm;
  ASSERT(NULL != pb_msg4);
  rc = xml2cstruct_set_attr_by_name(pbcm,(char *)"i1",(char *)"22");
  ASSERT(0 == rc);
  inner_pbcm = xml2cstruct_submsg_by_index(pbcm,4); //optional "foo"
  ASSERT(NULL != inner_pbcm);
  rc = xml2cstruct_set_attr_by_name(inner_pbcm,(char *)"n",(char *)"junko");
  ASSERT(0 == rc);  
  inner_pbcm = xml2cstruct_submsg_by_name(pbcm,(char *)"moo"); //required "moo"
  ASSERT(NULL != inner_pbcm);
  rc = xml2cstruct_set_attr_by_name(inner_pbcm,(char *)"i2",(char *)"22222");
  ASSERT(0 == rc);  

  // test repeated msg "doo"
  {
    int i;
    char str[32];
    for(i=0;i<23;i++) {
      sprintf(str,"%d",i);
      inner_pbcm = xml2cstruct_submsg_by_name(pbcm,(char *)"doo"); //repeated "doo"
      ASSERT(NULL != inner_pbcm);
      rc = xml2cstruct_set_attr_by_name(inner_pbcm,(char *)"c",str);
      ASSERT(0 == rc);
      rc = xml2cstruct_set_attr_by_index(inner_pbcm,4,(char *)"EN2");
      ASSERT(0 == rc);
    }
  }
  inner_pbcm = xml2cstruct_submsg_by_name(pbcm,(char *)"zoo"); //required "zoo"
  // Convert Protobuf to C-struct and release the ProtoBuf
  msg4_c = (struct __msg4 *)xml2cstruct_pb_to_c(pbcm,msgtypehandle);
  ASSERT(NULL != msg4_c);
  // Free the C-struct
  PB2C_DEEP_FREE_struct__msg4(msg4_c,1);


  // Cleanup the instance
  xml2cstruct_instance_del(instance);
#endif  
}

void options_test(void) {
  int rc;
  uint8_t pkbuf[MAX_DATA_SIZE];
  struct __m6 m6_c;
  struct __m6noopt m6noopt_c;

  ((void)rc);

  memset(&m6_c, 0, sizeof(m6_c));
  memset(&m6noopt_c, 0, sizeof(m6noopt_c));

  m6_c.i1 = 11;
  m6_c.s1 = strdup("s1");
  memcpy(&m6_c, &m6noopt_c, sizeof(m6_c));

  if (m6__descriptor.rw_flags & RW_PROTOBUF_MOPT_FLAT) {
    printf("m6__descriptor.rw_flags => flat...\n");
  }
  if (m6__descriptor.rw_flags & RW_PROTOBUF_MOPT_COMP) {
    printf("m6__descriptor.rw_flags => comp...\n");
  }

  rc = PB2CM_pack_struct__m6(&m6_c, pkbuf, sizeof(pkbuf));
  int rc2 = PB2CM_pack_struct__m6noopt(&m6noopt_c, pkbuf, sizeof(pkbuf));
  ((void)rc2);
  ASSERT(rc == rc2 || rc != rc2/*meh*/);
  free(m6_c.s1);

  M6 m6msg;
  m6__init(&m6msg);

  if (m6msg.base.descriptor->rw_flags & RW_PROTOBUF_MOPT_FLAT) {
    printf("M6 msg descriptor rw_flags => flat\n");
  }




}

static void myapp_bar1(Foo_Service *srv, const Msg1 *input, Msg2_Closure closure, void *clodat) {
  ((void)srv);
  ((void)input);
  printf("myapp_bar1 invoked\n");
  Msg2 rsp /*= MSG2__INIT*/;
  msg2__init(&rsp);
  closure(&rsp, clodat);
}
static void myapp_bar2(Foo_Service *srv, const Msg2 *input, Msg3_Closure closure, void *clodat) {
  ((void)srv);
  ((void)input);
  printf("myapp_bar2 invoked\n");
  Msg3 rsp /*= MSG3__INIT*/;
  msg3__init(&rsp);
  closure(&rsp, clodat);

  /* This is slightly annoying for users who take a while to answer.
     Absent other information, they to have to hang onto the input
     Msg, the closure, and the closure data.  In practice the clodat
     is always the rwmsg_request_t, and the closure function is the
     same.  Ditching the redundant arguments would require separating
     out the client and server Foo_Service entirely in protoc-c's
     c_server.cc and protobuf-c.[ch].  Probably not worth it at this
     time, not least because in practice most requests are just
     answered directly. */
}

struct bar2clodat {
  int i;
};

// -Wall: defined but not used
//static void myapp_bar2rsp(const Msg3 *rsp, void *clodat) {
//  printf("myapp_bar2rsp invoked\n");
//}

// -Wall: defined but not used
//static void myapp_bar2rsp2(const Msg3 *rsp, rwmsg_request_t req, void *clodat) {
//  printf("myapp_bar2rsp2 invoked\n");
//}

#if 0
static void rwmsg_generic_clihandler_methidx_0(ProtobufCService *s,
					       rwmsg_destination_t dest,
					       const ProtobufCMessage *in,
					       ProtobufCClosure clo,
					       void *clodat) {
  /* Supposed to make a req and call rwmsg send on the channel in
     s->rw_context.  This is for method index 0 in the service
     descriptor, NOT for method NUMBER 0 as declared on the wire. */
  printf("rwmsg_generic_clihandler_methidx_1 invoked\n");
}
static void rwmsg_generic_clihandler_methidx_1(ProtobufCService *s,
					       rwmsg_destination_t dest,
					       const ProtobufCMessage *in,
					       ProtobufCClosure clo,
					       void *clodat) {
  printf("rwmsg_generic_clihandler_methidx_1 invoked\n");
}
#endif

struct rwmsg_request_t_placeholder {
  ProtobufCMessage *reqmsg;
  // rwmsg_srvchan_t *srvchan;
  // ...
};

static void rwmsg_shuttlersp(ProtobufCMessage *rsp, struct rwmsg_request_t_placeholder *req) {
  ((void)rsp);
  printf("rwmsg_shuttlersp invoked\n");

  if (req->reqmsg) {
    protobuf_c_message_free_unpacked(&protobuf_c_default_instance, req->reqmsg);
  }

  /* Serialize rsp and call rwmsg_request_send_response(req, rspprotobuf, rspprotobuf_len);  Error handling??  TBD */

  //...

  /* Normally the rwmsg_request would survive until it is completely
     written to the local broker or in-process requesting task */
  free(req);
}

struct rwmsg_closure_s { void *cb; void *ud; };

rw_status_t rwmsg_clichan_send_protoc_internal(void *rc, rwmsg_destination_t *dst, uint32_t flg, ProtobufCService *srv, uint32_t idx, ProtobufCMessage *in, rwmsg_closure_t *clo, rwmsg_request_t **reqout) {
  ((void)dst);
  ((void)flg);
  ((void)srv);
  ((void)idx);
  ((void)in);
  ((void)clo);
  ((void)reqout);
  ((void)rc);
  printf("rwmsg_clichan_send_protoc_internal invoked\n");
  return RW_STATUS_SUCCESS;
}

void service_test(void) {

  /* M6 tests.  M6 is a message declared several times.  All are
     identical in terms of classical protobuf syntax, and therefore
     all encode the same on the wire.  However each has different
     rift-proto-c options used such that the C language structure
     vary.

     There is some hassle with detection of sub messages having
     default values or not.  So they tend to be transmitted and
     unpacked.  In the absence of disagreement about what the default
     values are this is mostly just wasted time.  

   */

  M6flat flatmess;
  m6flat__init(&flatmess);

  flatmess.has_m2 = TRUE;

  ASSERT(!flatmess.has_r_opt_nodefault);
  ASSERT(flatmess.r_opt == 311);
  if (flatmess.has_m1) {
    ASSERT(flatmess.m1.c);
  }
  ASSERT(0==strcmp(flatmess.m1_req.c, "ccc")); // required
  ASSERT(flatmess.n_m1_rep == 0);		   // repeated
  ASSERT(0==strcmp(flatmess.s1, "s1def"));
  ASSERT(0==strcmp(flatmess.s1_req, "s1_reqdef"));
  ASSERT(0==flatmess.n_s1_rep);
  ASSERT(0==strlen(flatmess.s1_rep[0]));
  ASSERT(0==strlen(flatmess.s1_rep[1]));
  ASSERT(0==strlen(flatmess.s1_rep[2]));

  flatmess.r[flatmess.n_r++] = 100;
  flatmess.r[flatmess.n_r++] = 101;
  ASSERT(flatmess.n_r == 2);

  ASSERT(flatmess.m2.a == 99);
  ASSERT(flatmess.m2.b == 999);
  ASSERT(flatmess.m2.f);
  ASSERT(0==strcmp(flatmess.m2.f, "ffff"));

  flatmess.m2.a = 1234;
  flatmess.i1 = 1111;
  strcat(flatmess.s1_rep[flatmess.n_s1_rep++], "s1_rep[0]");
  strcat(flatmess.s1_rep[flatmess.n_s1_rep++], "s1_rep[1]");

  ASSERT(flatmess.byt_req.len == strlen("byt_req_def"));
  ASSERT(0==memcmp(flatmess.byt_req.data, "byt_req_def", 11));
  flatmess.has_byt_opt = TRUE;
  flatmess.byt_opt.len = 7;
  strcat((char*)flatmess.byt_opt.data, "optval");
  flatmess.byt_rep[0].len = 5;
  strcat((char*)flatmess.byt_rep[0].data, "val0");
  flatmess.byt_rep[1].len = 6;
  strcat((char*)flatmess.byt_rep[1].data, "val_1");
  flatmess.n_byt_rep = 2;

  uint8_t buf[1000];
  int buf_len=0;

  {
    ProtobufCBufferSimple pbuf = PROTOBUF_C_BUFFER_SIMPLE_INIT(&protobuf_c_default_instance, buf);
    buf_len = m6flat__pack_to_buffer(&flatmess, (ProtobufCBuffer*)&pbuf);
    printf("M6flat flatmess packed in buffer len=%d\n", buf_len);
    ASSERT(buf_len);

    /* Decode as a flat message */
    M6flat *m6copy = (M6flat*)protobuf_c_message_unpack(&protobuf_c_default_instance, flatmess.base.descriptor, buf_len, buf);
    ASSERT(m6copy);

    ASSERT(!m6copy->has_r_opt_nodefault);
    ASSERT(m6copy->r_opt == 311);
    if (m6copy->has_m1) {
      ASSERT(m6copy->m1.c);
      ASSERT(0==strcmp(m6copy->m1.c, "ccc")); // optional
    }
    ASSERT(0==strcmp(m6copy->m1_req.c, "ccc")); // required
    ASSERT(m6copy->n_m1_rep == 0);		   // repeated
    ASSERT(0==strcmp(m6copy->s1, "s1def"));
    ASSERT(0==strcmp(m6copy->s1_req, "s1_reqdef"));
    ASSERT(2==m6copy->n_s1_rep);
    ASSERT(0==strcmp(m6copy->s1_rep[0], "s1_rep[0]"));
    ASSERT(0==strcmp(m6copy->s1_rep[1], "s1_rep[1]"));
    ASSERT(0==strlen(m6copy->s1_rep[2]));
    ASSERT(2 == m6copy->n_r);
    ASSERT(100 == m6copy->r[0]);
    ASSERT(101 == m6copy->r[1]);
    ASSERT(m6copy->i1 == 1111);
    if (m6copy->has_m2) {
      ASSERT(m6copy->m2.a == 1234);
      ASSERT(m6copy->m2.b == 999);
      ASSERT(m6copy->m2.f);
      ASSERT(0==strcmp(m6copy->m2.f, "ffff"));
    }

    ASSERT(m6copy->n_byt_rep == 2);
    ASSERT(m6copy->byt_rep[0].len == 5);
    ASSERT(0==memcmp(m6copy->byt_rep[0].data, "val0", 5));
    ASSERT(m6copy->byt_rep[1].len == 6);
    ASSERT(0==memcmp(m6copy->byt_rep[1].data, "val_1", 6));

    ASSERT(m6copy->byt_req.len == 11);
    ASSERT(0==memcmp(m6copy->byt_req.data, "byt_req_def", 11));

    ASSERT(m6copy->byt_opt.len == 7);
    ASSERT(0==memcmp((char*)m6copy->byt_opt.data, "optval", 7));

    m6flat__free_unpacked(&protobuf_c_default_instance, m6copy);


    /* Decode this as a totally not flat message */
    M6nonflat *m6non = (M6nonflat*)protobuf_c_message_unpack(&protobuf_c_default_instance, &m6nonflat__descriptor, buf_len, buf);
    ASSERT(m6non->i1 == 1111);
    ASSERT(m6non->r_opt == 311);
    if (m6non->m1) {
      ASSERT(m6non->m1->c);
      ASSERT(0==strcmp(m6non->m1->c, "ccc")); // optional
    }
    ASSERT(m6non->m1_req); // required
    ASSERT(0==strcmp(m6non->m1_req->c, "ccc")); // required
    if (m6non->m2) {
      ASSERT(m6non->m2->a == 1234);
      ASSERT(m6non->m2->b == 999);
      ASSERT(0==strcmp(m6non->m2->f, "ffff"));
    }
    ASSERT(2==m6non->n_s1_rep);
    ASSERT(0==strcmp(m6non->s1_rep[0], "s1_rep[0]"));
    ASSERT(0==strcmp(m6non->s1_rep[1], "s1_rep[1]"));
      ASSERT(2 == m6non->n_r);
    ASSERT(100 == m6non->r[0]);
    ASSERT(101 == m6non->r[1]);
    ASSERT(m6non->n_byt_rep == 2);
    ASSERT(m6non->byt_rep[0].len == 5);
    ASSERT(0==memcmp(m6non->byt_rep[0].data, "val0", 5));
    ASSERT(m6non->byt_rep[1].len == 6);
    ASSERT(0==memcmp(m6non->byt_rep[1].data, "val_1", 6));
    ASSERT(m6non->byt_req.len == 11);
    ASSERT(0==memcmp(m6non->byt_req.data, "byt_req_def", 11));
    ASSERT(m6non->byt_opt.len == 7);
    ASSERT(0==memcmp((char*)m6non->byt_opt.data, "optval", 7));

    m6nonflat__free_unpacked(&protobuf_c_default_instance, m6non);

    /* Decode this as a partly flat message */
    M6partflat *m6p = (M6partflat*)protobuf_c_message_unpack(&protobuf_c_default_instance, &m6partflat__descriptor, buf_len, buf);
    ASSERT(m6p->i1 == 1111);
    ASSERT(m6p->r_opt == 311);
    if (m6p->has_m1) {
      ASSERT(m6p->m1.c);			      // inline optional
      ASSERT(0==strcmp(m6p->m1.c, "ccc"));      // inline optional
    }
    ASSERT(0==strcmp(m6p->m1_req.c, "ccc"));  // inline optional
    if (m6p->m2) {
      ASSERT(m6p->m2);			 // not inline, optional
      ASSERT(m6p->m2->a == 1234);
      ASSERT(m6p->m2->b == 999);
      ASSERT(0==strcmp(m6p->m2->f, "ffff"));
    }
    ASSERT(2==m6p->n_s1_rep);
    ASSERT(0==strcmp(m6p->s1_rep[0], "s1_rep[0]"));
    ASSERT(0==strcmp(m6p->s1_rep[1], "s1_rep[1]"));
    ASSERT(0==strlen(m6p->s1_rep[2]));
    ASSERT(2 == m6p->n_r);
    ASSERT(100 == m6p->r[0]);
    ASSERT(101 == m6p->r[1]);
    ASSERT(m6p->n_byt_rep == 2);
    ASSERT(m6p->byt_rep[0].len == 5);
    ASSERT(0==memcmp(m6p->byt_rep[0].data, "val0", 5));
    ASSERT(m6p->byt_rep[1].len == 6);
    ASSERT(0==memcmp(m6p->byt_rep[1].data, "val_1", 6));
    ASSERT(m6p->byt_req.len == 11);
    ASSERT(0==memcmp(m6p->byt_req.data, "byt_req_def", 11));
    ASSERT(m6p->has_byt_opt);
    ASSERT(m6p->byt_opt.len == 7);
    ASSERT(0==memcmp((char*)m6p->byt_opt.data, "optval", 7));

    m6partflat__free_unpacked(&protobuf_c_default_instance, m6p);
    m6p=NULL;

    /* Decode a message into our own memory (except for any non-inline parts) */
    M6partflat m6phere;
    m6partflat__init(&m6phere);
    m6p = m6partflat__unpack_usebody(&protobuf_c_default_instance, buf_len, buf, &m6phere);
    ASSERT(m6p);
    ASSERT(m6p == &m6phere);
    ASSERT(m6p->i1 == 1111);
    ASSERT(m6p->r_opt == 311);
    if (m6p->has_m1) {
      ASSERT(m6p->m1.c);			      // inline optional
      ASSERT(0==strcmp(m6p->m1.c, "ccc"));      // inline optional
    }
    ASSERT(0==strcmp(m6p->m1_req.c, "ccc"));  // inline optional
    if (m6p->m2) {
      ASSERT(m6p->m2);			 // not inline, optional
      ASSERT(m6p->m2->a == 1234);
      ASSERT(m6p->m2->b == 999);
      ASSERT(0==strcmp(m6p->m2->f, "ffff"));
    }
    ASSERT(2==m6p->n_s1_rep);
    ASSERT(0==strcmp(m6p->s1_rep[0], "s1_rep[0]"));
    ASSERT(0==strcmp(m6p->s1_rep[1], "s1_rep[1]"));
    ASSERT(0==strlen(m6p->s1_rep[2]));
    ASSERT(2 == m6p->n_r);
    ASSERT(100 == m6p->r[0]);
    ASSERT(101 == m6p->r[1]);
    ASSERT(m6p->n_byt_rep == 2);
    ASSERT(m6p->byt_rep[0].len == 5);
    ASSERT(0==memcmp(m6p->byt_rep[0].data, "val0", 5));
    ASSERT(m6p->byt_rep[1].len == 6);
    ASSERT(0==memcmp(m6p->byt_rep[1].data, "val_1", 6));
    ASSERT(m6p->byt_req.len == 11);
    ASSERT(0==memcmp(m6p->byt_req.data, "byt_req_def", 11));
    ASSERT(m6p->has_byt_opt);
    ASSERT(m6p->byt_opt.len == 7);
    ASSERT(0==memcmp((char*)m6p->byt_opt.data, "optval", 7));
    m6partflat__free_unpacked_usebody(&protobuf_c_default_instance, &m6phere);

  }

  void *srvchan = NULL;
  Foo_Service *myapisrv = (Foo_Service*)malloc(sizeof(*myapisrv));
  FOO__INITSERVER(myapisrv, myapp_);
  myapisrv->base.rw_context = srvchan;  // normally done in rwmsg_srvchan_addservice

  void *clichan = NULL;
  Foo_Client myapicli;
  FOO__INITCLIENT(&myapicli);
  myapicli.base.rw_context = clichan;   // normally done in rwmsg_clichan_addservice

  /* Note that the clichan and thus this Foo_Service is bound to a
     specific destination.  Want another destination for foo methods,
     make another Foo_Service client instance. */

  Msg2 msg2;

  /* Rift-themed generated code provides this */
  rwmsg_request_t *rsp;
  rw_status_t status = foo__bar2_b(&myapicli, NULL/*dest*/, &msg2, &rsp);
  (void)status; // -Wall: variable 'status' set but not used
  /* status tells us if we even got a response flow control let the thing out?  is this clichan a dud?  etc */
  /* rwmsg_..._unpack_protomsg(rsp, msg3); */

  /* Rift-themed generated code provides this */
  rwmsg_closure_t rclo = { NULL, NULL };
  rwmsg_request_t *req;
  status = foo__bar2(&myapicli, NULL/*dest*/, &msg2, &rclo, &req);
  /* status tells us did flow control let the thing out?  is this clichan a dud?  etc */


  /***********/

  /* This is what the rwmsg srvchan plumbing calls to invoke service
     handlers on each arriving request.  The service contains an array
     of pointers after the base indexed with method_idx (not methno),
     so the srvchanl would do a methno->methidx lookup and call
     foo__bar2 by pointer out of this array of handlers. */

  /* We're going to pretend buf2 is something we just read from the
     socket, and figured out the service+method number from the header
     that arrived earlier. */
  {
    /* Fill in buf2 with serialized message payload */
    Msg2 pbreq /*= MSG2__INIT*/;
    msg2__init(&pbreq);
    ProtobufCBufferSimple pbuf = PROTOBUF_C_BUFFER_SIMPLE_INIT(&protobuf_c_default_instance, buf);
    buf_len = msg2__pack_to_buffer(&pbreq, (ProtobufCBuffer*)&pbuf);
    printf("fake req Msg2 packed up in buffer, len=%d\n", buf_len);
  }
  ASSERT(buf_len);

  unsigned methno = 2;		/* from rwmsg header, as would be
				   service number, to be looked up
				   among the service(s) bound to the
				   rwmsg_srvchannel */
  int methidx=-1; /* not methno! */
  /* lookup methidx from methno */
  for (unsigned i=0; i<myapisrv->base.descriptor->n_methods; i++) {
    if (myapisrv->base.descriptor->methods[i].rw_methno == methno) {
      methidx = i;
      break;
    }
  }
  assert(methidx > -1);

  /* How to allocate and init a generic ProtobufCMessage of the proper size */
  if (0) {
    /* The init routine does plug in defaults.  Unpack allocates and calls init for us */
    size_t msgsize = myapisrv->base.descriptor->methods[methidx].input->sizeof_message;
    ProtobufCMessage *arrivingmsg = (ProtobufCMessage*)malloc/*notzero!*/(msgsize);
    protobuf_c_message_init(myapisrv->base.descriptor->methods[methidx].input, arrivingmsg);
    ASSERT(arrivingmsg->descriptor);
  }

  /* The unpack calls _init first thing, so defaults do appear, and no
     pre-zeroing is needed.  It can handle working with a protobuf
     descriptor in isolation, although the normal case invokes a
     generated initializer function (pointed to by the protobuf C
     bindings' descriptor structure) rather than the data-driven
     generic initializer. */
  ProtobufCMessage *arrivingmsg;
  arrivingmsg = protobuf_c_message_unpack(&protobuf_c_default_instance, 
					  myapisrv->base.descriptor->methods[methidx].input, 
					  buf_len, 
					  buf);
  
  /* Find the service handler and invoke it with our generic closure
     callback.  Probably need a bit of glue or a bodge of some sort to
     provide a pleasing API to users instead of the callback argument
     stuff. Not least to get explicit error returns back. */
  typedef void (*GenericHandler) (void *service,
				  const ProtobufCMessage *input,
				  ProtobufCClosure closure,
				  void *closure_data);
  GenericHandler *handlers = (GenericHandler *) (&myapisrv->base + 1);
  ASSERT(handlers[methidx] == (GenericHandler)myapp_bar2);

  struct rwmsg_request_t_placeholder *rwreq = (struct rwmsg_request_t_placeholder*)malloc0(sizeof(*rwreq));
  *rwreq = { .reqmsg = arrivingmsg };
  handlers[methidx](myapisrv, (ProtobufCMessage*)&msg2, (ProtobufCClosure)rwmsg_shuttlersp, rwreq);


  free(myapisrv);
}

int
main(int argc, char **argv)
{

  ((void)argc);
  ex1_tests();

  ipsec_test();

  reflection_test();

  msgtype_test();

  options_test();

  service_test();

  printf("\n");
  printf("ALL TESTS PASS (%s) \n",argv[0]);

  exit(0);
}
