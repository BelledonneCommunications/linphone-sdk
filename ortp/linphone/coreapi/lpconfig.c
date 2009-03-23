/***************************************************************************
 *            lpconfig.c
 *
 *  Thu Mar 10 11:13:44 2005
 *  Copyright  2005  Simon Morlat
 *  Email simon.morlat@linphone.org
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define MAX_LEN 1024

#include "linphonecore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#define lp_new0(type,n)	(type*)calloc(sizeof(type),n)

#include "lpconfig.h"

#define LISTNODE(_struct_)	\
	struct _struct_ *_prev;\
	struct _struct_ *_next;

typedef struct _ListNode{
	LISTNODE(_ListNode)
} ListNode;

typedef void (*ListNodeForEachFunc)(ListNode *);

ListNode * list_node_append(ListNode *head,ListNode *elem){
	ListNode *e=head;
	while(e->_next!=NULL) e=e->_next;
	e->_next=elem;
	elem->_prev=e;
	return head;
}

ListNode * list_node_remove(ListNode *head, ListNode *elem){
	ListNode *before,*after;
	before=elem->_prev;
	after=elem->_next;
	if (before!=NULL) before->_next=after;
	if (after!=NULL) after->_prev=before;
	elem->_prev=NULL;
	elem->_next=NULL;
	if (head==elem) return after;
	return head;
}

void list_node_foreach(ListNode *head, ListNodeForEachFunc func){
	for (;head!=NULL;head=head->_next){
		func(head);
	}
}


#define LIST_PREPEND(e1,e2) (  (e2)->_prev=NULL,(e2)->_next=(e1),(e1)->_prev=(e2),(e2) )
#define LIST_APPEND(head,elem) ((head)==0 ? (elem) : (list_node_append((ListNode*)(head),(ListNode*)(elem)), (head)) ) 
#define LIST_REMOVE(head,elem) 

/* returns void */
#define LIST_FOREACH(head) list_node_foreach((ListNode*)head)

typedef struct _LpItem{
	char *key;
	char *value;
} LpItem;

typedef struct _LpSection{
	char *name;
	MSList *items;
} LpSection;

struct _LpConfig{
	FILE *file;
	char *filename;
	MSList *sections;
	int modified;
};

LpItem * lp_item_new(const char *key, const char *value){
	LpItem *item=lp_new0(LpItem,1);
	item->key=strdup(key);
	item->value=strdup(value);
	return item;
}

LpSection *lp_section_new(const char *name){
	LpSection *sec=lp_new0(LpSection,1);
	sec->name=strdup(name);
	return sec;
}

void lp_item_destroy(void *pitem){
	LpItem *item=(LpItem*)pitem;
	free(item->key);
	free(item->value);
	free(item);
}

void lp_section_destroy(LpSection *sec){
	free(sec->name);
	ms_list_for_each(sec->items,lp_item_destroy);
	ms_list_free(sec->items);
	free(sec);
}

void lp_section_add_item(LpSection *sec,LpItem *item){
	sec->items=ms_list_append(sec->items,(void *)item);
}

void lp_config_add_section(LpConfig *lpconfig, LpSection *section){
	lpconfig->sections=ms_list_append(lpconfig->sections,(void *)section);
}

void lp_config_remove_section(LpConfig *lpconfig, LpSection *section){
	lpconfig->sections=ms_list_remove(lpconfig->sections,(void *)section);
	lp_section_destroy(section);
}

static bool_t is_first_char(const char *start, const char *pos){
	const char *p;
	for(p=start;p<pos;p++){
		if (*p!=' ') return FALSE;
	}
	return TRUE;
}

void lp_config_parse(LpConfig *lpconfig){
	char tmp[MAX_LEN];
	LpSection *cur=NULL;
	
	if (lpconfig->file==NULL) return;
	
	while(fgets(tmp,MAX_LEN,lpconfig->file)!=NULL){
		char *pos1,*pos2;
		pos1=strchr(tmp,'[');
		if (pos1!=NULL && is_first_char(tmp,pos1) ){
			pos2=strchr(pos1,']');
			if (pos2!=NULL){
				int nbs;
				char secname[MAX_LEN];
				secname[0]='\0';
				/* found section */
				*pos2='\0';
				nbs = sscanf(pos1+1,"%s",secname);
				if (nbs == 1 ){
					if (strlen(secname)>0){
						cur=lp_section_new(secname);
						lp_config_add_section(lpconfig,cur);
					}
				}else{
					ms_warning("parse error!");
				}
			}
		}else {
			pos1=strchr(tmp,'=');
			if (pos1!=NULL){
				char key[MAX_LEN];
				key[0]='\0';
				
				*pos1='\0';
				if (sscanf(tmp,"%s",key)>0){
					
					pos1++;
					pos2=strchr(pos1,'\n');
					if (pos2==NULL) pos2=pos1+strlen(pos2);
					else {
						*pos2='\0'; /*replace the '\n' */
						pos2--;
					}
					/* remove ending white spaces */
					for (; pos2>pos1 && *pos2==' ';pos2--) *pos2='\0';
					if (pos2-pos1>=0){
						/* found a pair key,value */
						if (cur!=NULL){
							lp_section_add_item(cur,lp_item_new(key,pos1));
							/*printf("Found %s %s=%s\n",cur->name,key,pos1);*/
						}else{
							ms_warning("found key,item but no sections");
						}
					}
				}
			}
		}
	}
}

