#define RBTREE_IMPL
#include "rbtree.h"

bool int_lt(const void *a, const void *b) { return (*(int *)a) < (*(int *)b); }

int main() {
  rb_tree_t tree = rb_create(&int_lt);
  int a = 5;
  rb_insert(&tree, &a);
}
