# CS_342_Operating_Systems_Project
Contains the projects that we have done for the Bilkent CS 342 Operating Systems course.
- `Project 1` : Processes, IPC, and Threads
   - `Description`:  In this project we were asked to develop an application that will find the K most frequently occurring words, i.e., top-K words, in a given input
data set. In part A, the application used multiple child processes to process the data set. In part B it used multiple threads.
   -***How to run:***   
   For part A, the program will be run from the command line in the following way: 
   1. The user will first type make. 
   2. The user will type  ./proctopk <K> <outfile> <N> <infile1> .... <infileN> where k is the frequency number, outfile is the name of the output file that the program will write to, n being the number of input files and infiles being the name of the input files. The process is the same for part B except this time the program name is threatopk.
