/**
 * @file db-debug.c
 * Debug routines for libdb
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>

#include "db.h"


static void do_display_tree(db_tree_t const * tree, db_page_idx_t page_idx)
{
	db_page_count_t i;
	db_page_t * page;

	if (page_idx == db_nil_page)
		return;

	page = page_nr_to_page_ptr(tree, page_idx);

	do_display_tree(tree, page->p0);

	for (i = 0 ; i < page->count ; ++i) {
		printf("%d\t", page->page_table[i].key);
		do_display_tree(tree, page->page_table[i].child_page);
	}
}

void db_display_tree(db_tree_t const * tree)
{
	do_display_tree(tree, tree->descr->root_idx);
}

void db_raw_display_tree(db_tree_t const * tree)
{
	db_page_count_t i;
	printf("tree root %d\n", tree->descr->root_idx);
	for (i = 1 ; i < tree->descr->current_size ; ++i) {
		db_page_t * page;
		db_page_count_t j;

		page = page_nr_to_page_ptr(tree, i);
		printf("p0: %d\n", page->p0);
		for (j = 0 ; j < page->count ; ++j) {
			printf("(%d, %d, %d)\n",
			       page->page_table[j].key,
			       page->page_table[j].info,
			       page->page_table[j].child_page);
		}
		printf("\n");
	}
}

static int do_check_page_pointer(db_tree_t const * tree,
	db_page_idx_t page_idx, int * viewed_page)
{
	int ret;
	db_page_count_t i;
	db_page_t const * page;

	if (page_idx == db_nil_page)
		return 0;

	if (page_idx >= tree->descr->current_size) {
		printf("%s:%d invalid page number, max is %d page_nr is %d\n",
		       __FILE__, __LINE__, tree->descr->current_size,
		       page_idx);
		return 1;
	}

	if (viewed_page[page_idx]) {
		printf("%s:%d child page number duplicated %d\n",
		       __FILE__, __LINE__, page_idx);
		return 1;
	}

	viewed_page[page_idx] = 1;

	page = page_nr_to_page_ptr(tree, page_idx);

	ret = do_check_page_pointer(tree, page->p0, viewed_page);
	if (ret)
		return 1;

	for (i = 0 ; i < page->count ; ++i) {
		ret |= do_check_page_pointer(tree,
					     page->page_table[i].child_page,
					     viewed_page);
		if (ret)
			return 1;
	}

	/* this is not a bug, item at pos >= page->count are in an undefined
	 * state */
/*
	for ( ; i < MAX_PAGE ; ++i) {
		if (page->page_table[i].child_page != db_nil_page) {
			ret = 1;
		}
	}
*/

	return ret;
}

int db_check_page_pointer(db_tree_t const * tree)
{
	int ret;
	int * viewed_page;

	if (tree->descr->current_size > tree->descr->size) {
		printf("%s:%d invalid current size %d, %d\n",
		       __FILE__, __LINE__,
		       tree->descr->current_size, tree->descr->size);
	}

	viewed_page = calloc(tree->descr->current_size, sizeof(int));

	ret = do_check_page_pointer(tree, tree->descr->root_idx, viewed_page);

	free(viewed_page);

	return ret;
}

static int do_check_tree(db_tree_t const * tree,
	db_page_idx_t page_nr, db_key_t last)
{
	db_page_count_t i;
	db_page_t const * page;

	page = page_nr_to_page_ptr(tree, page_nr);

	if (page->count == 0) {
		printf("%s:%d page->count == 0 at page %d\n", __FILE__, __LINE__, page_nr);
		return 1;
	}

	if (page->p0 != db_nil_page) {
		if (do_check_tree(tree, page->p0, last) != 0)
			return 1;
	}

	for (i = 0 ; i < page->count ; ++i) {
		if (page->page_table[i].key <= last) {
			printf("%s:%d page ordering wrong at index %d page %d\n", __FILE__, __LINE__, i, page_nr);
			return 1;
		}
		last = page->page_table[i].key;
		if (page->page_table[i].child_page != db_nil_page)
			if (do_check_tree(tree, page->page_table[i].child_page,
				       last))
				return 1;
	}

	return 0;
}

int db_check_tree(db_tree_t const * tree)
{
	int ret = db_check_page_pointer(tree);
	if (!ret) {
		if (tree->descr->root_idx != db_nil_page)
			ret = do_check_tree(tree, tree->descr->root_idx, 0u);
	}

	return ret;
}
