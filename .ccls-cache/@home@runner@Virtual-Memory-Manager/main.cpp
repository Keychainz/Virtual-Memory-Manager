#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <fcntl.h>


using namespace std;

struct Process
{
	int pid; /* Process ID */
  int order; /* Order of processes just to help with semaphore allocation */
	int size; /* Total number of page frames on disk */
	vector<sem_t*> semaphores; // Semaphores to see whos turn it is
  vector<sem_t*> deadSemaphores; // Semaphores to tell process is dead
};
Process process;

struct AddressTable
{
	int pid;
	int address;
	int offset;
};

struct VirtualMemoryManager 
{
	int tp; /* total_number_of_page_frames (in main memory) */
	int ps; /* page size (in number of bytes) */
	int offsetSize;
	int r; /* number_of_page_frames_per_process for LIFO, MRU, LRU-K, LFU and OPT, or delta (window size) for the Working Set algorithm */
	int X; /* lookahead window size for OPT, X for LRU-X, 0 for others (which do not use this value) */
	int minimum; /* min free pool size */
	int maximum; /* max free pool size */
	int k; /* total number of processes */
	vector<Process> processes; /* Process IDs and sizes list */
	vector<AddressTable> address; /* Process IDs and address list */
};
VirtualMemoryManager page;

struct Table
{
  bool isAllocated;
  int processID;
  int pageID;
  int frameID;
  int age;
};

int generateRandom7DigitNumber() 
{
  // Seed the random number generator
  srand(time(nullptr));

  // rand() returns a number between 0 and RAND_MAX
  // Ensure the number is 7 digits long, between 1000000 and 9999999
  return 1000000 + rand() % 9000000;
}

int memKey = generateRandom7DigitNumber(); // Random number for memory segment

int wsMaxSize = 0;

int FindOffset(int x)
{
		int count = 1;
		int temp = 2;
		while(temp != x)
		{
				temp *= 2;
				count++;
		}
		return count;
}

//Converts Hexadecimal to Binary
// https://www.geeksforgeeks.org/program-to-convert-hexadecimal-number-to-binary/
// 4/26/2024 20:42:00
string HexadecimaltoBinary(string hex)
{
	int j=0;
	string binary="";
	
	if(hex[1] == 'x' || hex[1] == 'X')
	{
		j=2;
	}

	for(int i=j; i<hex.length(); i++)
	{
		switch (hex[i])
		{
				case '0':
					binary += "0000";
					break;
				case '1':
					binary += "0001";
					break;
				case '2':
					binary += "0010";
					break;
				case '3':
					binary += "0011";
					break;
				case '4':
					binary += "0100";
					break;
				case '5':
					binary += "0101";
					break;
				case '6':
					binary += "0110";
					break;
				case '7':
					binary += "0111";
					break;
				case '8':
					binary += "1000";
					break;
				case '9':
					binary += "1001";
					break;
				case 'A':
				case 'a':
					binary += "1010";
					break;
				case 'B':
				case 'b':
					binary += "1011";
					break;
				case 'C':
				case 'c':
					binary += "1100";
					break;
				case 'D':
				case 'd':
					binary += "1101";
					break;
				case 'E':
				case 'e':
					binary += "1110";
					break;
				case 'F':
				case 'f':
					binary += "1111";
					break;
				default:
					cout << "\nInvalid hexadecimal digit " << hex[i];
			}
	}
	return binary;
}

// Function to convert binary to decimal
// https://www.geeksforgeeks.org/program-binary-decimal-conversion/
// 4/26/2024 20:44:20
int binaryToDecimal(string n)
{
		string num = n;
		int dec_value = 0;

		// Initializing base value to 1, i.e 2^0
		int base = 1;

		int len = num.length();
		for (int i = len - 1; i >= 0; i--) {
				if (num[i] == '1')
						dec_value += base;
				base = base * 2;
		}

		return dec_value;
}

