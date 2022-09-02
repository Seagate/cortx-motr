CC=gcc

MOTR_PATH=/root/motr-true-bulk-rebased
LUSTRE_PATH=/usr/src/lustre-2.7.18.4-headers

CFLAGS=-g -std=gnu99 -Wall -Werror -Wno-attributes -Wno-unused-variable			\
-Wno-unused-but-set-variable -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL=''		\
-DM0_EXTERN=extern -fno-strict-aliasing -fno-omit-frame-pointer -fno-common	\
-fPIC

INCLUDE_FLAGS=-include config.h -I$(MOTR_PATH) -I$(LUSTRE_PATH)/lnet/include	\
-I$(LUSTRE_PATH)/lustre/include

LDFLAGS=-L$(MOTR_PATH)/extra-libs/gf-complete/src/.libs -L$(MOTR_PATH)/motr/.libs -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmotr

OBJS=src/main.o

m0inst: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDE_FLAGS) -o $@ $<
