#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "account.h"

// global variables that are utilized by multiple functions in program
account *acts; // global pointer to structs that are accessed and mutated throughout program
int size; // global size for number of accounts
int num_transactions; // global variable to keep track of all the number of transactions
int calls = 0;

int count_token (char* buf, const char* delim){ // function from lab1
    char *s = strdup(buf);
    char *token;
    char *rest = s;

    int count = 0;
    while ((token = strtok_r(rest, delim, &rest)))
        count++;
    
    free(s);
    return count + 1;
}

command_line str_filler (char* buf, const char* delim){ // function from lab1
    command_line cmd;

    char *buf2 = strtok(buf, "\n");
    int count = count_token(buf2, delim);

    cmd.num_token = count;

    char *s = strdup(buf2);
    char *token;
    char *rest = s;

    cmd.command_list = malloc(sizeof(char*) * count);

    int len;
    int c = 0;
   
    
    while ((token = strtok_r(rest, delim, &rest))){
    	if (token != NULL){
            len = strlen(token) + 1;           
            cmd.command_list[c] = malloc(sizeof(char) * len);
            strcpy(cmd.command_list[c],token);
    	}
        c++;
    }
    cmd.command_list[c] = NULL;
    free(s);
    return cmd;
}

void free_command_line(command_line *command){ // function from lab1
    for (int i = 0; i < command->num_token; i++)
        free(command->command_list[i]);
    free(command->command_list);
}

int accounts(char *filename){ // this function will take in the input file and determine the number of accounts in the
							  // file based on the first line of the file and return it on function call
	FILE *file;
	size_t len = 128;
	char *line = malloc(len);
	file = fopen(filename, "r");

	int accounts;
	int count = 0;
	
	while(getline(&line, &len, file) != -1){
		if (count == 0)
			accounts = atoi(line);
		count++;
	}
	free(line);
	fclose(file);
	return accounts;
}

void account_info(char *filename){ // this function will fill the account structs with the appropriate info given by the input file
	size = accounts(filename); // utilize above function to determine number of accounts
	
	acts = malloc(sizeof(account) * size); // allocate number of structs based on the number of total accounts
	
	FILE *file;
	size_t len = 128;
	char *line = malloc(len);
	file = fopen(filename, "r");
	
	int num = 0;
	int count = 0;
	int account_line;
	int ret;
	num_transactions = 0;
	
	while(getline(&line, &len, file) != -1){ // read through the file given a line buffer and use getline()
		if (count < 51){ // since instructions indicate that the input file has same format as test file, we only care about first 
						 // 50 lines to parse out the account info
			if ( (count - 1) % 5 == 0){ // every 5 lines of first 50 lines have info about a corresponding account
				num++; // we update this num variable to keep track of which account we are currently dealing with and filling in info for
				account_line = 0;
			}
			account_line++; // this variable will handle the line parsing for necessary info
			
			if (num > 0){
				char *buf = strtok(line, "\n"); 
				if (account_line == 2){ // every second line consistenly holds the account number
					strcpy(acts[num-1].account_number, buf);
					acts[num-1].account_number[17] = '\0';
				}
				if (account_line == 3){ // every third line consistently holds the password
					strcpy(acts[num-1].password, buf);
					acts[num-1].password[9] = '\0';
				}
				if (account_line == 4) // every fourth line consistently holds the starting balance
					acts[num-1].balance = strtod(buf, NULL);
				if (account_line == 5) // every fifth line consistently holds the reward rate
					acts[num-1].reward_rate = strtod(buf, NULL);
				acts[num-1].transaction_tracter = 0.0; // initialize the transaction tracker for each account
				ret = pthread_mutex_init(&acts[num-1].ac_lock, NULL); // initialize the lock for each account
				
			}
		}
		else
			num_transactions++; // variable updates for each valid transaction line
		count++;
	}
	free(line);
	fclose(file);
}