// Loads input file into variables
void load(string inputFile)
{
	ifstream infile(inputFile);
	if (!infile) 
	{
		cerr << "Error opening file." << endl;
		return;
	}
	
	infile >> page.tp;
	infile >> page.ps;
	page.offsetSize = FindOffset(page.ps);
	infile >> page.r;
	infile >> page.X;
	infile >> page.minimum;
	infile >> page.maximum;
	infile >> page.k;
	for(int i=0; i < page.k; i++)
	{
		Process process;
		infile >> process.pid >> process.size;
    process.order = i;
		page.processes.push_back(process);
	}
	AddressTable addressTable;
	string temp;
	while (infile >> addressTable.pid >> temp) 
	{
		if(temp != "-1")
		{
			temp = HexadecimaltoBinary(temp);
			int splitIndex = temp.length() - page.offsetSize;
			string address = temp.substr(0, splitIndex);
			string offset = temp.substr(splitIndex);

			addressTable.address = binaryToDecimal(address);
			addressTable.offset = binaryToDecimal(offset);
		}
		else
		{
			addressTable.address = stoi(temp);
			addressTable.offset = -1;
		}
		page.address.push_back(addressTable);
	}

	infile.close();
}

// Creaks k forks
void CreateFork()
{
	for(int i=0;i<page.k;i++)
	{
		if(fork() == 0)
		{
			process.pid = page.processes[i].pid;
      process.order = i;
			break;
		}
		else
		{
			process.pid = -1;
		}
	}
	for(int i=0;i<page.k;i++)
	{
		wait(NULL);
	}
}

// Create semaphores to check if process is done with turn, initalized with 1
// Create semaphores to check if process is dead, initalized with 0
void CreateSemaphores()
{
  process.semaphores.resize(page.k);
  process.deadSemaphores.resize(page.k);
  
  for (int i = 0; i < page.k; ++i) 
  {
    string semName = "/" + to_string(page.processes[i].pid);
    // Initialize all semaphores to 1
    process.semaphores[i] = sem_open(semName.c_str(), O_CREAT, 0644, 2);
    if (process.semaphores[i] == SEM_FAILED) 
    {
      cerr << "Failed to create semaphore: " << semName << endl;
      exit(EXIT_FAILURE);
    }

    string semName2 = "*" + to_string(page.processes[i].pid);
    // Initialize all semaphores to 0
    process.deadSemaphores[i] = sem_open(semName2.c_str(), O_CREAT, 0644, 0);
    if (process.deadSemaphores[i] == SEM_FAILED) 
    {
      cerr << "Failed to create semaphore: " << semName << endl;
      exit(EXIT_FAILURE);
    } 
  }
}

void WaitSemaphore(int index, string type) 
{
  if(type == "live") // For semaphore to check if process turn is done
  {
    if(index < 0 || index >= process.semaphores.size()) 
    {
      cerr << "Index out of bounds for semaphores vector!!!" << endl;
      return;
    }
    if(sem_wait(process.semaphores[index]) == -1) 
    {
      cerr << "Failed to wait on semaphore at index " << index << " by Process " << process.pid << "!!!" << endl;
      exit(EXIT_FAILURE);
    }
    cout << "--" << process.pid << " locked semaphore " << index << endl;
  }
  else if(type == "dead") // For semaphore to check if process is dead
  {
    if(index < 0 || index >= process.deadSemaphores.size()) 
    {
      cerr << "Index out of bounds for semaphores vector." << endl;
      return;
    }
    if(sem_wait(process.deadSemaphores[index]) == -1) 
    {
      cerr << "Failed to wait on semaphore at index " << index << " by Process " << process.pid << endl;
      exit(EXIT_FAILURE);
    }
    cout << "___" << process.pid << " locked semaphore " << index << endl;
  }
}

