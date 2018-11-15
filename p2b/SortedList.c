/*
NAME: Jianzhi Liu
EMAIL: ljzprivate@yahoo.com
ID: 204742214
*/
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include "SortedList.h"

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
  SortedListElement_t *iter;
  // make sure list point to head node, and element point to some node
  if (!list || !element || list->key) return;
  // deal with empty list
  if (!list->next) {
    if (opt_yield & INSERT_YIELD) sched_yield();
    element->prev = list;
    element->next = list;
    list->next = element;
    list->prev = element;
    return;
  }
  // deal with regular case
  for (iter = list->next;;) {
    if (!iter || iter == list) break;
    if (strcmp(iter->key, element->key) >= 0) break;
    if (opt_yield & INSERT_YIELD) sched_yield();
    iter = iter->next;
  }
  if (opt_yield & INSERT_YIELD) sched_yield();
  element->prev = iter->prev;
  element->next = iter;
  iter->prev->next = element;
  iter->prev = element;
  return;
}

int SortedList_delete( SortedListElement_t *element) {
  if (!element) return 1; // nothing to delete
  if ((element->next && element->prev) &&
      (element != element->next->prev || element != element->prev->next))
    return 1;
  if (opt_yield & DELETE_YIELD) sched_yield();
  element->prev->next = element->next;
  element->next->prev = element->prev;
  //free(element);
  return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
  SortedListElement_t *iter;
  if (!list || list->key) return NULL; // invalid head node
  for (iter = list->next;;) {
    if (!iter || iter == list) { iter = NULL; break; } // empty or a full loop finished
    if (strcmp(iter->key, key) == 0) break;
    if (opt_yield & LOOKUP_YIELD) sched_yield();
    iter = iter->next;
  }
  return iter;
}

int SortedList_length(SortedList_t *list) {
  if (!list || list->key) return -1; // invalid head node
  // deal with empty list
  if (!list->next) {
    if (list->prev) return -1;
    else return 0;
  }
  // deal with regular cases
  SortedListElement_t *iter;
  int len = 0;
  for (iter = list->next;;) {
    if (iter == list) break; // a full loop finished
    if (!iter->prev || iter->prev->next != iter) return -1;
    if (!iter->next) return -1;
    len++;
    if (opt_yield & LOOKUP_YIELD) sched_yield();
    iter = iter->next;
  }
  return len;
}