void* process_transaction(void* arg){ // start routine for worker threads that takes in a string of arrays and accordingly handles them
	char **lines = (char **)(arg); // initialize argument parameter given by worker threads

	command_line small_token_buffer;
 	for(int x = 0; x < num_transactions/10; x++){ // loop through the segment of total transaction divided by the number of workers (10)
 		small_token_buffer = str_filler (lines[x], " ");
		for (int j = 0; small_token_buffer.command_list[j] != NULL; j++){
			if (strcmp (small_token_buffer.command_list[j], "T") == 0){ // processing the "Transfer" transaction
				int valid = 0; // variable to determine a valid transaction between two accounts
				for(int x = 0; x < size; x++){ // loop through the accounts
					if(strcmp(small_token_buffer.command_list[j+2], acts[x].password) == 0){ // if account number of transaction matches an account and password, transaction is valid
						valid = 1;
						
					}
				}
				if(valid){
					for(int i = 0; i < size; i++){ 
						if( (strcmp(small_token_buffer.command_list[j+1], acts[i].account_number) == 0) ){ // look for corresponding account once again
							pthread_mutex_lock(&acts[i].ac_lock); // lock to ensure no two threads mutate same account
							acts[i].transaction_tracter = acts[i].transaction_tracter + strtod(small_token_buffer.command_list[j+4], NULL); // add to account's transaction tracker with amount in transaction line
							acts[i].balance = acts[i].balance - strtod(small_token_buffer.command_list[j+4], NULL);	// subtract amount from transfer account
							pthread_mutex_unlock(&acts[i].ac_lock); // unlock the account lock
						}
						if(strcmp(small_token_buffer.command_list[j+3], acts[i].account_number) == 0){ // dealing with account which is receving the amount
							pthread_mutex_lock(&acts[i].ac_lock);
							acts[i].balance = acts[i].balance + strtod(small_token_buffer.command_list[j+4], NULL);	// add ammount to receiving account
							pthread_mutex_unlock(&acts[i].ac_lock);
						}
					}
				}
			}						
			if (strcmp (small_token_buffer.command_list[j], "C") == 0) { // note that since "Check Balance" transaction is trivial, if you wish
																		 // to see some functionality here, simply uncomment code below
				/* uncomment code if you want to display current balance of account handling a "Check Balance" transaction
				for(int i = 0; i < size; i++){
					if( (strcmp(small_token_buffer.command_list[j+1], acts[i].account_number) == 0) && (strcmp( small_token_buffer.command_list[j+2], acts[i].password) == 0) ){
						pthread_mutex_lock(&acts[i].ac_lock);
						printf("Current Balance of Account %s is: %f\n", acts[i].account_number, acts[i].balance);
						pthread_mutex_unlock(&acts[i].ac_lock);
					}
				}
				*/
			}
			if (strcmp (small_token_buffer.command_list[j], "D") == 0){ // dealing with "Deposit" transactions
				for(int i = 0; i < size; i++){ // loop through accounts
					if( (strcmp(small_token_buffer.command_list[j+1], acts[i].account_number) == 0) && (strcmp( small_token_buffer.command_list[j+2], acts[i].password) == 0) ){
						pthread_mutex_lock(&acts[i].ac_lock); // once account number matches, we lock with account lock
						acts[i].transaction_tracter = acts[i].transaction_tracter + strtod(small_token_buffer.command_list[j+3], NULL); // add amount to account's transaction tracker 
						acts[i].balance = acts[i].balance + strtod(small_token_buffer.command_list[j+3], NULL); // add stated amount from found account
						pthread_mutex_unlock(&acts[i].ac_lock); // unlock
					}
				}
			}
			if (strcmp (small_token_buffer.command_list[j], "W") == 0){ // deal with "Withdraw" transaction
				for(int i = 0; i < size; i++){ // loop through accounts
					if( (strcmp(small_token_buffer.command_list[j+1], acts[i].account_number) == 0) && (strcmp(small_token_buffer.command_list[j+2], acts[i].password) == 0) ){
						pthread_mutex_lock(&acts[i].ac_lock); // lock when account and password match
						acts[i].transaction_tracter = acts[i].transaction_tracter + strtod(small_token_buffer.command_list[j+3], NULL); // add amount to transaction tracker of account
						acts[i].balance = acts[i].balance - strtod(small_token_buffer.command_list[j+3], NULL);	// withdraw given amount from account
						pthread_mutex_unlock(&acts[i].ac_lock); // unlock
					}
				}
			}	
		}
		free_command_line(&small_token_buffer);
		memset (&small_token_buffer, 0, 0);
	}	
}