void PostSemaphore(int index, string type) 
{
  if(type == "live") // For semaphore to check if process turn is done
  {
    if(index < 0 || index >= process.semaphores.size()) 
    {
      cerr << "Index out of bounds for semaphores vector!!!" << endl;
      return;
    }
    if(sem_post(process.semaphores[index]) == -1)
    {
      cerr << "Failed to post on semaphore at index " << index << " by process " << process.pid << "!!!" << endl;
      exit(EXIT_FAILURE);
    }
    cout << "--" << process.pid << " released semaphore " << index << endl;
  }
  else if(type == "dead") // For semaphore to check if process is dead
  {
    if(index < 0 || index >= process.deadSemaphores.size()) 
    {
      cerr << "Index out of bounds for semaphores vector." << endl;
      return;
    }
    if(sem_post(process.deadSemaphores[index]) == -1)
    {
      cerr << "Failed to post on semaphore at index " << index << " by process " << process.pid << endl;
      exit(EXIT_FAILURE);
    }
    cout << "___" << process.pid << " released semaphore " << index << endl;
  }
}

// Close semaphores
void CloseSemaphore()
{
  for (int i = 0; i < page.k; ++i) 
  {
    string semName = "/" + to_string(page.processes[i].pid);
    sem_close(process.semaphores[i]);
    sem_unlink(semName.c_str());

    string semName2 = "*" + to_string(page.processes[i].pid);
    sem_close(process.deadSemaphores[i]);
    sem_unlink(semName2.c_str());
  }
}

// Check if free pool space is at size minumum or higher
bool FreePoolSpace(Table* pageFrame, int &placesOnMem)
{
  int empty = 0;
  bool free = false;
  for (int i = 0; i < page.tp; i++)
  {
    if(pageFrame[i].isAllocated == false)
    {
      empty++;
    }
    else if(pageFrame[i].isAllocated == true)
    {
      if(pageFrame[i].processID == process.pid)
      {
        placesOnMem++;
      }
    }
  }
  empty = empty - page.minimum;

  if(empty > 0)
  {
    free = true;
  }
  
  return free;
}

// Check if a page fault has occured
bool PageFault(vector<Table> pageTable, int address)
{
  bool fault = true;
  for(int i = 0; i < page.r; i++)
  {
    if(pageTable[i].isAllocated == true)
    {
      if(pageTable[i].pageID == address)
      {
        fault = false;
      }
    }
  }
  return fault;
}

// Clears the pageframe of all addressed for a process if it has finished
void ClearProcess(Table* pageFrame)
{
  for (int i=0; i < page.tp; i++)
  {
    if(pageFrame[i].processID == process.pid)
    {
      pageFrame[i].isAllocated = false;
      pageFrame[i].processID = -1;
      pageFrame[i].pageID = -1;
      pageFrame[i].frameID = -1;
      pageFrame[i].age = -1;
    }
  }
}

// Checks which process holds the majority of the memory
// If a process needs to be halted, it will wait on the process which holds most of the main memory
int MajorityOnFrame(Table* pageFrame)
{
  unordered_map<int, int> countMap;
  int majority = (page.tp - page.minimum) / 2;
  int maxCount = 0;
  int mostFrequentProcess = pageFrame[0].processID;

  // Count each number
  for(int i=0; i<page.tp; i++)
  {
    countMap[pageFrame[i].processID]++;
    if(countMap[pageFrame[i].processID] > majority) 
    {
      if(pageFrame[i].processID != process.pid)
      {
        maxCount = countMap[pageFrame[i].processID];
        mostFrequentProcess = pageFrame[i].processID;
        break;
      }
    }
  }
  for(int i=0; i<page.k; i++)
  {
    if(page.processes[i].pid == mostFrequentProcess)
    {
      mostFrequentProcess = page.processes[i].order;
      break;
    }
  }
  return mostFrequentProcess;
}

