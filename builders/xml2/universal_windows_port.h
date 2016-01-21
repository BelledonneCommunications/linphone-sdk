
#ifndef LIBXML2_PORT_H
#define LIBXML2_PORT_H

#define GetVersionEx(osvi)		(((osvi)->dwPlatformId = 0) != 0)
#define getcwd(a, b) NULL
#define getenv(x) NULL

#endif /* LIBXML2_PORT_H */
