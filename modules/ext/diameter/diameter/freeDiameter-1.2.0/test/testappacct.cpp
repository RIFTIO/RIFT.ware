/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2013, WIDE Project and NICT								 *
* All rights reserved.											 *
* 													 *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						 *
* 													 *
* * Redistributions of source code must retain the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer.										 *
*    													 *
* * Redistributions in binary form must reproduce the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer in the documentation and/or other						 *
*   materials provided with the distribution.								 *
* 													 *
* * Neither the name of the WIDE Project or NICT nor the 						 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of WIDE Project and 						 *
*   NICT.												 *
* 													 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	 *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	 *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								 *
*********************************************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
#include "../tests/tests.h"
#ifdef __cplusplus
}
#endif

#include "rwut.h"
using ::testing::AtLeast;
*

/* The connection string to the database */
#ifndef TEST_CONNINFO
#error "Please specify the conninfo information"
#endif /* TEST_CONNINFO */

/* The table used for tests. This table will receive the following instructions:
DROP TABLE <table>;
CREATE TABLE <table>
(
  recorded_on timestamp with time zone NOT NULL,
  "Accounting-Record-Type" integer,
  "Session-Id" bytea,
  "Accounting-Record-Number" integer,
  "Route-Record1" bytea,
  "Route-Record2" bytea,
  "Route-Record3" bytea,
  "Route-Record4" bytea
);
*/
#define TABLE "incoming_test"

#include "../extensions/app_acct/app_acct.h"
#include <libpq-fe.h>

static int add_avp_in_conf(char * avpname, int multi) 
{
	struct acct_conf_avp *vnew;
	struct dict_object * dict;
	struct dict_avp_data dictdata;

	/* Validate the avp name first */
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, avpname, &dict, ENOENT) );
	CHECK_FCT( fd_dict_getval( dict, &dictdata ));

	/* Create a new entry */
	CHECK_MALLOC( vnew = ( acct_conf_avp *) malloc(sizeof(struct acct_conf_avp)) );
	memset(new, 0, sizeof(struct acct_conf_avp));
	fd_list_init(&vnew->chain, NULL);
	vnew->avpname = avpname;
	vnew->avpobj = dict;
	vnew->avptype = dictdata.avp_basetype;
	vnew->multi = multi;

	/* Add this new entry at the end of the list */
	fd_list_insert_before( &acct_config->avps, &vnew->chain );
	
	return 0;
}

#define LOCAL_ID	"test.app.acct"
#define LOCAL_REALM	"app.acct"

	extern pthread_key_t connk; /* in acct_db.c */
static	PGconn *conn;
	extern int fd_ext_init(int major, int minor, char * conffile); /* defined in include's extension.h */
	extern void fd_ext_fini(void); /* defined in the extension itself */
static	struct msg * msg;
static	os0_t sess_bkp;
static	size_t sess_bkp_len;

