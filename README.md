# Multi-Thread-Bank
This project was completed during my Operating System course. 

Application takes in an input file that contains the number of accounts and account information, such as a password, starting balance, and account number, and the rest of the input file contained a single line transaction of either a withdrawal, transfer, deposit, or checking the balance. The application ran through each line to verify the account(s) and proceed with the transaction for that account for the stated amount. 

The added element for this application was the use of multiple threads using the Pthread API and having 10 working threads work through 12000 transactions each and outputting the final balance of each account to an output text file. 

All of the code was written in C and intended to run in Linux OS.
