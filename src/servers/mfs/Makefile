# Makefile for File System (FS)
SERVER = mfs
DEST=/sbin/$(SERVER)
NR_BUFS=1024
BS=4096

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i $(EXTRA_OPTS) $(CPROFILE) -DNR_BUFS=$(NR_BUFS)
LDFLAGS = -i
LIBS =  -lsys -ltimers

OBJ =	cache.o device.o link.o \
	mount.o misc.o open.o pipe.o protect.o read.o \
	stadir.o table.o time.o utility.o \
	write.o inode.o main.o path.o super.o

# build local binary 
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S `expr $(NR_BUFS) \* $(BS) \* 2.2` $(SERVER)

install: $(SERVER)
	-mv $(DEST) $(DEST).prev
	install $(SERVER) $(DEST)

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend


