/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2002  David Woodhouse <dwmw2@infradead.org>
  (C) 2012  Michel Lespinasse <walken@google.com>

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

  linux/include/linux/rbtree_augmented.h
*/

#ifndef _RDX_RBTREE_AUGMENTED_H
#define _RDX_RBTREE_AUGMENTED_H

// #include <linux/compiler.h>
#include "compiler.h"
#include "rbtree.h"

/*
 * Please note - only struct rb_augment_callbacks and the prototypes for
 * rb_insert_augmented() and rb_erase_augmented() are intended to be public.
 * The rest are implementation details you are not expected to depend on.
 *
 * See Documentation/rbtree.txt for documentation and samples.
 */

struct rdx_rb_augment_callbacks {
	void (*propagate)(struct rdx_rb_node *node, struct rdx_rb_node *stop);
	void (*copy)(struct rdx_rb_node *old, struct rdx_rb_node *new);
	void (*rotate)(struct rdx_rb_node *old, struct rdx_rb_node *new);
};

extern void __rdx_rb_insert_augmented(struct rdx_rb_node *node, struct rdx_rb_root *root,
	void (*augment_rotate)(struct rdx_rb_node *old, struct rdx_rb_node *new));
/*
 * Fixup the rbtree and update the augmented information when rebalancing.
 *
 * On insertion, the user must update the augmented information on the path
 * leading to the inserted node, then call rdx_rb_link_node() as usual and
 * rdx_rb_augment_inserted() instead of the usual rdx_rb_insert_color() call.
 * If rdx_rb_augment_inserted() rebalances the rbtree, it will callback into
 * a user provided function to update the augmented information on the
 * affected subtrees.
 */
static inline void
rdx_rb_insert_augmented(struct rdx_rb_node *node, struct rdx_rb_root *root,
		    const struct rdx_rb_augment_callbacks *augment)
{
	__rdx_rb_insert_augmented(node, root, augment->rotate);
	augment->propagate(node, NULL);
}

#define RDX_RB_DECLARE_CALLBACKS(rbstatic, rbname, rbstruct, rbfield,		\
				 rbtype, rbaugmented, rbcompute) 		\
static inline void								\
rbname ## _propagate(struct rdx_rb_node *rb, struct rdx_rb_node *stop)		\
{										\
	while (rb != stop) {							\
		rbstruct *node = rdx_rb_entry(rb, rbstruct, rbfield);		\
		rbtype augmented = rbcompute(node);				\
		node->rbaugmented = augmented;					\
		rb = rdx_rb_parent(&node->rbfield);				\
	}									\
}										\
static inline void								\
rbname ## _copy(struct rdx_rb_node *rb_old, struct rdx_rb_node *rb_new)		\
{										\
	rbstruct *old = rdx_rb_entry(rb_old, rbstruct, rbfield);		\
	rbstruct *new = rdx_rb_entry(rb_new, rbstruct, rbfield);		\
	new->rbaugmented = old->rbaugmented;					\
}										\
static void									\
rbname ## _rotate(struct rdx_rb_node *rb_old, struct rdx_rb_node *rb_new)	\
{										\
	rbstruct *old = rdx_rb_entry(rb_old, rbstruct, rbfield);		\
	rbstruct *new = rdx_rb_entry(rb_new, rbstruct, rbfield);		\
	old->rbaugmented = rbcompute(old);					\
	new->rbaugmented = rbcompute(new);					\
}										\
rbstatic const struct rdx_rb_augment_callbacks rbname = {			\
	rbname ## _propagate, rbname ## _copy, rbname ## _rotate		\
};


#define	RDX_RB_RED	0
#define	RDX_RB_BLACK	1

#define __rdx_rb_parent(pc)    ((struct rdx_rb_node *)(pc & ~3))

#define __rdx_rb_color(pc)     ((pc) & 1)
#define __rdx_rb_is_black(pc)  __rdx_rb_color(pc)
#define __rdx_rb_is_red(pc)    (!__rdx_rb_color(pc))
#define rdx_rb_color(rb)       __rdx_rb_color((rb)->__rb_parent_color)
#define rdx_rb_is_red(rb)      __rdx_rb_is_red((rb)->__rb_parent_color)
#define rdx_rb_is_black(rb)    __rdx_rb_is_black((rb)->__rb_parent_color)

static inline void rdx_rb_set_parent(struct rdx_rb_node *rb, struct rdx_rb_node *p)
{
	rb->__rb_parent_color = rdx_rb_color(rb) | (unsigned long)p;
}

