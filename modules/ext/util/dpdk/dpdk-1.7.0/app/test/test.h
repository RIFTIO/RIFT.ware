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

#ifndef _TEST_H_
#define _TEST_H_

#include <sys/queue.h>

#define TEST_ASSERT(cond, msg, ...) do {						\
		if (!(cond)) {											\
			printf("TestCase %s() line %d failed: "			\
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);	\
			return -1;											\
		}														\
} while (0)

#define TEST_ASSERT_EQUAL(a, b, msg, ...)  {					\
		if (!(a == b)) {										\
			printf("TestCase %s() line %d failed: "				\
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);	\
			return -1;											\
		}														\
} while (0)

#define TEST_ASSERT_NOT_EQUAL(a, b, msg, ...) do {				\
		if (!(a != b)) {										\
			printf("TestCase %s() line %d failed: "			\
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);	\
			return -1;											\
		}														\
} while (0)

#define TEST_ASSERT_SUCCESS(val, msg, ...) do {					\
		if (!(val == 0)) {										\
			printf("TestCase %s() line %d failed: "			\
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);	\
			return -1;											\
		}														\
} while (0)

#define TEST_ASSERT_FAIL(val, msg, ...) do {					\
		if (!(val != -1)) {										\
			printf("TestCase %s() line %d failed: "			\
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);	\
			return -1;											\
		}														\
} while (0)


#define TEST_ASSERT_NULL(val, msg, ...) do {					\
		if (!(val == NULL)) {									\
			printf("TestCase %s() line %d failed: "			\
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);	\
			return -1;											\
		}														\
} while (0)

#define TEST_ASSERT_NOT_NULL(val, msg, ...) do {				\
		if (!(val != NULL)) {									\
			printf("TestCase %s() line %d failed: "			\
				msg "\n", __func__, __LINE__, ##__VA_ARGS__);	\
			return -1;											\
		}														\
} while (0)

struct unit_test_case {
	int (*setup)(void);
	int (*teardown)(void);
	int (*testcase)(void);
	const char *success_msg;
	const char *fail_msg;
};

#define TEST_CASE(fn) { NULL, NULL, fn, #fn " succeeded", #fn " failed"}

#define TEST_CASE_ST(setup, teardown, testcase) 		\
		{ setup, teardown, testcase, #testcase " succeeded",	\
		#testcase " failed "}

#define TEST_CASES_END() { NULL, NULL, NULL, NULL, NULL }

struct unit_test_suite {
	const char *suite_name;
	int (*setup)(void);
	int (*teardown)(void);
	struct unit_test_case unit_test_cases[];
};

int unit_test_suite_runner(struct unit_test_suite *suite);

/* icc on baremetal gives us troubles with function named 'main' */
#ifdef RTE_EXEC_ENV_BAREMETAL
#define main _main
#endif
#ifdef RTE_LIBRW_PMD_TUN
int test_pmd_tun(void);
#endif

#define RECURSIVE_ENV_VAR "RTE_TEST_RECURSIVE"

extern const char *prgname;

int commands_init(void);

int main(int argc, char **argv);

int test_pci(void);
int test_pci_run;

int test_mp_secondary(void);

int test_ivshmem(void);
#ifdef RTE_LIBRW_PMD_RAW
int test_pmd_raw(void);
int test_pmd_tun(void);
#endif

typedef int (test_callback)(void);
TAILQ_HEAD(test_commands_list, test_command);
struct test_command {
	TAILQ_ENTRY(test_command) next;
	const char *command;
	test_callback *callback;
};

void add_test_command(struct test_command *t);

#define REGISTER_TEST_COMMAND(t) \
static void testfn_##t(void);\
void __attribute__((constructor, used)) testfn_##t(void)\
{\
	add_test_command(&t);\
}

#endif
