#ifndef ROOM_DX67OSYT

#define ROOM_DX67OSYT

#include <pthread.h>
#include "client_t.h"

#define ROOM_MAX_MEMBERS 4 

struct room_t {
	unsigned rm_id;
	struct client_t **rm_members;
	size_t rm_mem_count;
	size_t rm_size; /* of members */
	pthread_mutex_t *rm_locked;
	pthread_t rm_thread;
};

/* room.c */

void room_set_id(struct room_t *, unsigned);
char room_add_member(struct room_t *, struct client_t *);
size_t room_free_space(struct room_t *);
char room_remove_member(struct room_t *, struct client_t *);

#endif /* end of include guard: ROOM_DX67OSYT */
