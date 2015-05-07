

JSON=third-party/cJSONFiles/cJSON

LDFLAGS=-lapr-1 -lm

CFLAGS+=-Wall -g
CFLAGS+=-Ithird-party/libstomp-master/src
CFLAGS+=-I${JSON}/

traintrack: libstomp.a src/train-track.o ${JSON}/cJSON.o
	${CC} ${LDFLAGS} -o $@ src/train-track.o ${JSON}/cJSON.o libstomp.a



libstomp.a: build/stomp.o
	ar -r $@ $^
	ranlib $@

build/stomp.o: third-party/libstomp-master/src/stomp.c
	$(CC) $(CFLAGS) -c $^ -o $@

	