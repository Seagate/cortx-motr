#ifndef _SIMPLE_H_
#define _SIMPLE_H_


int init(char *ha_addr, char *local_addr, char *profile, char *process_fid);

int index_create(long long index_id);

int index_delete(long long index_id);


int index_put();

int index_get();

#endif 