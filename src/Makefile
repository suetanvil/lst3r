CC = gcc
CFLAGS=-Wall -g -std=c99

LD = gcc
LDFLAGS =
LIBS = -lm

VM = lst3
BOOT = buildImage
IMAGE = systemImage

COMMON_SRC = memory.c names.c news.c interp.c primitive.c filein.c lex.c \
				parser.c unixio.c tty.c 
BOOT_SRC = initial.c $(COMMON_SRC)
VM_SRC = st.c $(COMMON_SRC)

BOOT_OBJ = $(BOOT_SRC:.c=.o)
VM_OBJ = $(VM_SRC:.c=.o)



all: $(VM) $(BOOT) $(IMAGE)
	mv $(VM) ..

$(VM): $(VM_OBJ)
	$(LD) $(LDFLAGS) -o $(VM) $^ $(LIBS)

$(BOOT): $(BOOT_OBJ)
	$(LD) $(LDFLAGS) -o $(BOOT) $^ $(LIBS)

$(IMAGE): $(BOOT)
	cd ../bootstrap && \
		../src/$(BOOT) basic.st mag.st collect.st file.st mult.st tty.st && \
		mv systemImage ..

test: all
	(bash run_tests.sh)

.c.o:	
	$(CC) -c $(CFLAGS) $<

tags:
	etags *.[ch]

clean:
	rm -f *.o TAGS deps.mk $(VM) $(BOOT) ../$(IMAGE) ../bootstrap/object-map.txt

# Actual dependencies: these are generated with 'gcc -MM' and
# included.  If your compiler doesn't support this, just comment it
# out.
deps.mk:
	gcc -MM $(VM_SRC) initial.c > deps.mk

include deps.mk


