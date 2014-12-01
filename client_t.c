#include "client_t.h"

#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

inline void client_set_id(struct client_t *target, const unsigned id) 
{
	target->cl_id = id;
}

inline void client_set_sock(struct client_t *target, const int sock) 
{
	target->cl_sock = sock;
}

inline void client_buff_clear(struct client_t *target) 
{ /* Can we do better here? */
	memset(target->cl_buff, 0, CLIENT_BUFFER_SIZE);
	target->cl_free = CLIENT_BUFFER_SIZE;
}

static void client_dump_buffer(struct client_t *target) 
{
	/* TODO */
}

void client_buff_push(struct client_t *target, char *buff, size_t sz) 
{ /* TODO: mutex */
	pthread_mutex_lock(target->cl_buff_locked);

	while (target->cl_free > 0) {
		*target->cl_write_p = *buff++;
		--sz;
		--target->cl_free;
		++target->cl_write_p;
	}

	if (target->cl_free == 0) 
		client_dump_buffer(target);
	pthread_mutex_unlock(target->cl_buff_locked);

	if (sz > 0) 
		client_buff_push(target, buff, sz);
}

struct client_t *client_init(void) 
{
	struct client_t *ret;
	ret = (struct client_t *) malloc(1 * sizeof(struct client_t)); 
	
	ret->cl_id = 0;
	ret->cl_sock = 0;
	ret->cl_write_p = ret->cl_buff;
	ret->cl_free = CLIENT_BUFFER_SIZE;
	memset(ret->cl_buff, 0, CLIENT_BUFFER_SIZE);
	pthread_mutex_init(ret->cl_buff_locked, NULL);
return ret;
}

inline void client_destroy(struct client_t *target) 
{
	close(target->cl_sock);
	pthread_mutex_destroy(target->cl_buff_locked);
	free(target);
}

