CC 	= gcc

INCPATH = include

CFLAGS  = -Wall -pedantic -g -I $(INCPATH) -fpic

OBJS	= $(OBJ32) $(OBJ64)

OBJ32   = obj/pp32.o obj/pb32.o

OBJ64   = obj/pp64.o obj/pb64.o

EXTRACLEAN = tryme32 tryme64 $(LIBS)

DIRS = obj lib lib64 static/lib static/lib64 static 

LIBS = 	lib/libpp.so lib64/libpp.so 	\
	static/lib/libpp.a static/lib64/libpp.a

all: $(DIRS)  $(LIBS)

lib:
	mkdir lib

lib64:
	mkdir lib64

static/lib:
	mkdir -p static/lib

static/lib64: 
	mkdir -p static/lib64

obj:
	mkdir obj

allclean: clean
	rm -f $(EXTRACLEAN)
	-rmdir $(DIRS)

static/lib/libpp.a: $(OBJ32)
	ar r static/lib/libpp.a  $(OBJ32)

static/lib64/libpp.a: $(OBJ64)
	ar r static/lib64/libpp.a $(OBJ64)

lib/libpp.so: $(OBJ32)
	$(CC) $(CFLAGS) -m32 -shared -o $@ $(OBJ32)

lib64/libpp.so: $(OBJ64)
	$(CC) $(CFLAGS) -m64 -shared -o $@ $(OBJ64)

clean:	
	rm -f $(OBJS) *~ TAGS

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS)

depend:
	@echo Regenerating local dependencies.
	@makedepend $(SRCS) $(HDRS)

tags : $(SRCS) $(HDRS)
	etags $(SRCS) $(HDRS)  


obj/%32.o: src/%.c
	$(CC) -m32 -c $(CFLAGS) -o $@ $< 

obj/%64.o: src/%.c
	$(CC) -m64 -c $(CFLAGS) -o $@ $< 


tryme32: testmain.c lib/libpp.a
	$(CC) -o tryme32 -m32 testmain.c $(CFLAGS) -L $(LIBPATH)/lib -lpp

tryme64: testmain.c lib64/libpp.a
	$(CC) -o tryme64 -m64 testmain.c $(CFLAGS) -L $(LIBPATH)/lib64 -lpp
test:	tryme32 tryme64
	echo "32-bit version"
	./tryme32
	echo "64-bit version"
	./tryme64

PUB = Notes Makefile include lib lib64 static src
EXCLUDES=--exclude '*~' --exclude Test --exclude PLNNotes --exclude Test --exclude RCS
pub:
	chmod a+rX .
	chmod -R a+rX $(PUB)
	rsync -rvltpz . pn-cs453@unix6.csc.calpoly.edu:Given/Asgn1 $(EXCLUDES)
	rsync -rvltpz . pn-cs453@localhost:Given/Asgn1 $(EXCLUDES)

	#scp -pr $(PUB) pn-cs453@unix6.csc.calpoly.edu:Given/Asgn1
	#scp -pr $(PUB) pn-cs453@localhost:Given/Asgn1


