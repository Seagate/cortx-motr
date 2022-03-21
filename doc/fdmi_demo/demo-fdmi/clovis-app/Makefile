CC=gcc

MERO_PATH=/root/mero-true-bulk-rebased
LUSTRE_PATH=/usr/src/lustre-2.7.18.4-headers

CFLAGS=-g -std=gnu99 -Wall -Werror -Wno-attributes -Wno-unused-variable			\
-Wno-unused-but-set-variable -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL=''		\
-DM0_EXTERN=extern -fno-strict-aliasing -fno-omit-frame-pointer -fno-common	\
-fPIC

INCLUDE_FLAGS=-include config.h -I$(MERO_PATH) -I$(LUSTRE_PATH)/lnet/include	\
-I$(LUSTRE_PATH)/lustre/include

LDFLAGS=-L$(MERO_PATH)/extra-libs/gf-complete/src/.libs -L$(MERO_PATH)/mero/.libs -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero

OBJS=src/main.o

NID:=$(sudo lctl list_nids)

clovis-app: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDE_FLAGS) -o $@ $<

# test: clovis-app
#   ./clovis-app $(NID):12345:45:1 $(NID):12345:44:101 '<0x7000000000000001:0>' '<0x7200000000000000:0>'
# -
