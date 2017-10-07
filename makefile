CC = gcc
CFLAGS = -Wall -pedantic 

SRCS = server.c
HEADERS = 
OBJS = server.o

client: ${OBJS} ${HEADERS}
	${CC} ${SRCS} -o prog

${OBJS}: ${SRCS}
	${CC} ${CFLAGS} -c $(@:.o=.c)

debug: ${OBJS}
	${CC} ${CFLAGS} -g ${SRCS} -o debug

clean: 
	rm *.o prog debug *~
