CC = gcc
CFLAGS = -D_REENTRANT -g -Wall -Wno-unknown-pragmas
LDFLAGS = -lpthread -pthread

##################### Compilation Targets #####################

web_server: server.c server.h util.o
	${CC} $(CFLAGS) -o web_server server.c util.o ${LDFLAGS}

###############################################################

##################### Testing Targets #########################
test_prelim:
	@# Port: passed_in | Path: ./testing | num_dispatcher: 1 | num_workers: 1
	@# queue_length: 50 | cache_size: 50

	@read -p "Enter a random port between 1024 & 65536: " sock_in; \
	./web_server $$sock_in ./testing 1 1 50 50

test:
	@# Port: passed_in | Path: ./testing | num_dispatcher: 5 | num_workers: 5
	@# queue_length: 50 | cache_size: 50

	@read -p "Enter a random port between 1024 & 65536: " sock_in; \
	./web_server $$sock_in ./testing 4 4 50 50

test_full:
	@# Port: passed_in | Path: ./testing | num_dispatcher: 5 | num_workers: 5
	@# queue_length: 50 | cache_size: 50

	@read -p "Enter a random port between 1024 & 65536: " sock_in; \
	./web_server $$sock_in ./testing 100 100 50 50

####################################################################

##################### Solution Running Targets #####################

prelim_solution:
	@# Port: passed_in | Path: ./testing | num_dispatcher: 1 | num_workers: 1
	@# queue_length: 50 | cache_size: 50
	@chmod +x server_prelim_sol
	@read -p "Enter a random port between 1024 & 65536: " sock_in; \
	./server_prelim_sol $$sock_in ./testing 1 1 50 50

solution:
	@# Port: passed_in | Path: ./testing | num_dispatcher: 5 | num_workers: 5
	@# queue_length: 50 | cache_size: 50
	@chmod +x web_server_sol
	@read -p "Enter a random port between 1024 & 65536: " sock_in; \
	./web_server_sol $$sock_in ./testing 100 100 50 50

####################################################################

##################### Utility Targets #####################

force_kill:
	@echo -n "Force Killing web_server proc PID: "; ps -C "web_server" -o pid=
	@ps -C "web_server" -o pid= | xargs kill -9
	@echo -n "Force Killing parent proc PID: "; ps -C "make" -o pid=
	@ps -C "make" -o pid= | xargs kill -9

submission: clean
	@echo "Creating Tar Submission..."
	@read -p "Please enter your group number: " grou_num; \
	tar_name="group_"$$grou_num"_p3_submission.tar.gz";\
	rm -f $$tar_name;tar -czvf $$tar_name server.c README.md server.h util.h util.o Makefile;\
	echo "Submission Created: " $$tar_name

clean:
	rm -f web_server webserver_log *.tar.gz
	@clear
	@echo "Succesful Clean"

####################################################################
