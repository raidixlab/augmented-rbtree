#include <stdio.h>
#include <stdlib.h>

#include "rbtree_augmented.h"

int verbose = false;

struct avg_payload
{
	size_t count;
};

struct my_node
{
	long long weak_key;
	long long strict_key;
	struct avg_payload payload;
	struct rdx_rb_node node;
};

struct avg_payload construct_payload()
{
	return (struct avg_payload){ .count = 1 };
}

struct avg_payload combine_payloads(struct avg_payload *a,
				    struct avg_payload *b)
{
	struct avg_payload result = { .count = a->count + b->count };
	return result;
}

struct my_node *construct_node(long long strict_key, long long weak_key)
{
	struct my_node* data = (struct my_node*)malloc(sizeof(*data));
	if (data != NULL) {
		data->strict_key = strict_key;
		data->weak_key = weak_key;
		data->payload = construct_payload();
	}
	return data;
}

void free_node(struct my_node *data)
{
	free(data);
}

int weak_compare(struct my_node *left, struct my_node *right)
{
	if (left->weak_key < right->weak_key) {
		return -1;
	} else if (left->weak_key == right->weak_key) {
		return 0;
	} else {
		return 1;
	}
}

int strict_compare(struct my_node *left, struct my_node *right)
{
	int weak_result = weak_compare(left, right);
	if (weak_result) {
		return weak_result;
	} else if (left->strict_key < right->strict_key) {
		return -1;
	} else if (left->strict_key == right->strict_key) {
		return 0;
	} else {
		return 1;
	}
}

int weak_compare_rb(struct rdx_rb_node *left, struct rdx_rb_node *right)
{
	struct my_node *my_left = container_of(left, struct my_node, node);
	struct my_node *my_right = container_of(right, struct my_node, node);
	return weak_compare(my_left, my_right);
}

int strict_compare_rb(struct rdx_rb_node *left, struct rdx_rb_node *right)
{
	struct my_node *my_left = container_of(left, struct my_node, node);
	struct my_node *my_right = container_of(right, struct my_node, node);
	return strict_compare(my_left, my_right);
}

void print_node(struct my_node *node)
{
	printf("%lli %lli {%zu}", node->strict_key, node->weak_key, node->payload.count);
}

struct avg_payload compute_payload(struct my_node *data)
{
	if (verbose) {
		printf("Compute payload ");
		print_node(data);
		printf("\n");
	}
	struct avg_payload result = construct_payload();
	if (data->node.rb_left)
		result = combine_payloads(&result, &(rdx_rb_entry(data->node.rb_left, struct my_node, node)->payload));
	if (data->node.rb_right)
		result = combine_payloads(&result, &(rdx_rb_entry(data->node.rb_right, struct my_node, node)->payload));
	return result;
}

int payloads_equal(struct avg_payload *a, struct avg_payload *b)
{
	return a->count == b->count;
}

RDX_RB_DECLARE_CALLBACKS(static, payload_callbacks, struct my_node,	\
			 node, struct avg_payload, payload,		\
			 compute_payload, my_node_mmap);

void print_subtree(struct rdx_rb_node *node, int offset)
{
	if (node) {
		print_subtree(node->rb_left, offset + 1);
		struct my_node *data =
			rdx_rb_entry(node, struct my_node, node);
		for (int i = 0; i < offset; i++)
			printf("\t");
		print_node(data);
		printf("\n");
		print_subtree(node->rb_right, offset + 1);
	}
}

void print_items(struct rdx_rb_root *tree)
{
	print_subtree(tree->rb_node, 0);
}

int is_consistent_node(struct rdx_rb_node *node) {
	if (node) {
		struct my_node *data =
			rdx_rb_entry(node, struct my_node, node);
		struct avg_payload needed_payload = compute_payload(data);
		return payloads_equal(&data->payload, &needed_payload) &&
			is_consistent_node(node->rb_left) && is_consistent_node(node->rb_right);
	} else {
		return true;
	}
}

