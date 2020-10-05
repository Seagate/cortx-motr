CFLAGS  = -Wall -Werror -g -fPIC -Wno-attributes
CFLAGS += -I.. -I../extra-libs/galois/include
CFLAGS += -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern
LDFLAGS = -L../motr/.libs -lmotr /usr/lib64/libreadline.so.6

SRC = m0hsm_api.o m0hsm.o

all: m0hsm

m0hsm: $(SRC)
	gcc $(LDFLAGS) $(SRC) -o m0hsm

clean:
	rm -f $(SRC) m0hsm m0trace*