// Replaces the last one in
// pageTable should be in order for earliest to latest so will have to replace from the end.
void LIFOreplace(vector<Table> &pageTable, int address, Table* pageFrame)
{
  // Adds to empty frame
  for(int i=0; i<page.r; i++)
  {
    if(pageTable[i].isAllocated == false)
    {
      pageTable[i].pageID = address;
      pageTable[i].isAllocated = true;
      for(int j=0; j<page.tp; j++)
      {
        if(pageFrame[j].isAllocated == false)
        {
          pageFrame[j].isAllocated = true;
          pageFrame[j].processID = process.pid;
          pageFrame[j].pageID = i;
          pageTable[i].frameID = j;
          break;
        }
      }
      return;
    }
  }
  // Replaces frame
  pageTable[page.r-1].pageID = address;
}

// Helper function which ensures most recently used is at the end of the pagetable
void MRUhelper(vector<Table> &pageTable, int address, Table* pageFrame)
{
  int recentlyUsedIndex = -1;
  int lastIndex = -1;
  for(int i=0; i<page.r; i++)
  {
    if(pageTable[i].pageID == address)
    {
      recentlyUsedIndex = i;
    }
    if(pageTable[i].isAllocated == true)
    {
      lastIndex = i;
    }
  }
  if(recentlyUsedIndex != lastIndex)
  {
    swap(pageTable[recentlyUsedIndex], pageTable[lastIndex]);
  }
  pageFrame[pageTable[lastIndex].frameID].pageID = lastIndex;
}

void LRU(vector<Table> &pageTable, int address, Table* pageFrame, int index)
{
  vector<int> LRUtracker(page.r);
  for(int i=0; i<page.r; i++)
  {
    LRUtracker[i] = 0;
  }
  
  for(int i=0; i<page.r; i++)
  {
    for(int j=index; j>=0; j--)
    {
      LRUtracker[i]++;
      if(page.address[j].address == pageTable[i].pageID)
      {
        break;
      }
    }
  }
  int maxIndex = 0;
  for(int i=0; i<LRUtracker.size(); i++)
  {
    if(LRUtracker[i] > LRUtracker[maxIndex])
    {
      maxIndex = i;
    }
  }

  pageTable[maxIndex].pageID = address;
}

void LFUreplace(vector<Table> &pageTable, int address, Table* pageFrame)
{
  // Add to frame if empty space available
  for(int i=0; i<page.r; i++)
  {
    if(pageTable[i].isAllocated == false)
    {
      pageTable[i].pageID = address;
      pageTable[i].age = 1;
      pageTable[i].isAllocated = true;
      for(int j=0; j<page.tp; j++)
      {
        if(pageFrame[j].isAllocated == false)
        {
          pageFrame[j].isAllocated = true;
          pageFrame[j].processID = process.pid;
          pageFrame[j].pageID = i;
          pageTable[i].frameID = j;
          break;
        }
      }
      return;
    }
  }

  // Swap out LFU
  int min = 0;
  for(int i=1; i<page.r; i++)
  {
    if(pageTable[i].age < pageTable[min].age)
    {
      min = i;
    }
  }

  pageTable[min].pageID = address;
  pageTable[min].age = 1;
}

void LFUhelper(vector<Table> &pageTable, int address, Table* pageFrame)
{
  for(int i=1; i<page.r; i++)
  {
    if(pageTable[i].pageID == address)
    {
      pageTable[i].age++;
    }
  }
}