int is_consistent_tree(struct rdx_rb_root *root) {
	if (verbose) {
		printf("Consistency check\n");
	}
	int result = is_consistent_node(root->rb_node);
	if (verbose) {
		printf("Consistensy end check\n");
	}
	return result;
}

struct rdx_rb_node *random_node(struct rdx_rb_root *root, size_t nodes_count) {
	if (nodes_count == 0) {
		return (struct rdx_rb_node *)NULL;
	}
	struct rdx_rb_node *it = rdx_rb_first(root);
	size_t node_number = rand() % nodes_count;
	for (size_t i = 0; i < node_number; i++) {
		it = rdx_rb_next(it);
	}
	return it;
}

int test_insert(struct my_node *node, struct rdx_rb_root *tree,
		int expected_result)
{
	printf("Insert ");
	print_node(node);
	printf("\n");

	int result = my_node_mmap_insert(node, tree) == expected_result;

	if (!is_consistent_tree(tree)) {
		verbose = false;
		printf("Tree is inconsistent\n");
		print_items(tree);
		result = false;
	}

	return result;
}

int test_erase(struct my_node *victim, struct rdx_rb_root *tree)
{
	printf("Erase ");
	print_node(victim);
	printf("\n");

	my_node_mmap_erase(victim, tree);

	if (!is_consistent_tree(tree)) {
		printf("Tree is inconsistent\n");
		return false;
	}

	return true;
}

int test_rightmost_le(struct my_node *node, struct rdx_rb_root *tree,
		      struct my_node *expected_result)
{
	printf("Find LE ");
	print_node(node);
	printf("\n");

	return my_node_mmap_rightmost_less_equiv(node, tree) ==
	       expected_result;
}

int test_leftmost_ge(struct my_node *node, struct rdx_rb_root *tree,
		     struct my_node *expected_result)
{
	printf("Find GE ");
	print_node(node);
	printf("\n");

	return my_node_mmap_leftmost_greater_equiv(node, tree) ==
	       expected_result;
}

#define DECLARE_NODE(strict_key, weak_key)			\
	struct my_node *n_ ## strict_key ## _ ## weak_key =	\
		construct_node(strict_key, weak_key)		\

#define TRY(expr) if (!(expr)) {	\
		printf("Fail\n\n");	\
		return 1;		\
	} else {			\
		printf("OK\n\n");		\
	}

int main()
{
	struct rdx_rb_root tree =
		RDX_RB_ROOT(strict_compare_rb, weak_compare_rb);

	struct my_node *null = (struct my_node *)NULL;

	DECLARE_NODE(0, 2);
	DECLARE_NODE(1, 1);
	DECLARE_NODE(2, 3);
	DECLARE_NODE(3, 1);
	DECLARE_NODE(4, 3);
	DECLARE_NODE(5, 4);
	DECLARE_NODE(6, 0);

	TRY(test_insert(n_0_2, &tree, true));
	TRY(test_insert(n_1_1, &tree, true));
	TRY(test_insert(n_2_3, &tree, true));
	TRY(test_insert(n_3_1, &tree, true));
	TRY(test_insert(n_4_3, &tree, true));
	TRY(test_insert(n_5_4, &tree, true));
	TRY(test_insert(n_6_0, &tree, true));

	TRY(test_erase(n_0_2, &tree));
	TRY(test_erase(n_6_0, &tree));

	TRY(test_rightmost_le(n_2_3, &tree, n_4_3));
	TRY(test_rightmost_le(n_4_3, &tree, n_4_3));
	TRY(test_rightmost_le(n_6_0, &tree, null));
	TRY(test_rightmost_le(n_0_2, &tree, n_3_1));

	TRY(test_erase(n_5_4, &tree));

	TRY(test_leftmost_ge(n_1_1, &tree, n_1_1));
	TRY(test_leftmost_ge(n_3_1, &tree, n_1_1));
	TRY(test_leftmost_ge(n_5_4, &tree, null));
	TRY(test_leftmost_ge(n_0_2, &tree, n_2_3));

	printf("All tests OK\n");

	return 0;
}
