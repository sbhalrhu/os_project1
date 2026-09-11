all: schedule 

schedule: schedule.c
	gcc -Wall -Werror -g -o schedule schedule.c

clean:
	rm -rf schedule