void OPTreplace(vector<Table> &pageTable, int address, Table* pageFrame, int index)
{
  // Adds to empty frame
  for(int i=0; i<page.r; i++)
  {
    if(pageTable[i].isAllocated == false)
    {
      pageTable[i].pageID = address;
      pageTable[i].isAllocated = true;
      for(int j=0; j<page.tp; j++)
      {
        if(pageFrame[j].isAllocated == false)
        {
          pageFrame[j].isAllocated = true;
          pageFrame[j].processID = process.pid;
          pageFrame[j].pageID = i;
          pageTable[i].frameID = j;
          break;
        }
      }
      return;
    }
  }
  
  int x = page.X;
  if(index + x > page.address.size()-1)
  {
    x = page.address.size() - 1 - index;
  }
  
  vector<int> distance(page.r);
  for(int i=0; i<page.r; i++)
  {
    distance[i] = -1;
  }

  // Updates distance vector
  for(int i=0; i<page.r; i++)
  {
    for(int j=index+1; j<index+x ; j++)
    {
      if(pageTable[i].pageID == page.address[j].address)
      {
        distance[i] = j;
        break;
      }
    }
  }

  // If distance is past lookout int then replace and exit
  for (int i = 1; i < distance.size(); i++) 
  {
    if(distance[i] == -1)
    {
      pageTable[i].pageID = address;
      return;
    }
  }

  int maxIndex = 0; // Start by assuming the first element is the smallest

  for (int i = 1; i < distance.size(); i++) 
  {
    if (distance[i] > distance[maxIndex]) 
    {
      maxIndex = i; // Update minIndex when a smaller element is found
    }
  }

}

void WShelper(vector<Table> &pageTable, int address, Table* pageFrame, int index)
{
  vector<int> lastX;
  int count = 0;

  for(int i = index; i >= 0; i--)
  {
    if(page.address[i].pid == process.pid)
    {
      if(find(lastX.begin(), lastX.end(), page.address[i].address) == lastX.end())
      {
        lastX.push_back(page.address[i].address);
      }
      count++;
      if(count == page.r)
      {
        break;
      }
    }
  }

  for(int i=0; i<lastX.size(); i++)
  {
    if(pageTable[i].isAllocated == true)
    {
      pageTable[i].pageID = lastX[i];
      pageTable[i].processID = process.pid;
    }
    else if(pageTable[i].isAllocated == false)
    {
      int pfIndex = 0;
      for(int j=0; j<page.tp; j++)
      {
        if(pageFrame[i].isAllocated == false)
        {
          pfIndex = j;
          break;
        }
      }
      pageFrame[pfIndex].isAllocated = true;
      pageFrame[pfIndex].pageID = i;
      pageFrame[pfIndex].processID = process.pid;
      pageTable[i].frameID = pfIndex;
      pageTable[i].isAllocated = true;
      pageTable[i].pageID = lastX[i];
    }
  }
  if(pageTable.size() >= wsMaxSize)
  {
    wsMaxSize = pageTable.size();
  }
}

void Print(int* faultCount, string algo)
{
  ofstream outfile("output.txt", ios::app);

  int total = 0;

  outfile << endl << "Running " << algo << " algorithm:" << endl;
    
  for(int i=0; i<page.k; i++)
  {
    outfile << "Page faults for process " << page.processes[i].pid << ": " <<  faultCount[i] << endl;
    total += faultCount[i];
  }
  outfile << "***** TOTAL PAGE FAULTS: " << total << " *****" << endl;

  if(algo == "WS")
  {
    outfile << "Minimum WS size: " << 0 << endl;
    outfile << "Maximum WS size: " << wsMaxSize << endl;
  }
  outfile.close();
}

