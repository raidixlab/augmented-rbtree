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

  linux/lib/rbtree.c
*/

#include "rbtree_augmented.h"
// #include <linux/export.h>

/*
 * red-black trees properties:  http://en.wikipedia.org/wiki/Rbtree
 *
 *  1) A node is either red or black
 *  2) The root is black
 *  3) All leaves (NULL) are black
 *  4) Both children of every red node are black
 *  5) Every simple path from root to leaves contains the same number
 *     of black nodes.
 *
 *  4 and 5 give the O(log n) guarantee, since 4 implies you cannot have two
 *  consecutive red nodes in a path and every red node is therefore followed by
 *  a black. So if B is the number of black nodes on every simple path (as per
 *  5), then the longest possible path due to 4 is 2B.
 *
 *  We shall indicate color with case, where black nodes are uppercase and red
 *  nodes will be lowercase. Unknown color nodes shall be drawn as red within
 *  parentheses and have some accompanying text comment.
 */

static inline void rdx_rb_set_black(struct rdx_rb_node *rb)
{
	rb->__rb_parent_color |= RDX_RB_BLACK;
}

static inline struct rdx_rb_node *rdx_rb_red_parent(struct rdx_rb_node *red)
{
	return (struct rdx_rb_node *)red->__rb_parent_color;
}

/*
 * Helper function for rotations:
 * - old's parent and color get assigned to new
 * - old gets assigned new as a parent and 'color' as a color.
 */
static inline void
__rdx_rb_rotate_set_parents(struct rdx_rb_node *old, struct rdx_rb_node *new,
			    struct rdx_rb_root *root, int color)
{
	struct rdx_rb_node *parent = rdx_rb_parent(old);
	new->__rb_parent_color = old->__rb_parent_color;
	rdx_rb_set_parent_color(old, new, color);
	__rdx_rb_change_child(old, new, parent, root);
}

