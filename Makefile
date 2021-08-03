all: bank

%.o: %.c account.h
		gcc -c -o $@ $< 

bank: bank.o
		gcc -o bank bank.o -lpthread