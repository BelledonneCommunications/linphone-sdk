CUnitIncludedir=${includedir}/CUnit

AUTOMATED_INCLUDES = Automated.h
CONSOLE_INCLUDES = Console.h
CURSES_INCLUDES = Curses.h
FRAMEWORK_INCLUDES = CUnit.h Errno.h TestDB.h TestRun.h

# Default configuration without enabling anything
INCLUDE_FILES = ${FRAMEWORK_INCLUDES} ${CONSOLE_INCLUDES} ${CURSES_INCLUDES} ${AUTOMATED_INCLUDES}

CUnitInclude_HEADERS = ${INCLUDE_FILES}

#Util.h, TestRun.h are not included because these are used internally 
#by the CUnit files but is not for the external inclusion.