// My disk driver process, handles all processes
void STARTDISK(string PageReplacementAlgo)
{
  /* Creating locked memory segment to share across processes */
  /* Allocated 512 bytes which is arbitrary, also created a random memKey */
  /* Created frameTable which is main memory simulation */
  cout << "Creating main memory segment for frameTable" << endl;
  int id_pageFrame = shmget(memKey++, 1024, 0666 | IPC_CREAT | IPC_EXCL);
  if (id_pageFrame == -1)
  {
    cout << "\nERROR creating a memory segment for frameTable !\n";
    return ;
  }
  Table* pageFrame = (Table*)shmat(id_pageFrame, NULL, 0);  
  // Initializing the array just in case
  for(int i = 0; i < page.tp; i++)
  {
    pageFrame[i].isAllocated = false;
    pageFrame[i].processID = -1;
    pageFrame[i].pageID = -1;
    pageFrame[i].frameID = -1;
    pageFrame[i].age = -1;
  }

  // Creating a memory space to keep track of page faults, easier than piping 
  cout << "Creating fault counter memory segment" << endl;
  int id_faultCount = shmget(memKey++, 256, 0666 | IPC_CREAT | IPC_EXCL);
  if (id_faultCount == -1)
  {
    cout << "\nERROR creating a memory segment for fault counter !\n";
    return ;
  }
    
  int* faultCount = (int*)shmat(id_faultCount, NULL, 0);
  for(int i=0; i<page.k; i++)
  {
    faultCount[i] = 0;
  }

  cout << "Creating Semaphores" << endl;
  CreateSemaphores();
  cout << "Creating processes" << endl;
  CreateFork();

  /* for testing processes */
  // if(process.pid == 100 || process.pid == 102)
  // {
  //   return;
  // }

  if(process.pid != -1)
  {
    // Create pageTable per process
    vector<Table> pageTable(page.r);
    
    start:
    // Initialize just in case
    for(int i=0; i < page.r; i++)
    {
      pageTable[i].isAllocated = false;
      pageTable[i].pageID = -1;
      pageTable[i].processID = -1;
      pageFrame[i].frameID = -1;
      pageFrame[i].age = -1;
    }

    for(int i=0; i<page.address.size(); i++) // Cycle through instructions
    {
      if(page.address[i].pid == process.pid) // Check if instruction is for current process
      {   
        if(page.address[i].address == -1) // Check if process is complete
        {
          // End process and clear Main memory
          cout << process.pid << " ended, terminating process" << endl;
          ClearProcess(pageFrame);
          PostSemaphore(process.order, "dead");
          return;
        }
        
        cout << process.pid << "s turn, handling address " << page.address[i].address << endl;
        cout << process.pid << " calling semaphore " << process.order << endl;
        /* uncomment to use step by step process */
        // WaitSemaphore(process.order, "live");
        if(PageFault(pageTable, page.address[i].address)) // Check for page fault
        {
          faultCount[process.order]++;
          cout << process.pid << " has a pagefault at " << page.address[i].address << endl;
          int placesOnMem = 0;
          if(!FreePoolSpace(pageFrame, placesOnMem) && placesOnMem != page.r) // Check if min freepool is respected
          {
            cout << "no free pool space for " << process.pid << endl;
            // halt process
            int majority = MajorityOnFrame(pageFrame);
            cout << "halting " << process.pid << " for " << page.processes[majority].pid << endl;
            ClearProcess(pageFrame);
            WaitSemaphore(majority, "dead");
            cout << process.pid << " unhalted" << endl;
            PostSemaphore(majority, "dead");
            // restart process from begenning
            goto start;
          }
          if(PageReplacementAlgo == "LIF0")
          {
            LIFOreplace(pageTable, page.address[i].address, pageFrame);
          }
          else if(PageReplacementAlgo == "MRU")
          {
            // same swaping logic at LIFO so why waste code
            LIFOreplace(pageTable, page.address[i].address, pageFrame);
          }
          else if(PageReplacementAlgo == "LRU")
          {
            LRU(pageTable, page.address[i].address, pageFrame, i);
          }
          else if(PageReplacementAlgo == "LFU")
          {
            LFUreplace(pageTable, page.address[i].address, pageFrame);
          }
          else if(PageReplacementAlgo == "OPT")
          {
            OPTreplace(pageTable, page.address[i].address, pageFrame, i);
          }
          else if(PageReplacementAlgo == "WS")
          {
            WShelper(pageTable, page.address[i].address, pageFrame, i);
          }
        }
        else
        {
          if(PageReplacementAlgo == "LIFO")
          {
            // do nothing
          }
          else if(PageReplacementAlgo == "MRU")
          {
            MRUhelper(pageTable, page.address[i].address, pageFrame);
          }
          else if(PageReplacementAlgo == "LRU")
          {
            // Do nothing
          }
          else if(PageReplacementAlgo == "LFU")
          {
            LFUhelper(pageTable, page.address[i].address, pageFrame);
          }
          else if(PageReplacementAlgo == "OPT")
          {
            // Do nothing
          }
          else if(PageReplacementAlgo == "WS")
          {
            WShelper(pageTable, page.address[i].address, pageFrame, i);
          }
        }
        cout << process.pid << " finished allocating " << page.address[i].address << endl;
        /* uncomment to use step by step process */
        // PostSemaphore(process.order, "live");
      }
      /* uncomment to use step by step process */
      // else
      // {
      //   // wait for process to finish
      // //   cout << process.pid << " is waiting its turn" << endl;
      //   sleep(1);
      //   for(int j=0; j<page.processes.size(); j++)
      //   {
      //     if(page.processes[j].pid == (page.address[i].pid))
      //     {
      //       WaitSemaphore(page.processes[j].order, "live");
      //       PostSemaphore(page.processes[j].order, "live"); 
      //       break;
      //     }
      //   }
      // }
    }
  }

  if(process.pid == -1)
  {
    cout << "Closing Semaphores" << endl;
    CloseSemaphore();
  
  
    // for(int i=0; i<page.k; i++)
    // {
    // //   cout << page.processes[i].pid << ": " << faultCount[i] << " Page Faults" << endl;
    // }

    Print(faultCount, PageReplacementAlgo);
  
    cout << "Closing main memory segment" << endl;
    if(shmdt(pageFrame) == -1) 
    {
      cout << "ERROR detaching shared memory\n";
    }
    shmctl(id_pageFrame, IPC_RMID, NULL);
  
    if(shmdt(faultCount) == -1) 
    {
      cout << "ERROR detaching shared memory\n";
    }
    shmctl(id_faultCount, IPC_RMID, NULL);
  }
}