TEST(RwDiameter, DiameterAcct)
{
	fd_g_config->cnf_diamid = strdup(LOCAL_ID);
	fd_g_config->cnf_diamid_len = CONSTSTRLEN(LOCAL_ID);
	fd_g_config->cnf_diamrlm = strdup(LOCAL_REALM);
	fd_g_config->cnf_diamrlm_len = CONSTSTRLEN(LOCAL_REALM);
	
	EXPECT_EQ( 0, fd_queues_init()  );
	EXPECT_EQ( 0, fd_msg_init()  );
	EXPECT_EQ( 0, fd_rtdisp_init()  );
	
	{
		EXPECT_EQ( 0, acct_conf_init() );
		acct_config->conninfo = strdup(TEST_CONNINFO);
		acct_config->tablename = strdup(TABLE);
		acct_config->tsfield = strdup("recorded_on");
		EXPECT_EQ( 0, add_avp_in_conf(strdup("Session-Id"), 0) );
		EXPECT_EQ( 0, add_avp_in_conf(strdup("Accounting-Record-Type"), 0) );
		EXPECT_EQ( 0, add_avp_in_conf(strdup("Accounting-Record-Number"), 0) );
		EXPECT_EQ( 0, add_avp_in_conf(strdup("Route-Record"), 4) );
		
		/* Now, call the one of the extension */
		EXPECT_EQ( 0, fd_ext_init(FD_PROJECT_VERSION_MAJOR, FD_PROJECT_VERSION_MINOR,NULL) );
		conn = pthread_getspecific(connk);
	}
	
	/* Drop and recreate the table for the test */
	{
		PGresult * res;
		EXPECT_EQ( CONNECTION_OK, PQstatus(conn) );
		
		res = PQexec(conn, "DROP TABLE " TABLE ";");
		EXPECT_EQ( PGRES_COMMAND_OK, PQresultStatus(res) );
		PQclear(res);
		
		res = PQexec(conn, "CREATE TABLE " TABLE " ( "
					"  recorded_on timestamp with time zone NOT NULL, "
					"  \"Accounting-Record-Type\" integer, "
					"  \"Session-Id\" bytea, "
					"  \"Accounting-Record-Number\" integer, "
					"  \"Route-Record1\" bytea, "
					"  \"Route-Record2\" bytea, "
					"  \"Route-Record3\" bytea, "
					"  \"Route-Record4\" bytea "
					");"
				);
		EXPECT_EQ( PGRES_COMMAND_OK, PQresultStatus(res) );
		PQclear(res);
	}
	
	/* OK, we are ready to test now. Create an ACR message that will pass the ABNF check */
	{
		struct dict_object * d = NULL;
		struct avp *avp = NULL;
		union avp_value avp_val;

		/* Now find the ACR dictionary object */
		EXPECT_EQ( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Accounting-Request", &d, ENOENT ) );

		/* Create the instance */
		EXPECT_EQ( 0, fd_msg_new ( d, MSGFL_ALLOC_ETEID, &msg ) );
		
		/* App id */
		{
			struct msg_hdr * h;
			EXPECT_EQ( 0, fd_msg_hdr( msg, &h ) );
			h->msg_appl = 3;
		}
		
		/* sid */
		{
			struct session * sess = NULL;
			os0_t s;
			EXPECT_EQ( 0, fd_sess_new( &sess, fd_g_config->cnf_diamid, fd_g_config->cnf_diamid_len, NULL, 0) );
			EXPECT_EQ( 0, fd_sess_getsid(sess, &s, &sess_bkp_len) );
			EXPECT_EQ( 1, (sess_bkp = os0dup(s, sess_bkp_len)) ? 1 : 0);

			EXPECT_EQ( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Session-Id", &d, ENOENT ) );
			EXPECT_EQ( 0, fd_msg_avp_new ( d, 0, &avp ) );
			memset(&avp_val, 0, sizeof(avp_val));
			avp_val.os.data = sess_bkp;
			avp_val.os.len = sess_bkp_len;
			EXPECT_EQ( 0, fd_msg_avp_setvalue ( avp, &avp_val ) );
			EXPECT_EQ( 0, fd_msg_avp_add ( msg, MSG_BRW_FIRST_CHILD, avp) );
		}
		
		/* Origin-* */
		EXPECT_EQ( 0, fd_msg_add_origin(msg, 1) );
		
		/* Destination-Realm */
		{
			EXPECT_EQ( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Realm", &d, ENOENT ) );
			EXPECT_EQ( 0, fd_msg_avp_new ( d, 0, &avp ) );
			memset(&avp_val, 0, sizeof(avp_val));
			avp_val.os.data = (unsigned char *)fd_g_config->cnf_diamrlm;
			avp_val.os.len = fd_g_config->cnf_diamrlm_len;
			EXPECT_EQ( 0, fd_msg_avp_setvalue ( avp, &avp_val ) );
			EXPECT_EQ( 0, fd_msg_avp_add ( msg, MSG_BRW_LAST_CHILD, avp) );
		}
		
		/* Accounting-Record-Type */
		{
			EXPECT_EQ( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Accounting-Record-Type", &d, ENOENT ) );
			EXPECT_EQ( 0, fd_msg_avp_new ( d, 0, &avp ) );
			memset(&avp_val, 0, sizeof(avp_val));
			avp_val.u32 = 2;
			EXPECT_EQ( 0, fd_msg_avp_setvalue ( avp, &avp_val ) );
			EXPECT_EQ( 0, fd_msg_avp_add ( msg, MSG_BRW_LAST_CHILD, avp) );
		}
		
		/* Accounting-Record-Number */
		{
			EXPECT_EQ( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Accounting-Record-Number", &d, ENOENT ) );
			EXPECT_EQ( 0, fd_msg_avp_new ( d, 0, &avp ) );
			memset(&avp_val, 0, sizeof(avp_val));
			avp_val.u32 = 2;
			EXPECT_EQ( 0, fd_msg_avp_setvalue ( avp, &avp_val ) );
			EXPECT_EQ( 0, fd_msg_avp_add ( msg, MSG_BRW_LAST_CHILD, avp) );
		}
		
		/* Route-Record */
		{
			EXPECT_EQ( 0, fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Route-Record", &d, ENOENT ) );
			EXPECT_EQ( 0, fd_msg_avp_new ( d, 0, &avp ) );
			memset(&avp_val, 0, sizeof(avp_val));
			avp_val.os.data = (unsigned char *)"peer1";
			avp_val.os.len = strlen((char *)avp_val.os.data);
			EXPECT_EQ( 0, fd_msg_avp_setvalue ( avp, &avp_val ) );
			EXPECT_EQ( 0, fd_msg_avp_add ( msg, MSG_BRW_LAST_CHILD, avp) );
			
			EXPECT_EQ( 0, fd_msg_avp_new ( d, 0, &avp ) );
			memset(&avp_val, 0, sizeof(avp_val));
			avp_val.os.data = (unsigned char *)"peer2";
			avp_val.os.len = strlen((char *)avp_val.os.data);
			EXPECT_EQ( 0, fd_msg_avp_setvalue ( avp, &avp_val ) );
			EXPECT_EQ( 0, fd_msg_avp_add ( msg, MSG_BRW_LAST_CHILD, avp) );
		}
		
		/* Source */
		EXPECT_EQ( 0, fd_msg_source_set( msg, "peer3", CONSTSTRLEN("peer3") ) );
		EXPECT_EQ( 0, fd_msg_source_setrr( msg, "peer3", CONSTSTRLEN("peer3"), fd_g_config->cnf_dict ) );
	}
	
	/* Now, have the daemon handle this */

	EXPECT_EQ( 0, fd_fifo_post(fd_g_incoming, &msg) );
	
	/* It is picked by the dispatch module, the extension handles the query, inserts the records in the DB, send creates the answer.
	   Once the answer is ready, it is sent to "peer3" which is not available of course; then the message is simply destroyed.
	   We wait 1 second for this to happen... */
	sleep(1);
	
	/* Now, check the record was actually registered properly */
	{
		PGresult * res;
		uint8_t * bs;
		char * es;
		size_t l;
		
		res = PQexec(conn, "SELECT \"Session-Id\" from " TABLE ";");
		EXPECT_EQ( PGRES_TUPLES_OK, PQresultStatus(res) );
		
		/* We also check that the Session-Id we retrieve is the same as what we generated earlier (not trashed in the process) */
		es = PQgetvalue(res, 0, 0);
		bs = PQunescapeBytea((uint8_t *)es, &l);
		
		EXPECT_EQ( 0, fd_os_cmp(bs, l, sess_bkp, sess_bkp_len) );
		
		PQclear(res);
		PQfreemem(bs);
	}  

	/* That's all for the tests yet */
	
} 
	
