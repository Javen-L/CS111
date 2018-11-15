#include <stdlib.h>
#include <stdio.h>
#include "SortedList.h"

int opt_yield = 0;

void printls(SortedList_t *list) {
  SortedListElement_t *iter;
  if (!list || list->key) return; // invalid head node
  for (iter = list->next;; iter = iter->next) {
    if (!iter || iter == list) { // empty or a full loop finished
      printf("Head\n");
      break;
    }
    printf("%s\n", iter->key);
    /*
    printf("%#x\n",iter);
    printf("%#x\n",iter->next);
    printf("%#x\n",list);
    */
  }
  printf("----End\n");
}

int main() {
  printf("test list\n");
  SortedList_t hd;
  hd.key = NULL; hd.prev = NULL; hd.next = NULL;

  printf("Len: %d\n", SortedList_length(&hd));

  SortedListElement_t n[4];
  char *k[4];
  //printls(&hd);
  int i;
  for (i = 0; i < 4; i++) {
    k[i] = malloc(2*sizeof(char));
    sprintf(k[i], "%d", 9-i*3);
    n[i].key = k[i];
    n[i].prev = NULL; n[i].next = NULL;
    SortedList_insert(&hd, &(n[i]));
    //printls(&hd);
    printf("Len: %d\n", SortedList_length(&hd));
  }
  SortedListElement_t new;
  new.key = "5";
  SortedList_insert(&hd, &new);
  printls(&hd);
  printf("Len: %d\n", SortedList_length(&hd));

  for (i = 0; i < 4; i++) {
    SortedListElement_t *rv = SortedList_lookup(&hd, k[i]);
    if (!rv) printf("Nothing found\n");
    else printf("Look for %s, found %s\n", k[i], rv->key);
  }

  for (i = 0; i < 4; i++) {
    int rv;
    rv = SortedList_delete(&(n[3-i]));
    printf("Delete %s, rv=%d\n", n[3-i].key, rv);
    //printls(&hd);
    rv = SortedList_delete(&(n[3-i]));
    printf("Delete %s, rv=%d\n", n[3-i].key, rv);
    //printls(&hd);
    printf("Len: %d\n", SortedList_length(&hd));
  }

  for (i = 0; i < 4; i++) {
    SortedListElement_t *rv = SortedList_lookup(&hd, k[i]);
    if (!rv) printf("Nothing found\n");
    else printf("Look for %s, found %s\n", k[i], rv->key);
  }
  SortedListElement_t *rv1 = SortedList_lookup(&hd, new.key);
  if (!rv1) printf("Nothing found\n");
  else printf("Look for %s, found %s\n", new.key, rv1->key);

  int rv = SortedList_delete(&new);
  printf("Delete %s, rv=%d\n", new.key, rv);
  printls(&hd);

  printf("Len: %d\n", SortedList_length(&hd));

  for (i = 0; i < 4; i++) {
    SortedListElement_t *rv = SortedList_lookup(&hd, k[i]);
    if (!rv) printf("Nothing found\n");
    else printf("Look for %s, found %s\n", k[i], rv->key);
  }
  for (i = 0; i < 4; i++) {
    free(k[i]);
  }
  printf("Len: %d\n", SortedList_length(&hd));
  return 0;
}
