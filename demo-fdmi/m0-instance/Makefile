CC=gcc

MERO_PATH=/home/rroy/cortx/cortx-motr
LUSTRE_PATH=/usr/src/lustre-client-2.12.4

CFLAGS=-g -std=gnu99 -Wall -Werror -Wno-attributes -Wno-unused-variable			\
-Wno-unused-but-set-variable -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL=''		\
-DM0_EXTERN=extern -fno-strict-aliasing -fno-omit-frame-pointer -fno-common	\
-fPIC

INCLUDE_FLAGS=-include config.h -I$(MERO_PATH) -I$(LUSTRE_PATH)/lnet/include	\
-I$(LUSTRE_PATH)/lustre/include

LDFLAGS=-L$(MERO_PATH)/extra-libs/galois/src/.libs -L$(MERO_PATH)/mero/.libs -lm -lpthread -lrt -lgalois -lyaml -luuid -lmotr

OBJS=src/main.o

m0inst: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDE_FLAGS) -o $@ $<
