#include "client_t.h"
#include "room_t.h"

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>


inline void room_set_id(struct room_t *target, unsigned id) 
{
	target->rm_id = id;
}

static size_t check_if_member(struct room_t *target, struct client_t *victim) 
{ 
	size_t i = 0;
	while (i < target->rm_mem_count) {
		if ((*(target->rm_members + i))->cl_id == victim->cl_id) 
			return i;
		++i;
	}
return 0;
}

char room_add_member(struct room_t *target, struct client_t *victim) 
{
	int index;
	if (target->rm_mem_count >= ROOM_MAX_MEMBERS) 
		return 1;
	pthread_mutex_lock(target->rm_locked);
	if (target->rm_members == NULL) {
		/* This will probably be left unused, but is here */
		/* for precausion, in case somebody touches anything */
		target->rm_size = 4;
		target->rm_members = (struct client_t **) malloc(target->rm_size * 
						sizeof(struct client_t *)); 
	} else if (target->rm_size == target->rm_mem_count) {
		target->rm_size *= 2;
		target->rm_members = (struct client_t **) realloc( 
					target->rm_members, 
					target->rm_size);
	} 
	
	if (!check_if_member(target, victim)) {
		/**(target->rm_members + target->rm_mem_count++) = victim;*/
		index = insert_where(target->rm_members, victim->cl_id);
	}
	pthread_mutex_unlock(target->rm_locked);
	return 0;
}

static inline void room_remove_sth(struct room_t *target, size_t s) 
{
	size_t cc = s;
	--target->rm_mem_count;
	while (cc < target->rm_mem_count) 
		target->rm_members[cc] = target->rm_members[++cc];
}

char room_remove_member(struct room_t *target, struct client_t *victim) 
{
	size_t s;
	if ((s = check_if_member(target, victim))) 
		return 1;
	pthread_mutex_lock(target->rm_locked);
	room_remove_sth(target, s);
	pthread_mutex_unlock(target->rm_locked);
	return 0;
}

inline size_t room_free_space(struct room_t *target) 
{
	return ROOM_MAX_MEMBERS - target->rm_mem_count;
}

struct room_t *room_init(void)
{
	struct room_t *ret;
	ret = (struct room_t *) malloc(1 * sizeof(struct room_t)); 
	ret->rm_id = 0;
	ret->rm_members = (struct client_t *) 
			malloc(4 * sizeof(struct client_t)); 
	ret->rm_size = 4;
	ret->rm_members = 0;
	pthread_mutex_init(ret->rm_locked);
}

void room_destroy(struct room_t *target) 
{
	size_t i = 0;
	if (target == NULL) 
		return;
	while (i < target->rm_mem_count) 
		client_destroy(target->rm_members[i++]);
	free(target->rm_members);
	pthread_mutex_destroy(target->rm_locked);
}

char room_add_buff(struct room_t *target, char *buff, size_t sz) 
{ 
	size_t i = 0;
	pthread_mutex_lock(target->rm_locked);
	while (i < target->rm_mem_count) 
		client_buff_push(target->rm_members[i++], buff, sz);
	pthread_mutex_unlock(target->rm_locked);
}

