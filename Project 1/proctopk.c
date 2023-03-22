#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdint.h>

#define MAX_WORD_SIZE 64
#define TABLE_SIZE 100000
#define MIN_PROCESSES 1
#define MAX_PROCESSES 10
#define MIN_K 1
#define MAX_K 10
#define SHARED_MEM_NAME "/topKFrequencySharedMemory"

typedef struct Node {//array list
    char key[MAX_WORD_SIZE];
    int occurrence;
    struct Node *next;
} Node;

void increment_frequency(Node **table, char *key, int num) {
    Node *curr = *table;
    Node *prev = *table;
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {//if the word is not unique
            curr->occurrence = curr->occurrence + num;//increment its occurrence
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    // if the word is unique
    //insert new node to the end
    Node *new_node = malloc(sizeof(Node));//new node
    strcpy(new_node->key, key);
    new_node->occurrence = num;//ocurrance is 1
    new_node->next = NULL;
    if(*table!=NULL) {
        prev->next = new_node;
    } else {
        *table = new_node;
    }
}

//helper function for the qsort()
int descending_helper(const void *x, const void *y) {
    Node *node1 = (Node *) x;
    Node *node2 = (Node *) y;
    if(node2->occurrence - node1->occurrence != 0){
        return (node2->occurrence - node1->occurrence);//descending order
    } else {
        return strcmp(node1->key, node2->key);
    }
}

int processFile(char* fileName, int childNo, int topK, Node* sharedFrequencyTable){
    FILE *file = fopen(fileName, "r");
    if (!file) {
        perror("file could not be opened \n");
        exit(EXIT_FAILURE);
    }

    Node* wordCountList = NULL;

    char single_word[MAX_WORD_SIZE];
    while (fscanf(file, "%63s", single_word) == 1) {//reads a word (not equal to end of file)
        for (int x = 0; single_word[x] != '\0'; x++) {//while processing every char in the word
            single_word[x] = toupper(single_word[x]);//convert the word to upper case
        }
        increment_frequency(&wordCountList, single_word, 1);
    }
    fclose(file); //close the input file

    //Calculate size of the word frequency list
    Node* curr = wordCountList;
    int size=0;
    while(curr!=NULL){
        curr = curr->next;
        size++;
    }
    if(size<topK){
        topK=size;
    }

    //Convert word frequency linked list to array in order to use qsort function
    Node frequencyArray[size];
    curr = wordCountList;
    for(int z=0; z<size; z++){
        frequencyArray[z] = *curr;
        curr = curr->next;
    }

    qsort((void*)frequencyArray, size, sizeof(Node), descending_helper);

    int indexOffset = childNo*topK;
    
    //Copying top k frequency data to sharedMemeory
    for(int i=0; i<topK; i++){
        Node* offset = sharedFrequencyTable + childNo*topK +i;
        memcpy(offset, frequencyArray + i, sizeof(Node));
    }

    //Free linked list and array
    curr = wordCountList;
    while(curr!=NULL){
        Node * temp = curr->next;
        free(curr);
        curr = temp;
    }
    return 0;
}
// start timer
//struct timeval start, end, delta;
//gettimeofday(&start, NULL);
int main(int argc, char *argv[]) {
    int k, numOfFiles;
    char *outputfile;

    k = atoi(argv[1]);//assign the second argument to k
    if (k < MIN_K||k > MAX_K) {
        printf("Number of input files should be between 1-10 ! \n");
        exit(1);//exit the program
    }

    numOfFiles = atoi(argv[3]);//assign the third argument to numOfFiles

    if (argc != numOfFiles + 4) { //if number of command line argumens is smaller than
        //number of files + 4
        printf("Wrong number of arguments ! \n");
        printf("You should enter the following way: \n");
        printf(" proctopk <K> <outfile> <N> <infile1> .... <infileN>\n");
        exit(1);//exit the program
    }
    if (numOfFiles < MIN_PROCESSES||numOfFiles > MAX_PROCESSES) {
        printf("Number of input files should be between 1-10 ! \n");
        exit(1);//exit the program
    }

    outputfile = argv[2];//assign the second argument to the output file name
    char **inputfiles = malloc(numOfFiles * sizeof(char *));//array of input file names
    if (inputfiles == NULL) {// if there are no input files, exit
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < numOfFiles; i++) {
        inputfiles[i] = argv[i + 4];
    }

    int tableSize = k*numOfFiles;
    int shmem_size = k*numOfFiles*sizeof(Node);
    int shmem_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0600);
    if(shmem_fd < 0){
        perror("shm_open()");
        return EXIT_FAILURE;
    }
    ftruncate(shmem_fd, shmem_size);
    pid_t childPIDs[numOfFiles];
    //creating N child processes
    for (int i = 0; i < numOfFiles; i++) {
        pid_t process = fork();//use fork
        childPIDs[i] = process;
        if (process == -1) {//if fork  failed
            perror("fork failed! \n");
            return EXIT_FAILURE;
        }
        if (process == 0) {//child process
            int shmem_cfd = shm_open(SHARED_MEM_NAME, O_RDWR, 0600);
            if(shmem_cfd < 0){
                perror("shm_open error()");
                return EXIT_FAILURE;
            }
            Node* childSharedFrequencyTable = (Node*) mmap(0, tableSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmem_fd, 0);
            processFile(inputfiles[i], i, k, childSharedFrequencyTable);
            munmap(childSharedFrequencyTable, tableSize);
            close(shmem_cfd);
            exit(0);
        }
    }
    int n=numOfFiles, status;
    pid_t pid;
    while (n > 0) {
        pid = wait(&status);
        --n;
    }
    Node* sharedFrequencyTable = (Node*) mmap(0, tableSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmem_fd, 0);

    Node * globalFrequencyList = NULL;
    for(int i=0; i<k*numOfFiles; i++){
        increment_frequency(&globalFrequencyList, sharedFrequencyTable[i].key, sharedFrequencyTable[i].occurrence);
    }
    munmap(sharedFrequencyTable, tableSize);
    close(shmem_fd);
    shm_unlink(SHARED_MEM_NAME);
    Node* curr = globalFrequencyList;
    int size=0;
    while(curr!=NULL){
        curr = curr->next;
        size++;
    }
   
    Node globalFrequencyArray[size];
    curr = globalFrequencyList;
    for(int z=0; z<size; z++){
        globalFrequencyArray[z] = *curr;
        curr = curr->next;
    }

    qsort((void*)globalFrequencyArray, size, sizeof(Node), descending_helper);

    FILE* outputFptr = fopen(outputfile, "w");
    curr = globalFrequencyList;
    if(size<k){
        for(int i=0; i<size-1; i++){
        fprintf(outputFptr, "%s %d\n", globalFrequencyArray[i].key, globalFrequencyArray[i].occurrence);
       
        }
    }
    else{
        for(int i=0; i<k; i++){
            fprintf(outputFptr, "%s %d\n", globalFrequencyArray[i].key, globalFrequencyArray[i].occurrence);
           
        }
    }
    
    fclose(outputFptr);

    free(inputfiles);
    curr = globalFrequencyList;
    while(curr!=NULL){
        Node * temp = curr->next;
        free(curr);
        curr = temp;
    }
}
// print time
//gettimeofday(&end, NULL);
//timersub(&end, &start, &delta);
//uint64_t msecs = (delta.tv_sec * (uint64_t)1000) + (delta.tv_usec/1000);//
//printf("Delta of the program: %ld.%06ld milliseconds\n", msecs);