LpConfig * lp_config_new(const char *filename){
	LpConfig *lpconfig=lp_new0(LpConfig,1);
	if (filename!=NULL){
		lpconfig->filename=strdup(filename);
		lpconfig->file=fopen(filename,"rw");
		if (lpconfig->file!=NULL){
			lp_config_parse(lpconfig);
			fclose(lpconfig->file);
			/* make existing configuration files non-group/world-accessible */
			if (chmod(filename, S_IRUSR | S_IWUSR) == -1)
				ms_warning("unable to correct permissions on "
				  	  "configuration file: %s",
					   strerror(errno));
			lpconfig->file=NULL;
			lpconfig->modified=0;
		}
	}
	return lpconfig;
}

void lp_item_set_value(LpItem *item, const char *value){
	free(item->value);
	item->value=strdup(value);
}


void lp_config_destroy(LpConfig *lpconfig){
	if (lpconfig->filename!=NULL) free(lpconfig->filename);
	ms_list_for_each(lpconfig->sections,(void (*)(void*))lp_section_destroy);
	ms_list_free(lpconfig->sections);
	free(lpconfig);
}

LpSection *lp_config_find_section(LpConfig *lpconfig, const char *name){
	LpSection *sec;
	MSList *elem;
	/*printf("Looking for section %s\n",name);*/
	for (elem=lpconfig->sections;elem!=NULL;elem=ms_list_next(elem)){
		sec=(LpSection*)elem->data;
		if (strcmp(sec->name,name)==0){
			/*printf("Section %s found\n",name);*/
			return sec;
		}
	}
	return NULL;
}

LpItem *lp_section_find_item(LpSection *sec, const char *name){
	MSList *elem;
	LpItem *item;
	/*printf("Looking for item %s\n",name);*/
	for (elem=sec->items;elem!=NULL;elem=ms_list_next(elem)){
		item=(LpItem*)elem->data;
		if (strcmp(item->key,name)==0) {
			/*printf("Item %s found\n",name);*/
			return item;
		}
	}
	return NULL;
}

void lp_section_remove_item(LpSection *sec, LpItem *item){
	sec->items=ms_list_remove(sec->items,(void *)item);
	lp_item_destroy(item);
}

const char *lp_config_get_string(LpConfig *lpconfig, const char *section, const char *key, const char *default_string){
	LpSection *sec;
	LpItem *item;
	sec=lp_config_find_section(lpconfig,section);
	if (sec!=NULL){
		item=lp_section_find_item(sec,key);
		if (item!=NULL) return item->value;
	}
	return default_string;
}

int lp_config_get_int(LpConfig *lpconfig,const char *section, const char *key, int default_value){
	const char *str=lp_config_get_string(lpconfig,section,key,NULL);
	if (str!=NULL) return atoi(str);
	else return default_value;
}

float lp_config_get_float(LpConfig *lpconfig,const char *section, const char *key, float default_value){
	const char *str=lp_config_get_string(lpconfig,section,key,NULL);
	float ret=default_value;
	if (str==NULL) return default_value;
	sscanf(str,"%f",&ret);
	return ret;
}

void lp_config_set_string(LpConfig *lpconfig,const char *section, const char *key, const char *value){
	LpItem *item;
	LpSection *sec=lp_config_find_section(lpconfig,section);
	if (sec!=NULL){
		item=lp_section_find_item(sec,key);
		if (item!=NULL){
			if (value!=NULL)
				lp_item_set_value(item,value);
			else lp_section_remove_item(sec,item);
		}else{
			if (value!=NULL)
				lp_section_add_item(sec,lp_item_new(key,value));
		}
	}else if (value!=NULL){
		sec=lp_section_new(section);
		lp_config_add_section(lpconfig,sec);
		lp_section_add_item(sec,lp_item_new(key,value));
	}
	lpconfig->modified++;
}

void lp_config_set_int(LpConfig *lpconfig,const char *section, const char *key, int value){
	char tmp[30];
	snprintf(tmp,30,"%i",value);
	lp_config_set_string(lpconfig,section,key,tmp);
	lpconfig->modified++;
}

void lp_item_write(LpItem *item, FILE *file){
	fprintf(file,"%s=%s\n",item->key,item->value);
}

void lp_section_write(LpSection *sec, FILE *file){
	fprintf(file,"[%s]\n",sec->name);
	ms_list_for_each2(sec->items,(void (*)(void*, void*))lp_item_write,(void *)file);
	fprintf(file,"\n");
}

int lp_config_sync(LpConfig *lpconfig){
	FILE *file;
	if (lpconfig->filename==NULL) return -1;
#ifndef WIN32
	/* don't create group/world-accessible files */
	(void) umask(S_IRWXG | S_IRWXO);
#endif
	file=fopen(lpconfig->filename,"w");
	if (file==NULL){
		ms_warning("Could not write %s !",lpconfig->filename);
		return -1;
	}
	ms_list_for_each2(lpconfig->sections,(void (*)(void *,void*))lp_section_write,(void *)file);
	fclose(file);
	lpconfig->modified=0;
	return 0;
}

int lp_config_has_section(LpConfig *lpconfig, const char *section){
	if (lp_config_find_section(lpconfig,section)!=NULL) return 1;
	return 0;
}

void lp_config_clean_section(LpConfig *lpconfig, const char *section){
	LpSection *sec=lp_config_find_section(lpconfig,section);
	if (sec!=NULL){
		lp_config_remove_section(lpconfig,sec);
	}
	lpconfig->modified++;
}

int lp_config_needs_commit(const LpConfig *lpconfig){
	return lpconfig->modified>0;
}
