/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *	Contains Memory Related Defines to use internal routines to detect Memory Leak
 *	in Debug Versions
 *
 *	Created By      : Anil Kumar on ...(18 June 2002)
 *	Last Modified   : 18/Jun/2002
 *	Comment         : Memory Debug Funstions
 *	EMail           : aksaharan@yahoo.com
 */

#ifndef _CUNIT_MYMEM_H
#define _CUNIT_MYMEM_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MEMTRACE	
	void* my_calloc(size_t nmemb, size_t size, unsigned int uiLine, const char* szFileName);
	void* my_malloc(size_t size, unsigned int uiLine, const char* szFileName);
	void my_free(void *ptr, unsigned int uiLine, const char* szFileName);
	void* my_realloc(void *ptr, size_t size, unsigned int uiLine, const char* szFileName);

	void dump_memory_usage(void);
		
	#define MR_CALLOC(x, y)		my_calloc((x), (y), __LINE__, __FILE__)
	#define MY_MALLOC(x) 		my_malloc((x), __LINE__, __FILE__)
	#define MY_FREE(x)			my_free((x), __LINE__, __FILE__)
	#define MR_REALLOC(x, y)	my_realloc((x), (y), __LINE__, __FILE__)
	#define DUMP_MEMORY_USAGE()	dump_memory_usage()
#else
	#define MR_CALLOC(x, y)		calloc((x), (y))
	#define MY_MALLOC(x) 		malloc((x))
	#define MY_FREE(x)			free((x))
	#define MR_REALLOC(x, y)	realloc((x), (y))
	#define DUMP_MEMORY_USAGE()	
#endif	

#ifdef __cplusplus
}
#endif
#endif  /*  _CUNIT_MYMEM_H  */
