FLAGS = -Wall -pthread -g

all: broker producer consumer

clean:
	rm -f broker producer consumer

broker: src/broker.c
	gcc -o broker src/broker.c $(FLAGS)

producer: src/producer.c
	gcc -o producer src/producer.c $(FLAGS)

consumer: src/consumer.c
	gcc -o consumer src/consumer.c $(FLAGS)
