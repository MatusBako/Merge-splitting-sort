#include <algorithm>
#include <mpi.h>
#include <fstream>
#include <unistd.h>

//#define DEBUG

#define DEFAULT_TAG 1

#define MASTER_RANK 0

#define INPUT_ERR 	1
#define ARG_ERR 	2

bool isTimedRun(int argc, char** argv)
{
    bool timed_run = false;
    int opt;

    while ((opt = getopt(argc, argv, "t")) != -1) {
        switch (opt) {
            case 't':
                timed_run = true;
                break;
            case '?':
                std::cerr << "Unknown option: '" << char(optopt) << "'!" << std::endl;
                break;
        }
    }

    return timed_run;
}

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
 * \brief Function prints numbers of process in order of processes for debug purposses.
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
		MPI_Abort(MPI_COMM_WORLD, INPUT_ERR);
	}
}

int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);

	unsigned int fill_count = 0;
	int rank, proc_count, chunk_size;
    double start_time, end_time;
    bool timed_run;
    std::vector<unsigned char> numbers;
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
		if (argc < 2 || argc > 3)
		{
			std::cerr << argc << "Error: The only argument is path to input file." << std::endl;
			MPI_Abort(MPI_COMM_WORLD, ARG_ERR);
		}

		if (argc == 3 && strcmp(argv[2],"-t"))
		{
			std::cerr << "Error: Invalid third argument." << std::endl;
			MPI_Abort(MPI_COMM_WORLD, ARG_ERR);
		}

		// load input
		numbers = getNumbers(argv[1]);

		// getopt() destroys arguments!!
        timed_run = isTimedRun(argc, argv);

		if (numbers.size() < proc_count)
		{
			std::cerr << "Length of input vector is less than number of processes!" << std::endl;
			MPI_Abort(MPI_COMM_WORLD, INPUT_ERR);
		}

		#ifdef DEBUG
			std::cout << "Input array: " << std::endl;
		#endif

        // print input
        if (!timed_run)
        {
            for (unsigned char &number : numbers)
                std::cout << (int) number << " ";
            std::cout << std::endl;
        }

		if (numbers.size() % proc_count != 0)
		{
			fill_count = proc_count - (numbers.size() % proc_count);
			std::vector<unsigned char> fill (fill_count, 255);
			numbers.insert(numbers.end(), fill.begin(), fill.end());
		}

		chunk_size = static_cast<int>(numbers.size() / proc_count);

		// send chunk size
		MPI_Bcast(&chunk_size, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

		// create datatype
		MPI_Type_contiguous(chunk_size, MPI_UNSIGNED_CHAR, &number_chunk);
		MPI_Type_commit(&number_chunk);

		numbers.resize(chunk_size * 2);

        if (timed_run)
            start_time = MPI_Wtime();
	}
	else
	{
		// recieve chunk size
		MPI_Bcast(&chunk_size, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

		numbers = std::vector<unsigned char>(chunk_size * 2);

		// create data type
		MPI_Type_contiguous(chunk_size, MPI_UNSIGNED_CHAR, &number_chunk);
		MPI_Type_commit(&number_chunk);
	}

	// scatter data to processes
	MPI_Scatter(&numbers[0], 1, number_chunk, &numbers[0], 1, number_chunk, MASTER_RANK, MPI_COMM_WORLD);

    #ifdef DEBUG
        printAllNumbers(numbers, rank, proc_count, chunk_size);
    #endif

	MPI_Comm current_comm = even_comm;

    // odd process count => two more iterations
	for (int iter_cnt = 0; iter_cnt < proc_count + 2* (proc_count % 2); iter_cnt++)
	{
        #ifdef DEBUG
            MPI_Barrier(MPI_COMM_WORLD);
            if (!rank)
            {
                std::cout << "Iteration " << iter_cnt + 1 << std::endl;
                fflush(stdout);
            }
        #endif

		if (isNodeSorting(iter_cnt % 2, proc_count, rank))
        {
            if ((rank + iter_cnt) % 2)
            {
                #ifdef DEBUG
                    std::cout << "I'm rank " << rank << ", sending vector and recieving sorted." << std::endl;
                    fflush(stdout);
                #endif

                MPI_Isend(&numbers[0], 1, number_chunk, rank - 1, DEFAULT_TAG, current_comm, &request);
                MPI_Irecv(&numbers[0], 1, number_chunk, rank - 1, DEFAULT_TAG, current_comm, &request);
            }
            else
            {
                #ifdef DEBUG
                    std::cout << "I'm rank " << rank << ", waiting for vector and sorting." << std::endl;
                    fflush(stdout);
                #endif

                MPI_Recv(&numbers[chunk_size], 1, number_chunk, rank + 1, DEFAULT_TAG, current_comm, &status);
                //MPI_Wait(&request, &status);

                std::sort(numbers.begin(), numbers.end());

                MPI_Isend(&numbers[chunk_size], 1, number_chunk, rank + 1, DEFAULT_TAG, current_comm, &request);
            }

            MPI_Wait(&request, &status);
        }

		if (current_comm == even_comm)
			current_comm = odd_comm;
		else 
			current_comm = even_comm;
	}

    #ifdef DEBUG
        if (!rank)
            std::cout << "Sorted numbers: " << std::endl;
        printAllNumbers(numbers, rank, proc_count, chunk_size);
    #endif

	if (rank == MASTER_RANK)
	{
		numbers.resize(chunk_size * proc_count, 0);
	}

	MPI_Gather(&numbers[0], 1, number_chunk, &numbers[0], 1, number_chunk, MASTER_RANK, MPI_COMM_WORLD);

    if (rank == MASTER_RANK)
	{
        if (timed_run)
        {
            end_time = MPI_Wtime();
            std::cout << end_time - start_time << std::endl;
        }
        else
		{
			numbers.resize(numbers.size() - fill_count);
			for (unsigned char &number : numbers)
				std::cout << (int) number << std::endl;
		}

	}

	MPI_Type_free(&number_chunk);
	MPI_Finalize();
}

