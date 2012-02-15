CC=gcc #Compiler
EDL=gcc #Linker
AR=ar #Archiver
CCFLAGS=-Wall -g -m32 #Compiler options
EDLFLAGS=-Wall -g -m32 #Linker options
ARFLAGS=rcs
EXE=test #Binary name
LIB=libgthread.so#Static library name
ECHO=@echo

EOBJ=test.o tcpserver.o
LOBJ=gthread.o hw.o ipcs.o

lib : $(LIB)

install : $(LIB)
	@cp ./libgthread.so.1 /usr/lib32/
	@cp gthread.h /usr/include/
	@cp ipcs.h /usr/include/gipcs.h
	@echo Updating ld...
	ldconfig

$(EXE): $(EOBJ)
	@echo building $<
	$(EDL) -o $(EXE) $(EDLFLAGS) $(EOBJ) -lgthread -lgmem -lrt
	@echo done

$(LIB): $(LOBJ)
	@echo building $<
	$(CC) -m32 -shared -Wl,-soname,$(LIB) -o $(LIB).1 $(LOBJ)
	@#$(AR) $(ARFLAGS) $(LIB) $(LOBJ) -lgmem
	@echo done

%.o : %.c *.h
	@echo building $< ...
	$(CC) $(CCFLAGS) -c $<
	@echo done
	
clean: 
	@echo -n cleaning repository... 
	@rm -rf *.o
	@rm -rf *.a
	@rm -rf *.so*
	@rm -rf .*.swp
	@echo cleaned.
