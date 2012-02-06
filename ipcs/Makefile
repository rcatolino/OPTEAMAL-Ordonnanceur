CC=gcc #Compiler
EDL=gcc #Linker
AR=ar #Archiver
CCFLAGS=-Wall -m32 #Compiler options
EDLFLAGS=-Wall -m32 #Linker options
ARFLAGS=rcs
EXE=test #Binary name
LIB=libgthread.a #Static library name
ECHO=@echo

EOBJ=test.o 
LOBJ=gthread.o hw.o ipcs.o

$(EXE): $(EOBJ) $(LIB)
	@echo building $<
	$(EDL) -o $(EXE) $(EDLFLAGS) $(EOBJ) $(LIB) -lrt
	@echo done

$(LIB): $(LOBJ)
	@echo building $<
	$(AR) $(ARFLAGS) $(LIB) $(LOBJ)
	@echo done

%.o : %.c *.h
	@echo building $< ...
	$(CC) $(CCFLAGS) -c $<
	@echo done
	
clean: 
	@echo -n cleaning repository... 
	@rm -rf *.o
	@rm -rf *.a
	@rm -rf .*.swp
	@echo cleaned.
