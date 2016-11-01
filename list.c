/* Testing for Linux style linked-list with list_entry, etc */

#include "list.h"

struct test_s {
	struct list_head queue;
	int priv_num;
};

struct list_head q;

// int main(int argc, char* argv[]) {
// 	struct test_s t[10];
// 	int i = 0;
// 	for (i = 0; i < 10; i++){
// 		t[i].priv_num = i;
// 		// t[i].queue.prev = &t[ (i == 0) ? 0 : (i-1)].queue;
// 		// t[i].queue.next = &t[ (i == 10) ? 0 : (i+1)].queue;
// 		list_add(&t[i].queue, &q);	
// 		printf("%d: %x\n", i, &t[i]);
// 	}
// 	printf("done");

// 	struct test_s *q = list_entry(&t[2].queue, struct test_s, queue);
// 	printf("%x -> %x",q,  q->priv_num);
// 	return 0;
// }
