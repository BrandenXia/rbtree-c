#include <stdbool.h>
#include <stddef.h>

struct rb_node_s;

typedef struct rb_node_s rb_node_t;
typedef rb_node_t *rb_node_p;

struct rb_node_s {
  rb_node_p p;
  rb_node_p childs[2];
  size_t size;
  bool is_red;

  void *data;
};

typedef bool (*rb_lt_f)(const void *a, const void *b);
typedef struct rb_tree_s {
  rb_node_p root;
  rb_lt_f lt;
} rb_tree_t;

const rb_node_p _rb_insert(rb_tree_t *tree, void *data, size_t data_size);
/*
 * rb_insert(rb_tree_t *tree, type *data)
 * accepts a pointer to the data to be inserted.
 * data will be copied into the tree node.
 * type must be the actual type of the data being inserted.
 */
#ifndef rb_insert
#define rb_insert(tree, data) _rb_insert(tree, data, sizeof(*(data)))
#endif

/*
 * rb_erase(rb_tree_t *tree, rb_node_p n)
 * removes the node n from the tree.
 * the data inside the node will be freed, as well as the node itself.
 */
void rb_erase(rb_tree_t *tree, rb_node_p n);

/*
 * rb_create(rb_lt_f lt)
 * accepts a comparison function lt that returns true if a < b.
 * returns an empty red-black tree.
 * will allocate memory on stack.
 */
rb_tree_t rb_create(rb_lt_f lt);

#ifdef RBTREE_IMPL

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static bool _rb_child_dir(const rb_node_p n) { return n == n->p->childs[1]; };

static void _rb_rotate(rb_node_p *root, bool dir) {
  auto r = (*root);
  auto parent = r->p;
  auto new_root = r->childs[!dir];
  assert(new_root != NULL && "cannot rotate on a NULL child");

  new_root->size = r->size;
  r->size = r->childs[dir]->size + new_root->childs[dir]->size + 1;

  auto child = new_root->childs[dir];
  if (child)
    child->p = r;
  r->childs[!dir] = child;
  new_root->childs[dir] = r;
  r->p = new_root;
  new_root->p = parent;
  if (parent)
    parent->childs[_rb_child_dir(r)] = new_root;

  *root = new_root;
}

void _rb_insert_fixup(rb_tree_t *tree, rb_node_p *parent, rb_node_p *n,
                      bool dir) {
  while (*parent && (*parent)->is_red) {
    bool p_dir = _rb_child_dir(*parent);
    auto grand = (*parent)->p;
    auto uncle = grand->childs[!p_dir];

    if (uncle && uncle->is_red) {
      (*parent)->is_red = uncle->is_red = false;
      grand->is_red = true;
      (*n) = grand;
      continue;
    }

    if (_rb_child_dir(*n) != p_dir) {
      _rb_rotate(parent, p_dir);
      auto tmp = *parent;
      (*parent) = *n;
      (*n) = tmp;
    }

    (*parent)->is_red = false;
    grand->is_red = true;
    _rb_rotate(&grand, !p_dir);
  }

  tree->root->is_red = false;
}

const rb_node_p _rb_insert(rb_tree_t *tree, void *data, size_t data_size) {
  rb_node_p n = (rb_node_p)malloc(sizeof(rb_node_t));
  n->p = n->childs[0] = n->childs[1] = NULL;
  n->size = 1;
  n->data = malloc(data_size);
  memcpy(n->data, data, data_size);

  rb_node_p cur = tree->root, parent = NULL;
  bool dir = 0;
  while (cur) {
    parent = cur;
    dir = tree->lt(data, cur->data);
    cur = cur->childs[dir];
  }

  // insert node to leaf
  n->is_red = true;
  if (parent == NULL) {
    tree->root = n;
    return n;
  }
  parent->childs[dir] = n;
  parent->size += 1;
  n->p = parent;
  auto now = parent;
  while (now)
    now->size++, now = now->p;

  _rb_insert_fixup(tree, &parent, &n, dir);

  return n;
}

void rb_erase_fixup(rb_tree_t *tree, rb_node_p *x) {
  bool dir = (*x) == tree->root ? false : _rb_child_dir(*x);

  // remove branch or leaf
  auto parent = (*x)->p,
       s = (*x)->childs[0] ? (*x)->childs[0] : (*x)->childs[1];
  rb_node_p now;
  if (s)
    s->p = parent;
  if (!parent) {
    tree->root = s;
    goto start_fixup;
  }
  parent->childs[dir] = s;
  now = parent;
  while (now)
    now->size--, now = now->p;

start_fixup:
  parent = (*x)->p;
  if (!parent) {
    if (tree->root)
      tree->root->is_red = false;
    return;
  }

  auto sib = parent->childs[dir];
  if (sib) {
    sib->is_red = false;
    return;
  }

  while (parent && !(*x)->is_red) {
    sib = parent->childs[!dir];

    if (sib->is_red) {
      sib->is_red = false;
      parent->is_red = true;
      _rb_rotate(&parent, dir);
      sib = parent->childs[!dir];
    }

    auto child = sib->childs[dir], dir_child = sib->childs[!dir];
    if (!child->is_red && !dir_child->is_red) {
      sib->is_red = true;
      (*x) = parent;
      goto end_fixup;
    }

    if (!dir_child->is_red) {
      child->is_red = false;
      sib->is_red = true;
      _rb_rotate(&sib, !dir);
      sib = parent->childs[!dir];
      child = sib->childs[dir];
      dir_child = sib->childs[!dir];
    }

    sib->is_red = parent->is_red;
    parent->is_red = dir_child->is_red = false;
    _rb_rotate(&parent, dir);
    (*x) = tree->root;

  end_fixup:
    parent = (*x)->p;
    if (!parent)
      break;
    dir = _rb_child_dir(*x);
  }

  (*x)->is_red = false;
};

void rb_erase(rb_tree_t *tree, rb_node_p n) {
  if (!n)
    return;

  if (n->childs[0] && n->childs[1]) {
    // find successor
    rb_node_p succ = n->childs[1];
    while (succ->childs[0])
      succ = succ->childs[0];

    // swap data
    void *tmp = n->data;
    n->data = succ->data;
    succ->data = tmp;

    n = succ;
  }
  rb_erase_fixup(tree, &n);
  free(n->data);
  free(n);
}

rb_tree_t rb_create(rb_lt_f lt) {
  rb_tree_t tree;
  tree.root = NULL;
  tree.lt = lt;
  return tree;
}

#endif