static __always_inline void
__rdx_rb_insert(struct rdx_rb_node *node, struct rdx_rb_root *root,
		void (*augment_rotate)(struct rdx_rb_node *old, struct rdx_rb_node *new))
{
	struct rdx_rb_node *parent = rdx_rb_red_parent(node), *gparent, *tmp;

	while (true) {
		/*
		 * Loop invariant: node is red
		 *
		 * If there is a black parent, we are done.
		 * Otherwise, take some corrective action as we don't
		 * want a red root or two consecutive red nodes.
		 */
		if (!parent) {
			rdx_rb_set_parent_color(node, NULL, RDX_RB_BLACK);
			break;
		} else if (rdx_rb_is_black(parent))
			break;

		gparent = rdx_rb_red_parent(parent);

		tmp = gparent->rb_right;
		if (parent != tmp) {	/* parent == gparent->rb_left */
			if (tmp && rdx_rb_is_red(tmp)) {
				/*
				 * Case 1 - color flips
				 *
				 *       G            g
				 *      / \          / \
				 *     p   u  -->   P   U
				 *    /            /
				 *   n            n
				 *
				 * However, since g's parent might be red, and
				 * 4) does not allow this, we need to recurse
				 * at g.
				 */
				rdx_rb_set_parent_color(tmp, gparent, RDX_RB_BLACK);
				rdx_rb_set_parent_color(parent, gparent, RDX_RB_BLACK);
				node = gparent;
				parent = rdx_rb_parent(node);
				rdx_rb_set_parent_color(node, parent, RDX_RB_RED);
				continue;
			}

			tmp = parent->rb_right;
			if (node == tmp) {
				/*
				 * Case 2 - left rotate at parent
				 *
				 *      G             G
				 *     / \           / \
				 *    p   U  -->    n   U
				 *     \           /
				 *      n         p
				 *
				 * This still leaves us in violation of 4), the
				 * continuation into Case 3 will fix that.
				 */
				parent->rb_right = tmp = node->rb_left;
				node->rb_left = parent;
				if (tmp)
					rdx_rb_set_parent_color(tmp, parent,
								RDX_RB_BLACK);
				rdx_rb_set_parent_color(parent, node, RDX_RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_right;
			}

			/*
			 * Case 3 - right rotate at gparent
			 *
			 *        G           P
			 *       / \         / \
			 *      p   U  -->  n   g
			 *     /                 \
			 *    n                   U
			 */
			gparent->rb_left = tmp;  /* == parent->rb_right */
			parent->rb_right = gparent;
			if (tmp)
				rdx_rb_set_parent_color(tmp, gparent, RDX_RB_BLACK);
			__rdx_rb_rotate_set_parents(gparent, parent, root, RDX_RB_RED);
			augment_rotate(gparent, parent);
			break;
		} else {
			tmp = gparent->rb_left;
			if (tmp && rdx_rb_is_red(tmp)) {
				/* Case 1 - color flips */
				rdx_rb_set_parent_color(tmp, gparent, RDX_RB_BLACK);
				rdx_rb_set_parent_color(parent, gparent, RDX_RB_BLACK);
				node = gparent;
				parent = rdx_rb_parent(node);
				rdx_rb_set_parent_color(node, parent, RDX_RB_RED);
				continue;
			}

			tmp = parent->rb_left;
			if (node == tmp) {
				/* Case 2 - right rotate at parent */
				parent->rb_left = tmp = node->rb_right;
				node->rb_right = parent;
				if (tmp)
					rdx_rb_set_parent_color(tmp, parent,
								RDX_RB_BLACK);
				rdx_rb_set_parent_color(parent, node, RDX_RB_RED);
				augment_rotate(parent, node);
				parent = node;
				tmp = node->rb_left;
			}

			/* Case 3 - left rotate at gparent */
			gparent->rb_right = tmp;  /* == parent->rb_left */
			parent->rb_left = gparent;
			if (tmp)
				rdx_rb_set_parent_color(tmp, gparent, RDX_RB_BLACK);
			__rdx_rb_rotate_set_parents(gparent, parent, root, RDX_RB_RED);
			augment_rotate(gparent, parent);
			break;
		}
	}
}

/*
 * Inline version for rb_erase() use - we want to be able to inline
 * and eliminate the dummy_rotate callback there
 */
static __always_inline void
____rdx_rb_erase_color(struct rdx_rb_node *parent, struct rdx_rb_root *root,
	void (*augment_rotate)(struct rdx_rb_node *old, struct rdx_rb_node *new))
{
	struct rdx_rb_node *node = NULL, *sibling, *tmp1, *tmp2;

	while (true) {
		/*
		 * Loop invariants:
		 * - node is black (or NULL on first iteration)
		 * - node is not the root (parent is not NULL)
		 * - All leaf paths going through parent and node have a
		 *   black node count that is 1 lower than other leaf paths.
		 */
		sibling = parent->rb_right;
		if (node != sibling) {	/* node == parent->rb_left */
			if (rdx_rb_is_red(sibling)) {
				/*
				 * Case 1 - left rotate at parent
				 *
				 *     P               S
				 *    / \             / \
				 *   N   s    -->    p   Sr
				 *      / \         / \
				 *     Sl  Sr      N   Sl
				 */
				parent->rb_right = tmp1 = sibling->rb_left;
				sibling->rb_left = parent;
				rdx_rb_set_parent_color(tmp1, parent, RDX_RB_BLACK);
				__rdx_rb_rotate_set_parents(parent, sibling, root,
							    RDX_RB_RED);
				augment_rotate(parent, sibling);
				sibling = tmp1;
			}
			tmp1 = sibling->rb_right;
			if (!tmp1 || rdx_rb_is_black(tmp1)) {
				tmp2 = sibling->rb_left;
				if (!tmp2 || rdx_rb_is_black(tmp2)) {
					/*
					 * Case 2 - sibling color flip
					 * (p could be either color here)
					 *
					 *    (p)           (p)
					 *    / \           / \
					 *   N   S    -->  N   s
					 *      / \           / \
					 *     Sl  Sr        Sl  Sr
					 *
					 * This leaves us violating 5) which
					 * can be fixed by flipping p to black
					 * if it was red, or by recursing at p.
					 * p is red when coming from Case 1.
					 */
					rdx_rb_set_parent_color(sibling, parent,
								RDX_RB_RED);
					if (rdx_rb_is_red(parent))
						rdx_rb_set_black(parent);
					else {
						node = parent;
						parent = rdx_rb_parent(node);
						if (parent)
							continue;
					}
					break;
				}
				/*
				 * Case 3 - right rotate at sibling
				 * (p could be either color here)
				 *
				 *   (p)           (p)
				 *   / \           / \
				 *  N   S    -->  N   Sl
				 *     / \             \
				 *    sl  Sr            s
				 *                       \
				 *                        Sr
				 */
				sibling->rb_left = tmp1 = tmp2->rb_right;
				tmp2->rb_right = sibling;
				parent->rb_right = tmp2;
				if (tmp1)
					rdx_rb_set_parent_color(tmp1, sibling,
								RDX_RB_BLACK);
				augment_rotate(sibling, tmp2);
				tmp1 = sibling;
				sibling = tmp2;
			}
			/*
			 * Case 4 - left rotate at parent + color flips
			 * (p and sl could be either color here.
			 *  After rotation, p becomes black, s acquires
			 *  p's color, and sl keeps its color)
			 *
			 *      (p)             (s)
			 *      / \             / \
			 *     N   S     -->   P   Sr
			 *        / \         / \
			 *      (sl) sr      N  (sl)
			 */
			parent->rb_right = tmp2 = sibling->rb_left;
			sibling->rb_left = parent;
			rdx_rb_set_parent_color(tmp1, sibling, RDX_RB_BLACK);
			if (tmp2)
				rdx_rb_set_parent(tmp2, parent);
			__rdx_rb_rotate_set_parents(parent, sibling, root,
						    RDX_RB_BLACK);
			augment_rotate(parent, sibling);
			break;
		} else {
			sibling = parent->rb_left;
			if (rdx_rb_is_red(sibling)) {
				/* Case 1 - right rotate at parent */
				parent->rb_left = tmp1 = sibling->rb_right;
				sibling->rb_right = parent;
				rdx_rb_set_parent_color(tmp1, parent, RDX_RB_BLACK);
				__rdx_rb_rotate_set_parents(parent, sibling, root,
							    RDX_RB_RED);
				augment_rotate(parent, sibling);
				sibling = tmp1;
			}
			tmp1 = sibling->rb_left;
			if (!tmp1 || rdx_rb_is_black(tmp1)) {
				tmp2 = sibling->rb_right;
				if (!tmp2 || rdx_rb_is_black(tmp2)) {
					/* Case 2 - sibling color flip */
					rdx_rb_set_parent_color(sibling, parent,
								RDX_RB_RED);
					if (rdx_rb_is_red(parent))
						rdx_rb_set_black(parent);
					else {
						node = parent;
						parent = rdx_rb_parent(node);
						if (parent)
							continue;
					}
					break;
				}
				/* Case 3 - right rotate at sibling */
				sibling->rb_right = tmp1 = tmp2->rb_left;
				tmp2->rb_left = sibling;
				parent->rb_left = tmp2;
				if (tmp1)
					rdx_rb_set_parent_color(tmp1, sibling,
								RDX_RB_BLACK);
				augment_rotate(sibling, tmp2);
				tmp1 = sibling;
				sibling = tmp2;
			}
			/* Case 4 - left rotate at parent + color flips */
			parent->rb_left = tmp2 = sibling->rb_right;
			sibling->rb_right = parent;
			rdx_rb_set_parent_color(tmp1, sibling, RDX_RB_BLACK);
			if (tmp2)
				rdx_rb_set_parent(tmp2, parent);
			__rdx_rb_rotate_set_parents(parent, sibling, root,
						    RDX_RB_BLACK);
			augment_rotate(parent, sibling);
			break;
		}
	}
}

/* Non-inline version for rb_erase_augmented() use */
void __rdx_rb_erase_color(struct rdx_rb_node *parent, struct rdx_rb_root *root,
	void (*augment_rotate)(struct rdx_rb_node *old, struct rdx_rb_node *new))
{
	____rdx_rb_erase_color(parent, root, augment_rotate);
}
// EXPORT_SYMBOL(__rdx_rb_erase_color);

/*
 * Non-augmented rbtree manipulation functions.
 *
 * We use dummy augmented callbacks here, and have the compiler optimize them
 * out of the rb_insert_color() and rb_erase() function definitions.
 */

static inline void dummy_propagate(struct rdx_rb_node *node, struct rdx_rb_node *stop) {}
static inline void dummy_copy(struct rdx_rb_node *old, struct rdx_rb_node *new) {}
static inline void dummy_rotate(struct rdx_rb_node *old, struct rdx_rb_node *new) {}

static const struct rdx_rb_augment_callbacks dummy_callbacks = {
	dummy_propagate, dummy_copy, dummy_rotate
};

void rdx_rb_insert_color(struct rdx_rb_node *node, struct rdx_rb_root *root)
{
	__rdx_rb_insert(node, root, dummy_rotate);
}
// EXPORT_SYMBOL(rdx_rb_insert_color);

void rdx_rb_erase(struct rdx_rb_node *node, struct rdx_rb_root *root)
{
	struct rdx_rb_node *rebalance;
	rebalance = __rdx_rb_erase_augmented(node, root, &dummy_callbacks);
	if (rebalance)
		____rdx_rb_erase_color(rebalance, root, dummy_rotate);
}
// EXPORT_SYMBOL(rdx_rb_erase);

/*
 * Augmented rbtree manipulation functions.
 *
 * This instantiates the same __always_inline functions as in the non-augmented
 * case, but this time with user-defined callbacks.
 */

void __rdx_rb_insert_augmented(struct rdx_rb_node *node, struct rdx_rb_root *root,
	void (*augment_rotate)(struct rdx_rb_node *old, struct rdx_rb_node *new))
{
	__rdx_rb_insert(node, root, augment_rotate);
}
// EXPORT_SYMBOL(__rdx_rb_insert_augmented);

/*
 * This function returns the first node (in sort order) of the tree.
 */
struct rdx_rb_node *rdx_rb_first(const struct rdx_rb_root *root)
{
	struct rdx_rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}
// EXPORT_SYMBOL(rdx_rb_first);

struct rdx_rb_node *rdx_rb_last(const struct rdx_rb_root *root)
{
	struct rdx_rb_node	*n;

	n = root->rb_node;
	if (!n)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}
// EXPORT_SYMBOL(rdx_rb_last);

struct rdx_rb_node *rdx_rb_next(const struct rdx_rb_node *node)
{
	struct rdx_rb_node *parent;

	if (RDX_RB_EMPTY_NODE(node))
		return NULL;

	/*
	 * If we have a right-hand child, go down and then left as far
	 * as we can.
	 */
	if (node->rb_right) {
		node = node->rb_right; 
		while (node->rb_left)
			node = node->rb_left;
		return (struct rdx_rb_node *)node;
	}

	/*
	 * No right-hand children. Everything down and left is smaller than us,
	 * so any 'next' node must be in the general direction of our parent.
	 * Go up the tree; any time the ancestor is a right-hand child of its
	 * parent, keep going up. First time it's a left-hand child of its
	 * parent, said parent is our 'next' node.
	 */
	while ((parent = rdx_rb_parent(node)) && node == parent->rb_right)
		node = parent;

	return parent;
}
// EXPORT_SYMBOL(rdx_rb_next);

struct rdx_rb_node *rdx_rb_prev(const struct rdx_rb_node *node)
{
	struct rdx_rb_node *parent;

	if (RDX_RB_EMPTY_NODE(node))
		return NULL;

	/*
	 * If we have a left-hand child, go down and then right as far
	 * as we can.
	 */
	if (node->rb_left) {
		node = node->rb_left; 
		while (node->rb_right)
			node=node->rb_right;
		return (struct rdx_rb_node *)node;
	}

	/*
	 * No left-hand children. Go up till we find an ancestor which
	 * is a right-hand child of its parent.
	 */
	while ((parent = rdx_rb_parent(node)) && node == parent->rb_left)
		node = parent;

	return parent;
}
// EXPORT_SYMBOL(rdx_rb_prev);

void rdx_rb_replace_node(struct rdx_rb_node *victim, struct rdx_rb_node *new,
			 struct rdx_rb_root *root)
{
	struct rdx_rb_node *parent = rdx_rb_parent(victim);

	/* Set the surrounding nodes to point to the replacement */
	__rdx_rb_change_child(victim, new, parent, root);
	if (victim->rb_left)
		rdx_rb_set_parent(victim->rb_left, new);
	if (victim->rb_right)
		rdx_rb_set_parent(victim->rb_right, new);

	/* Copy the pointers/colour from the victim to the replacement */
	*new = *victim;
}
// EXPORT_SYMBOL(rdx_rb_replace_node);

static struct rdx_rb_node *rdx_rb_left_deepest_node(const struct rdx_rb_node *node)
{
	for (;;) {
		if (node->rb_left)
			node = node->rb_left;
		else if (node->rb_right)
			node = node->rb_right;
		else
			return (struct rdx_rb_node *)node;
	}
}

struct rdx_rb_node *rdx_rb_next_postorder(const struct rdx_rb_node *node)
{
	const struct rdx_rb_node *parent;
	if (!node)
		return NULL;
	parent = rdx_rb_parent(node);

	/* If we're sitting on node, we've already seen our children */
	if (parent && node == parent->rb_left && parent->rb_right) {
		/* If we are the parent's left node, go to the parent's right
		 * node then all the way down to the left */
		return rdx_rb_left_deepest_node(parent->rb_right);
	} else
		/* Otherwise we are the parent's right node, and the parent
		 * should be next */
		return (struct rdx_rb_node *)parent;
}
// EXPORT_SYMBOL(rdx_rb_next_postorder);

struct rdx_rb_node *rdx_rb_first_postorder(const struct rdx_rb_root *root)
{
	if (!root->rb_node)
		return NULL;

	return rdx_rb_left_deepest_node(root->rb_node);
}
// EXPORT_SYMBOL(rdx_rb_first_postorder);

struct rdx_rb_node *rdx_rb_find(struct rdx_rb_root *root, struct rdx_rb_node *elem)
{
	struct rdx_rb_node *node = root->rb_node;
	while (node) {
		int result;
		result = root->strict_compare(elem, node);
		if (result < 0) {
			node = node->rb_left;
		} else if (result > 0) {
			node = node->rb_right;
		} else {
			return node;
		}
	}
	return NULL;
}

struct rdx_rb_node *rdx_rb_insert(struct rdx_rb_root *root, struct rdx_rb_node *elem)
{
	struct rdx_rb_node **new = &(root->rb_node), *parent = NULL;
	while (*new) {
		int result = root->strict_compare(elem, *new);
		parent = *new;
		if (result < 0) {
			new = &((*new)->rb_left);
		} else if (result > 0) {
			new = &((*new)->rb_right);
		} else {
			return NULL;
		}
	}
	rdx_rb_link_node(elem, parent, new);
	return elem;
}

struct rdx_rb_node *rdx_rb_rightmost_less_equiv(struct rdx_rb_root *root, struct rdx_rb_node *elem)
{
	struct rdx_rb_node *node = root->rb_node;
	struct rdx_rb_node *result_node = NULL;
	while (node) {
		int result;
		result = root->weak_compare(node, elem);
		if (result < 0 || result == 0) {
			result_node = node;
			node = node->rb_right;
		} else {
			node = node->rb_left;
		}
	}
	return result_node;
}

struct rdx_rb_node *rdx_rb_leftmost_greater_equiv(struct rdx_rb_root *root, struct rdx_rb_node *elem)
{
	struct rdx_rb_node *node = root->rb_node;
	struct rdx_rb_node *result_node = NULL;
	while (node) {
		int result;
		result = root->weak_compare(node, elem);
		if (result > 0 || result == 0) {
			result_node = node;
			node = node->rb_left;
		} else {
			node = node->rb_right;
		}
	}
	return result_node;
}

