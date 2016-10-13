#include "ext2.h"

struct ide_buffer* new_buffer(struct ext2_fs* f) {
	struct ide_buffer* p = malloc(sizeof(struct ide_buffer));
	p->data = NULL;
	p->next = NULL;
	p->prev = NULL;
	p->last = NULL;
	p->head = NULL;

	p->flags = 0;
	p->block = NULL;
	p->data = malloc(f->block_size);

	return p;
}

void push(struct ext2_fs* f, struct ide_buffer* n) {
	printf("push %x onto %x\n", n, *f->cache);
	struct ide_buffer** head = f->cache;
	n->prev = (*head)->last;	/* old end-of-list is previous item */

	(*head)->last->next = n;	/* old end-of-list's next item is n */
	(*head)->last = n;			/* end-of-list is n */
	n->head = head;				/* beginning of list is head */
	n->next = NULL;
}


struct ide_buffer* pop(struct ext2_fs* f) {
	struct ide_buffer** head = f->cache;
	buffer* b = (*head)->last;
	(*head)->last = (*head)->last->prev;
	(*head)->last->next = NULL;
	return b;
}

struct ide_buffer* get_buffer(struct ext2_fs* f, int block) {
	struct ide_buffer** b;
	printf("request buffer for block %d\n", block);

	// for (b = f->cache; *b; b = &(*b)->next) {
	// 	printf("buffer %x\n", *b);
	// //	printf("traverse: %x, flags %x block %d p %x n %x h %x l %x\n", \
	// 		*b, (*b)->flags, (*b)->block, (*b)->prev, (*b)->next, *(*b)->head, (*b)->last);
	// 		if ((*b)->block == block) {
	// 			printf("match");
	// 			if (((*b)->flags & (B_BUSY | B_DIRTY)) == 0) {
	// 				printf("found match that is not dirty or in use\n");
	// 				(*b)->flags |= B_BUSY;
	// 				return *b;
	// 		}
	// 	}
	// }

	/* If we're gotten here, there are no free blocks matching */

	struct ide_buffer* new = new_buffer(f);
	new->block = block;
		printf("Making new buffer %x\n", new);
	push(f, new);
	trav(f);
	return new;
}

void trav(struct ext2_fs* f) {
	struct ide_buffer** b;
	for (b = f->cache; *b; b = &(*b)->next) {
		printf("traverse: %x, flags %x block %d p %x n %x h %x l %x\n", \
			*b, (*b)->flags, (*b)->block, (*b)->prev, (*b)->next, *(*b)->head, (*b)->last);
	}
}

void buffer_remove(struct ide_buffer** head, struct ide_buffer* n) {
	struct ide_buffer** b;
	if ((*head)->last == n) {
		pop(&head);
		free(n->data);
		free(n);
		return;
	}
	for (b = head; *b; b = &(*b)->next) {
		if (*b == n) {
			(*b)->prev->next = (*b)->next;
			(*b)->next = (*b)->next->next;

			//(*b)->prev = (*b)->prev->prev;
			free(n->data);
			free(n);
			return;
		}
	}
}

void destroy_buffer_list(struct ide_buffer** head) {
	struct ide_buffer** b;
	for (b = head; *b; b = &(*b)->next) {
			free((*b)->data);
			free((*b));
	}
	free((*head)->data);
	free((*head));
}


void buffer_init() {
	struct ext2_fs f;

	struct ide_buffer* list = new_buffer(&f);
	list->head = &list;
	list->last = list;

	for (int i = 0; i < 9; i++) {	
		buffer* b = new_buffer(&f);
		b->block = i;
		b->flags = 0;
		push(&list, b);
	}

	get_buffer(&f, 7);

	destroy_buffer_list(&list);


}