void* update_balance(void *arg){ // start routine for bank thread that simply updates balance with the reward rate for each account once all worker threads are done
	for(int i = 0; i < size; i++){
		pthread_mutex_lock(&acts[i].ac_lock); 
		acts[i].balance = acts[i].balance + (acts[i].transaction_tracter * acts[i].reward_rate); // simply perform operation of updating balace with reward rate
		pthread_mutex_unlock(&acts[i].ac_lock);
	}
	calls++;
	return (void*)(&calls); // return amount of times that this routine is called
}

typedef struct{ // struct for worker threads
	char **work_lines;
}work;

int main(int argc, char **argv){
	if (argc != 2){
		printf ("Missing file\n");
		exit(0);
	}
	
	account_info(argv[1]); // fill in the accounts with corresponding info
	
	work *w = malloc(sizeof(work) * 10); // initialize and dynamically allocate memory for the (10) worker threads which each holds an array of strings
	for(int i = 0; i < 10; i++){
		w[i].work_lines = malloc(sizeof(char *) * (num_transactions/10));
		for(int x = 0; x < num_transactions/10; x++)
			w[i].work_lines[x] = malloc(sizeof(char) * 128);
	}

	FILE *inFPtr;
	inFPtr = fopen (argv[1], "r");
	size_t len = 128;
	char* line_buf = malloc (len);

	/*
	for(int i = 0; i < size; i++)
		printf("%d Account Number: %s Password: %s Balance: %f Reward Rate: %f Transaction Tracker: %f\n", 
		i, acts[i].account_number, acts[i].password, acts[i].balance, acts[i].reward_rate, acts[i].transaction_tracter);
	 */
	
	command_line large_token_buffer;

	int line_num = 0;
	int c = 0;
	int index = 0;
	int t = 0;
	while ( (getline (&line_buf, &len, inFPtr) != -1) ){ // getline to read through the file with line buffer
		if (line_num > 50){ // same logic with account info, but now with transaction lines in file
			if(c % (num_transactions/10) == 0){ // once we reach first chunk of the total number of transactions divided by 10, we move to filling in the next worker struct
				index++;
				t = 0;
			}
			else
				t++;
			
			large_token_buffer = str_filler (line_buf, "\n");
			for (int i = 0; large_token_buffer.command_list[i] != NULL; i++) // loop through the line from file and copy buffer into the indexed struct's double pointer of char
				strcpy(w[index-1].work_lines[t], large_token_buffer.command_list[i]);

			c++; // update number of current file line
	
			free_command_line (&large_token_buffer);
			memset (&large_token_buffer, 0, 0);
		}	
		line_num++;
	}


	pthread_t workers[10]; // initalize the (10) worker threads of type pthread_t
	for(int i = 0; i < 10; i++) // loop through (10) worker threads and create with given start routine and argument which is corresponding work struct of array of strings
		pthread_create(&workers[i], NULL, process_transaction, (void *)w[i].work_lines);
	for(int i = 0; i < 10; i++) // wait for each worker thread to finihs with pthread_join function call
		pthread_join(workers[i], NULL);
	
	int *r;
	pthread_t bank_thread; // initalize the sole bank thread that will update each account with reward rate and transaction tracker
	pthread_create(&bank_thread, NULL, update_balance, NULL); // create bank thread with given start routine and no corresponding arguments
	pthread_join(bank_thread, (void*)(&r)); // wait for bank thread to finish its work
	
	
	FILE *fp; // this block of code handles the output.txt file that is expected with correct formatting
	fp = freopen("output.txt", "w", stdout);
	for(int t = 0; t < size; t++)
		printf("%d balance:\t%.2f\n\n", t, acts[t].balance);
	
	// appropriate free calls are made in this remaining block of code
	for(int i = 0; i < 10; i++){ 
		for(int x = 0; x < num_transactions/10; x++)
			free(w[i].work_lines[x]);
		free(w[i].work_lines);
	}
	free(w);
	free(acts);
	free (line_buf);
	fclose(inFPtr);
	
	return 0;
}