int main(int argc, char* argv[])
{
	// Load paramaters from file here
	load("input.txt");

	cout << "\n---------------------------------------------------------------\n" << endl;
	cout << "\n-------------------------	Parameters	------------------------\n" << endl;
	cout << "\n---------------------------------------------------------------\n" << endl;
	cout << "Total number of pages: \t\t" << page.tp << endl;
	cout << "Total page size: \t\t\t" << page.ps << endl;
	cout << "Page frames per process: \t" << page.r << endl;
	cout << "Lookahead window: \t\t\t" << page.X << endl;
	cout << "Min free pool size: \t\t" << page.minimum << endl;
	cout << "Max free pool size: \t\t" << page.maximum << endl;
	cout << "Total num processes: \t\t" << page.k << endl;
	for(int i=0; i < page.k; i++)
	{
	  cout << "Process ID and size: \t\t" << page.processes[i].pid << " " << page.processes[i].size << endl;
	}

  cout << "\n---------------------------------------------------------------\n" << endl;
  cout << "\n---------------------------------------------------------------\n" << endl;

  // Opens or clears output file
  ofstream outfile("output.txt", ios::out);
  if(!outfile.is_open()) 
  {
    cerr << "Error opening file!" << endl;
    return 1;
  }
  outfile.close();
  
  STARTDISK("LIFO");
  if(process.pid != -1)
  {
    return 1;
  }
  STARTDISK("MRU");
  if(process.pid != -1)
  {
    return 1;
  }
  STARTDISK("LRU");
  if(process.pid != -1)
  {
    return 1;
  }
  STARTDISK("LFU");
  if(process.pid != -1)
  {
    return 1;
  }
  STARTDISK("OPT");
  if(process.pid != -1)
  {
    return 1;
  }
  STARTDISK("WS");
  if(process.pid != -1)
  {
    return 1;
  }
  return 0;
}