all: lets-talk

lets-talk:
	gcc -g -o lets-talk lets-talk.c -pthread

clean:
	rm -f lets-talk