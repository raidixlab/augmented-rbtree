/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...
*/

#ifndef	_RDX_RBTREE_H
#define	_RDX_RBTREE_H

// #include <linux/kernel.h>
#include "kernel.h"
#include "stddef.h"

struct rdx_rb_node {
	unsigned long  __rb_parent_color;
	struct rdx_rb_node *rb_right;
	struct rdx_rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
    /* The alignment might seem pointless, but allegedly CRIS needs it */

struct rdx_rb_root {
	struct rdx_rb_node *rb_node;
	int (*strict_compare)(struct rdx_rb_node *left, struct rdx_rb_node *right);
	int (*weak_compare)(struct rdx_rb_node *left, struct rdx_rb_node *right);
};


#define rdx_rb_parent(r)   ((struct rdx_rb_node *)((r)->__rb_parent_color & ~3))

#define RDX_RB_ROOT(rbcompare)	(struct rdx_rb_root) { NULL, rbcompare }
#define	rdx_rb_entry(ptr, type, member) container_of(ptr, type, member)

#define RDX_RB_EMPTY_ROOT(root)  ((root)->rb_node == NULL)

/* 'empty' nodes are nodes that are known not to be inserted in an rbree */
#define RDX_RB_EMPTY_NODE(node)  \
	((node)->__rb_parent_color == (unsigned long)(node))
#define RDX_RB_CLEAR_NODE(node)  \
	((node)->__rb_parent_color = (unsigned long)(node))


extern void rdx_rb_insert_color(struct rdx_rb_node *, struct rdx_rb_root *);
extern void rdx_rb_erase(struct rdx_rb_node *, struct rdx_rb_root *);


/* Find logical next and previous nodes in a tree */
extern struct rdx_rb_node *rdx_rb_next(const struct rdx_rb_node *);
extern struct rdx_rb_node *rdx_rb_prev(const struct rdx_rb_node *);
extern struct rdx_rb_node *rdx_rb_first(const struct rdx_rb_root *);
extern struct rdx_rb_node *rdx_rb_last(const struct rdx_rb_root *);

/* Postorder iteration - always visit the parent after its children */
extern struct rdx_rb_node *rdx_rb_first_postorder(const struct rdx_rb_root *);
extern struct rdx_rb_node *rdx_rb_next_postorder(const struct rdx_rb_node *);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void rdx_rb_replace_node(struct rdx_rb_node *victim, struct rdx_rb_node *new, 
				struct rdx_rb_root *root);

static inline void rdx_rb_link_node(struct rdx_rb_node * node, struct rdx_rb_node * parent,
				    struct rdx_rb_node ** rb_link)
{
	node->__rb_parent_color = (unsigned long)parent;
	node->rb_left = node->rb_right = NULL;

	*rb_link = node;
}

#define rdx_rb_entry_safe(ptr, type, member) \
	({ typeof(ptr) ____ptr = (ptr); \
	   ____ptr ? rb_entry(____ptr, type, member) : NULL; \
	})

/**
 * rdx_rbtree_postorder_for_each_entry_safe - iterate over rdx_rb_root in post order of
 * given type safe against removal of rdx_rb_node entry
 *
 * @pos:	the 'type *' to use as a loop cursor.
 * @n:		another 'type *' to use as temporary storage
 * @root:	'rdx_rb_root *' of the rbtree.
 * @field:	the name of the rb_node field within 'type'.
 */
#define rdx_rbtree_postorder_for_each_entry_safe(pos, n, root, field) \
	for (pos = rdx_rb_entry_safe(rdx_rb_first_postorder(root), typeof(*pos), field); \
	     pos && ({ n = rdx_rb_entry_safe(rdx_rb_next_postorder(&pos->field), \
			typeof(*pos), field); 1; }); \
	     pos = n)

extern struct rdx_rb_node *rdx_rb_find(struct rdx_rb_root *root, struct rdx_rb_node *elem);
extern struct rdx_rb_node *rdx_rb_insert(struct rdx_rb_root *root, struct rdx_rb_node *elem);
extern struct rdx_rb_node *rdx_rb_rightmost_less_equiv(struct rdx_rb_root *root, struct rdx_rb_node *elem);
extern struct rdx_rb_node *rdx_rb_leftmost_greater_equiv(struct rdx_rb_root *root, struct rdx_rb_node *elem);
#endif	/* _RDX_RBTREE_H */
