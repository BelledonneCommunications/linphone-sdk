#ifndef belcard_tester_hpp
#define belcard_tester_hpp

#include "common/bc_tester_utils.h"

#include <string>
#include <memory>
#include <sstream>

#ifdef __cplusplus
extern "C" {
#endif

extern test_suite_t vcard_general_properties_test_suite;
extern test_suite_t vcard_identification_properties_test_suite;
extern test_suite_t vcard_addressing_properties_test_suite;
extern test_suite_t vcard_communication_properties_test_suite;
extern test_suite_t vcard_geographical_properties_test_suite;
extern test_suite_t vcard_organizational_properties_test_suite;
extern test_suite_t vcard_explanatory_properties_test_suite;
extern test_suite_t vcard_security_properties_test_suite;
extern test_suite_t vcard_calendar_properties_test_suite;
extern test_suite_t vcard_test_suite;

void belcard_tester_init(void(*ftester_printf)(int level, const char *fmt, va_list args));
void belcard_tester_uninit(void);

#ifdef __cplusplus
};
#endif

template<typename T>
void test_property(const std::string& input) {
	std::shared_ptr<T> var = T::parse(input);
	BC_ASSERT_TRUE_FATAL(var != NULL);
	std::stringstream sstream;
	sstream << *var;
	BC_ASSERT_EQUAL(input.compare(sstream.str()), 0, int, "%d");
}

#endif
