#include <algorithm>
#include <mpi.h>
#include <fstream>

//#define DEBUG

#define DEFAULT_TAG 1

#define MASTER_RANK 0

#define INPUT_ERR 	1
#define ARG_ERR 	2

/** \brief Determines if process is sorting during given iteration.
  * \param odd_iteration Determines if current iteration is odd.
  * \param proc_cnt Number of processes sorting.
  * \param rank Rank of process
  * \return Returns true if process is participating on sorting this iteration.
  */
bool isNodeSorting(bool odd_iteration, int proc_cnt, int rank)
{
	// odd iteration
	if (odd_iteration)
	{
		// first process can't do anything
		// process count is even => last process can't do anything
		if ((!(proc_cnt % 2) && (rank == proc_cnt - 1)) || !(rank))
			return false;
		else
			return true;
	}

	// even iteration
	else
	{
		// process count is odd => last process can't do anything
		if ((proc_cnt % 2) && (rank == proc_cnt - 1))
			return false;
		else
			return true;
	}
}

/**
 * \brief Function prints numbers of process in order of processes.
 * \param number Vector of numbers.
 * \param rank Rank of process.
 * \param proc_count Total number of processes.
 * \param chunk_size Number of bytes each process has.
 */
void printAllNumbers(std::vector<unsigned char>& numbers, int rank, int proc_count, int chunk_size)
{
	MPI_Status status;

	int tmp;
	if (rank)
		MPI_Recv(&tmp, 1, MPI_INT, rank - 1, DEFAULT_TAG, MPI_COMM_WORLD, &status);

    if (rank == MASTER_RANK)
        std::cout   << "----------------------------------------" << std::endl;
    std::cout << "Process " << rank << " has these values: ";
	for (int i = 0; i < chunk_size; ++i)
		std::cout << static_cast<int>(numbers[i]) << " ";
	std::cout << std::endl;

	if (rank != proc_count - 1)
		MPI_Send(&tmp, 1, MPI_INT, rank + 1, DEFAULT_TAG, MPI_COMM_WORLD);

	MPI_Barrier(MPI_COMM_WORLD);
}

/** \brief Function reads input from given filepath and saves it to vector of chars.
  * \param filepath Path to input file
  * \return Returns vector of bytes read from input.
  */
std::vector<unsigned char> getNumbers(const char* filepath)
{
	std::ifstream input(filepath, std::ios::in | std::ios::binary);
	
	if (input)
	{
		try
		{
			input.exceptions (std::ifstream::failbit | std::ifstream::badbit);
			input.seekg(0, std::ios::end);
			
			std::vector<char> contents = std::vector<char>(input.tellg());
			input.seekg(0, std::ios::beg);

			input.read(&contents[0], contents.size());
			input.close();
			return std::vector<unsigned char>(contents.begin(), contents.end());
		}
		catch (std::exception const& e)
		{
			std::cerr << "Error occurred while reading from file:" << e.what() << std::endl;
			MPI_Abort(MPI_COMM_WORLD, INPUT_ERR);
		}
	}
	else
	{
		std::cerr << "Error occurred while opening input file." << std::endl;
		exit(INPUT_ERR);
	}
}

int main(int argc, char **argv)
{  
	if (argc != 2)
	{
		std::cerr << "Error: The only argument is path to input file." << std::endl;
		MPI_Abort(MPI_COMM_WORLD, ARG_ERR);
	}

	MPI_Init(&argc, &argv);  

	std::vector<unsigned char> numbers;
	int rank, proc_count, chunk_size;
	MPI_Status status;
	MPI_Request request;

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &proc_count);

	// data type for one chunk of data
	MPI_Datatype number_chunk;

	// communicators between pairs of processes	
	MPI_Comm even_comm;
	MPI_Comm odd_comm;

	MPI_Comm_split(MPI_COMM_WORLD, proc_count - proc_count%2, 0, &even_comm);
	MPI_Comm_split(MPI_COMM_WORLD, proc_count + proc_count%2, 0, &odd_comm);

	if (rank == MASTER_RANK)
	{
		// load input
		numbers = getNumbers(argv[1]);

		if (numbers.size() % proc_count != 0)
		{
			std::cerr << "Input length is not divisible by number of processes." << std::endl;
			MPI_Abort(MPI_COMM_WORLD, INPUT_ERR);
		}

		#ifdef DEBUG
			std::cout << "Input array: " << std::endl;
		#endif

        // print input
		for (unsigned char &number : numbers)
            std::cout << (int) number << " ";
		std::cout << std::endl;

		chunk_size = static_cast<int>(numbers.size() / proc_count);

		// send chunk size
		MPI_Bcast(&chunk_size, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

		// create datatype
		MPI_Type_contiguous(chunk_size, MPI_UNSIGNED_CHAR, &number_chunk);
		MPI_Type_commit(&number_chunk);

		numbers.resize(chunk_size * 2);
	}
	else
	{
		// recieve chunk size
		MPI_Bcast(&chunk_size, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

		numbers = std::vector<unsigned char>(chunk_size * 2);

		// create type
		MPI_Type_contiguous(chunk_size, MPI_UNSIGNED_CHAR, &number_chunk);
		MPI_Type_commit(&number_chunk);
	}

	// scatter data to processes
	MPI_Scatter(&numbers[0], 1, number_chunk, &numbers[0], 1, number_chunk, MASTER_RANK, MPI_COMM_WORLD);

    #ifdef DEBUG
        printAllNumbers(numbers, rank, proc_count, chunk_size);
    #endif

	MPI_Comm current_comm = even_comm;

	for (int iter_cnt = 0; iter_cnt < proc_count + proc_count % 2; iter_cnt++)
	{
		if (isNodeSorting(iter_cnt % 2, proc_count, rank))
			if ((rank + iter_cnt)% 2)
			{
				MPI_Isend(&numbers[0], 1, number_chunk, rank - 1, DEFAULT_TAG, current_comm, &request);
				MPI_Irecv(&numbers[0], 1, number_chunk, rank - 1, DEFAULT_TAG, current_comm, &request);
			}
			else 
			{
				MPI_Irecv(&numbers[chunk_size], 1, number_chunk, rank + 1, DEFAULT_TAG, current_comm, &request);
				MPI_Wait(&request, &status);

				std::sort (numbers.begin(), numbers.end());

				MPI_Isend(&numbers[chunk_size], 1, number_chunk, rank + 1, DEFAULT_TAG, current_comm, &request);
			}
		
		MPI_Wait(&request, &status);

		// DEBUG: delete after
		//MPI_Barrier(MPI_COMM_WORLD);

		if (current_comm == even_comm)
			current_comm = odd_comm;
		else 
			current_comm = even_comm;
	}

    #ifdef DEBUG
        printAllNumbers(numbers, rank, proc_count, chunk_size);
    #endif


    //MPI_Barrier(MPI_COMM_WORLD);

	if (rank == MASTER_RANK)
	{
		numbers.resize(chunk_size * proc_count, 0);
	}

	MPI_Gather(&numbers[0], 1, number_chunk, &numbers[0], 1, number_chunk, MASTER_RANK, MPI_COMM_WORLD);

	//std::cout << "Rank " << rank << " has chunk size " << chunk_size << std::endl;

    if (rank == MASTER_RANK)
	{
        #ifdef DEBUG
            std::cout << "Ziskana postupnost: " << std::endl;
        #endif
        for (unsigned char &number : numbers)
            std::cout << (int) number << std::endl;
	}

	MPI_Type_free(&number_chunk);

	MPI_Finalize();
}

