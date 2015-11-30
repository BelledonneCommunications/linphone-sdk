#ifndef belcard_tester_hpp
#define belcard_tester_hpp

#include "common/bc_tester_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

extern test_suite_t vcard_identification_properties_test_suite;

void belcard_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args));
void belcard_tester_uninit(void);

#ifdef __cplusplus
};
#endif


#endif
