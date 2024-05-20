Virtual memory manager simulator.
Using linux functions forks, semaphores, and locked memory to simulate shared memory across multiple processes.

Simulate 6 different page replacement algorithms : LIFO, MRU, LRU, LFU, OPT-X, and WS.
So there will be 6 different outputs.

Can run processes concurrently or can take turns respective to the input.

Process will halt if there in no more space in main memory segment.
Process will resume once previous process completes and terminates.


Expected input type:
tp /* total_number_of_page_frames (in main memory) */
ps /* page size (in number of bytes) */
r /* number_of_page_frames_per_process for LIFO, MRU, LRU-K,
LFU and OPT,
or delta (window size) for the Working Set algorithm */
X /* lookahead window size for OPT, X for LRU-X, 0 for others (which do
not use this value) */
min /* min free pool size */
max /* max free pool size */
k /* total number of processes */
pid1 size1 /* process_id followed by total number of page frames on disk */
pid2 size2
: :
: :
pidk sizek
These parameters are followed by a list of process id and address pairs: pid addr.

Address is broken down into address and offset.
For example: 0x0F would be 00001111. Adress would be 0000 and Offset would be 1111.



To compile program:
g++ -o main main.cpp -lpthread

To run program:
./main

replace input.txt with whatever input file.

Output will be in output.txt
