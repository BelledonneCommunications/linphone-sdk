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
 *	Contains some generic functions used across CUnit project files.
 *
 *	Created By     : Anil Kumar on 13/Oct/2001
 *	Last Modified  : 13/Oct/2001
 *	Comment        : Moved some of the generic functions definitions 
 *	                 from other files to this one so as to use the 
 *	                 functions consitently. This file is not included
 *	                 in the distribution headers because it is used 
 *	                 internally by CUnit.
 *	EMail          : aksaharan@yahoo.com
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#ifdef MEMTRACE

#define MAX_FILE_NAME_LENGTH  256

const unsigned int NOT_DELETED = 0;
const char* szDumpFileName = "CUnit-Memory-Dump.lst";

typedef enum {
	ALLOCATED = 1,
	DEALLOCATED,
	REALLOCATED
} STATUS;

typedef struct mem_node {
	int 				nSize;
	void*				pLocation;
	unsigned int 		uiAllocationLine;
	char 				szAllocationFileName[MAX_FILE_NAME_LENGTH];
	unsigned int 		uiDeletionLine;
	char 				szDeletionFileName[MAX_FILE_NAME_LENGTH];
	STATUS				ChangeStatus;
	struct mem_node*	pNext;
} MEMORY_NODE;

typedef MEMORY_NODE* PMEMORY_NODE;

PMEMORY_NODE pMemoryTrackerHead = NULL, pMemoryTrackerTail = NULL;

PMEMORY_NODE allocate_memory(int nSize, void* pLocation, unsigned int uiAllocationLine, const char* szAllocationFile)
{
	PMEMORY_NODE pMemory = malloc(sizeof(MEMORY_NODE));
	assert(pMemory && "Memory Allocation failed in memory debug routine.");

	pMemory->nSize = nSize;
	pMemory->pLocation = pLocation;
	pMemory->uiAllocationLine = uiAllocationLine;
	strcpy(pMemory->szAllocationFileName, szAllocationFile);
	pMemory->uiDeletionLine = NOT_DELETED;
	pMemory->pNext = NULL;

	if (!pMemoryTrackerHead) {
		pMemoryTrackerHead = pMemoryTrackerTail = pMemory;
	} else {
		pMemoryTrackerTail->pNext = pMemory;
		pMemoryTrackerTail = pMemory;
	}
	
	return NULL;
}

void deallocate_memory(void* pLocation, unsigned int uiDeletionLine, const char* szDeletionFileName)
{
	PMEMORY_NODE pTemp = NULL;
	for (pTemp = pMemoryTrackerHead; pTemp; pTemp = pTemp->pNext) {
		if (pTemp->pLocation == pLocation && pTemp->uiDeletionLine == NOT_DELETED) {
			pTemp->uiDeletionLine = uiDeletionLine;
			strcpy(pTemp->szDeletionFileName, szDeletionFileName);
			return ;
		}
	}

	assert("Unable to find the allocated node in the memory allocation list.");
}

void* my_calloc(size_t nmemb, size_t size, unsigned int uiLine, const char* szFileName)
{
	void* pVoid = calloc(nmemb, size);
	if (pVoid) {
		allocate_memory(nmemb * size, pVoid, uiLine, szFileName);
	}

	return pVoid;
}

void* my_malloc(size_t size, unsigned int uiLine, const char* szFileName)
{
	void* pVoid = malloc(size);
	if (pVoid) {
		allocate_memory(size, pVoid, uiLine, szFileName);
	}

	return pVoid;
}

void my_free(void *ptr, unsigned int uiLine, const char* szFileName)
{
	deallocate_memory(ptr, uiLine, szFileName);
	free(ptr);
}

void* my_realloc(void *ptr, size_t size, unsigned int uiLine, const char* szFileName)
{
	void* pVoid = NULL;

	deallocate_memory(ptr, uiLine, szFileName);
	pVoid = realloc(ptr, size);
	if (pVoid) {
		allocate_memory(size, pVoid, uiLine, szFileName);
	}
	
	return pVoid;
}

void dump_memory_usage(void)
{
	int nSerial = 0;
	PMEMORY_NODE pTemp = NULL;
	FILE* pFile = fopen(szDumpFileName, "w");
	time_t tTime = 0;
	
	if (!pFile) {
		fprintf(stderr, "Failed to open file \"%s\" : %s", szDumpFileName, strerror(errno));
		return;
	}

	setvbuf(pFile, NULL, _IONBF, 0);

	fprintf(pFile, "<\?xml version=\"1.0\" \?>");
	fprintf(pFile, "\n<\?xml-stylesheet type=\"text/xsl\" href=\"Memory-Dump.xsl\" \?>");
	fprintf(pFile, "\n<!DOCTYPE MD_TEST_RUN_REPORT SYSTEM \"Memory-Dump.dtd\">");
	fprintf(pFile, "\n<MEMORY_DUMP_REPORT>");
	fprintf(pFile, "\n\t<MD_HEADER/>");
	fprintf(pFile, "\n\t<MD_RUN_LISTING>");

	for (pTemp = pMemoryTrackerHead, nSerial = 0; pTemp != NULL; pTemp = pTemp->pNext, nSerial++) {
		fprintf(pFile, "\n\t\t<MD_RUN_RECORD>");
		fprintf(pFile, "\n\t\t\t<MD_SIZE> %d </MD_SIZE>", pTemp->nSize);
		fprintf(pFile, "\n\t\t\t<MD_POINTER> %p </MD_POINTER>", pTemp->pLocation);
		fprintf(pFile, "\n\t\t\t<MD_SOURCE_FILE> %s </MD_SOURCE_FILE>", pTemp->szAllocationFileName);
		fprintf(pFile, "\n\t\t\t<MD_SOURCE_LINE> %d </MD_SOURCE_LINE>", pTemp->uiAllocationLine);
		fprintf(pFile, "\n\t\t\t<MD_DESTINATION_FILE> %s </MD_DESTINATION_FILE>", pTemp->szDeletionFileName);
		fprintf(pFile, "\n\t\t\t<MD_DESTINATION_LINE> %d </MD_DESTINATION_LINE>", pTemp->uiDeletionLine);
		fprintf(pFile, "\n\t\t\t<MD_RECORD_STATUS> %d </MD_RECORD_STATUS>", pTemp->ChangeStatus);
		fprintf(pFile, "\n\t\t</MD_RUN_RECORD>");
	}

	fprintf(pFile, "\n\t</MD_RUN_LISTING>");

	fprintf(pFile, "\n\t<MD_SUMMARY>");
	fprintf(pFile, "\n\t\t<MD_SUMMARY_VALID_RECORDS> %d </MD_SUMMARY_VALID_RECORDS>", nSerial);
	fprintf(pFile, "\n\t\t<MD_SUMMARY_INVALID_RECORDS> %d </MD_SUMMARY_INVALID_RECORDS>", nSerial);
	fprintf(pFile, "\n\t\t<MD_SUMMARY_TOTAL_RECORDS> %d </MD_SUMMARY_TOTAL_RECORDS>", nSerial);
	fprintf(pFile, "\n\t</MD_SUMMARY>");

	time(&tTime);
	fprintf(pFile, "\n\t<MD_FOOTER> Memory Trace for CUnit Run at %s </MD_FOOTER>", ctime(&tTime));
	fprintf(pFile, "</MEMORY_DUMP_REPORT>");

	fclose(pFile);
}

#endif	