static inline void rdx_rb_set_parent_color(struct rdx_rb_node *rb,
					   struct rdx_rb_node *p, int color)
{
	rb->__rb_parent_color = (unsigned long)p | color;
}

static inline void
__rdx_rb_change_child(struct rdx_rb_node *old, struct rdx_rb_node *new,
		      struct rdx_rb_node *parent, struct rdx_rb_root *root)
{
	if (parent) {
		if (parent->rb_left == old)
			parent->rb_left = new;
		else
			parent->rb_right = new;
	} else
		root->rb_node = new;
}

extern void __rdx_rb_erase_color(struct rdx_rb_node *parent, struct rdx_rb_root *root,
	void (*augment_rotate)(struct rdx_rb_node *old, struct rdx_rb_node *new));

static __always_inline struct rdx_rb_node *
__rdx_rb_erase_augmented(struct rdx_rb_node *node, struct rdx_rb_root *root,
			 const struct rdx_rb_augment_callbacks *augment)
{
	struct rdx_rb_node *child = node->rb_right, *tmp = node->rb_left;
	struct rdx_rb_node *parent, *rebalance;
	unsigned long pc;

	if (!tmp) {
		/*
		 * Case 1: node to erase has no more than 1 child (easy!)
		 *
		 * Note that if there is one child it must be red due to 5)
		 * and node must be black due to 4). We adjust colors locally
		 * so as to bypass __rdx_rb_erase_color() later on.
		 */
		pc = node->__rb_parent_color;
		parent = __rdx_rb_parent(pc);
		__rdx_rb_change_child(node, child, parent, root);
		if (child) {
			child->__rb_parent_color = pc;
			rebalance = NULL;
		} else
			rebalance = __rdx_rb_is_black(pc) ? parent : NULL;
		tmp = parent;
	} else if (!child) {
		/* Still case 1, but this time the child is node->rb_left */
		tmp->__rb_parent_color = pc = node->__rb_parent_color;
		parent = __rdx_rb_parent(pc);
		__rdx_rb_change_child(node, tmp, parent, root);
		rebalance = NULL;
		tmp = parent;
	} else {
		struct rdx_rb_node *successor = child, *child2;
		tmp = child->rb_left;
		if (!tmp) {
			/*
			 * Case 2: node's successor is its right child
			 *
			 *    (n)          (s)
			 *    / \          / \
			 *  (x) (s)  ->  (x) (c)
			 *        \
			 *        (c)
			 */
			parent = successor;
			child2 = successor->rb_right;
			augment->copy(node, successor);
		} else {
			/*
			 * Case 3: node's successor is leftmost under
			 * node's right child subtree
			 *
			 *    (n)          (s)
			 *    / \          / \
			 *  (x) (y)  ->  (x) (y)
			 *      /            /
			 *    (p)          (p)
			 *    /            /
			 *  (s)          (c)
			 *    \
			 *    (c)
			 */
			do {
				parent = successor;
				successor = tmp;
				tmp = tmp->rb_left;
			} while (tmp);
			parent->rb_left = child2 = successor->rb_right;
			successor->rb_right = child;
			rdx_rb_set_parent(child, successor);
			augment->copy(node, successor);
			augment->propagate(parent, successor);
		}

		successor->rb_left = tmp = node->rb_left;
		rdx_rb_set_parent(tmp, successor);

		pc = node->__rb_parent_color;
		tmp = __rdx_rb_parent(pc);
		__rdx_rb_change_child(node, successor, tmp, root);
		if (child2) {
			successor->__rb_parent_color = pc;
			rdx_rb_set_parent_color(child2, parent, RDX_RB_BLACK);
			rebalance = NULL;
		} else {
			unsigned long pc2 = successor->__rb_parent_color;
			successor->__rb_parent_color = pc;
			rebalance = __rdx_rb_is_black(pc2) ? parent : NULL;
		}
		tmp = successor;
	}

	augment->propagate(tmp, NULL);
	return rebalance;
}

static __always_inline void
rdx_rb_erase_augmented(struct rdx_rb_node *node, struct rdx_rb_root *root,
		       const struct rdx_rb_augment_callbacks *augment)
{
	struct rdx_rb_node *rebalance = __rdx_rb_erase_augmented(node, root, augment);
	if (rebalance) {
		__rdx_rb_erase_color(rebalance, root, augment->rotate);
	}
}

#endif	/* _RDX_RBTREE_AUGMENTED_H */
