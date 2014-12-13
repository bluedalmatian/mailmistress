CC = g++
COMPILERFLAGS = -I. -I /boot/develop/headers/be
LINKERFLAGS = -L. -L /boot/develop/lib/x86 -lbe -lmail


# $@ refers to the make command, $^ refers to all the dependencies for that command
#$< refers to the first dependency for that command


#Main App skeleton below

dist: MailMistress.rsrc MailMistress
	@echo --- $@ ---
	#The full distribution ready app
	xres -o MailMistress MailMistress.rsrc
	

MailMistress:  IncomingMail.o MailingList.o MailMistressApplication.o MailWorker.o 
	@echo --- $@ ---
	$(CC) $(COMPILERFLAGS) $(LINKERFLAGS)  ./main.cpp $^ -o $@

test:
	@echo --- $@ ---
	$(CC) $(COMPILERFLAGS) $(LINKERFLAGS)  ./test.cpp $^ -o $@
#End of Main App skeleton

#Supporting classes & files

IncomingMail.o: ./IncomingMail.cpp
	@echo --- $@ ---
	$(CC) $(COMPILERFLAGS) $< -o $@ -c 

MailingList.o: ./MailingList.cpp
	@echo --- $@ ---
	$(CC) $(COMPILERFLAGS) $< -o $@ -c 

MailMistressApplication.o: ./MailMistressApplication.cpp
	@echo --- $@ ---
	$(CC) $(COMPILERFLAGS) $< -o $@ -c 
	

MailWorker.o: ./MailWorker.cpp
	@echo --- $@ ---
	$(CC) $(COMPILERFLAGS) $< -o $@ -c 

MailMistress.rsrc: ./MailMistress.rdef
	@echo --- $@ ---
	rc -o $@ $<
	

clean:
	@echo --- $@ ---
	rm -f ./MailMistress
	rm -f ./*.o
	rm -f ./test
