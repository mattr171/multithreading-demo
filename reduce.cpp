#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

constexpr int rows = 1000;	///< the number of rows in the work matrix
constexpr int cols = 100;	///< the number of cols in the work matrix

std::mutex stdout_lock;		///< for serializing access to stdout

std::mutex counter_lock;	///< for dynamic balancing only
volatile int counter = rows;	///< for dynamic balancing only

std::vector<int> tcount;	///< count of rows summed for each thread
std::vector<uint64_t> sum;	///< the calculated sum from each thread

int work[rows][cols];		///< the matrix to be summed


/**
 * Use static load balancing to sum the rows of a 2-dimensional matrix
 *
 * This function uses static load balancing to determine
 * which threads will sum each row. The thread will add up the
 * row corresponding to its ID followed by each row which is
 * num_rows from the previous. For example, if there are 3 threads,
 * the first thread will sum row 0, row 3, row 6, etc
 *
 * @param tid int containing thread ID
 * @param num_threads int containing number of threads to use
 *
 * @note stdout_locks used to serialize access to stdout
 *
 ********************************************************************************/
void sum_static(int tid, int num_threads)
{
	stdout_lock.lock();
	std::cout << "Thread " << tid << " starting" << std::endl;
	stdout_lock.unlock();

	int i = tid;
	while (i < rows)
	{
		for (int j = 0; j < cols; ++j)
		{
			sum[tid] += (uint64_t) work[i][j];
		}
		++tcount[tid];
		i += num_threads;
	}

	stdout_lock.lock();
        std::cout << "Thread " << tid << " ending tcount=" << tcount[tid] << " sum=" << sum[tid] << std::endl;
	stdout_lock.unlock();
}

/**
 * Use dynamic load balancing to sum the rows of a 2-dimensional matrix
 *
 * This function uses dynamic load balancing to determine which threads will
 * sum each row. A copy of a volatile counter is used to iterate through the
 * rows of the matrix, adding each column to a vector containing the sums
 * of the rows added by each thread. After a thread had summed a row, its
 * index in the tcount vector is incremented by one.
 *
 * @param tid int containing thread ID
 *
 * @note stdout_locks used to serialize access to stdout, counter_lock used
 * to prevent extraneous access to counter
 *
 ********************************************************************************/
void sum_dynamic(int tid)
{
	stdout_lock.lock();
	std::cout << "Thread " << tid << " starting" << std::endl;
	stdout_lock.unlock();

	bool done = false;
	while (!done)
	{
		int count_copy;

		counter_lock.lock();
		{
			if (counter > 0)
				--counter;
			else
				done = true;
			count_copy = counter;
		}
		counter_lock.unlock();

		if (!done)
		{
			for (int j = 0; j < cols; ++j)
				sum[tid] += (uint64_t) work[count_copy][j];

			++tcount[tid];
		}
	}

	stdout_lock.lock();
	std::cout << "Thread " << tid << " ending tcount=" << tcount[tid] << " sum=" << sum[tid] << std::endl;
	stdout_lock.unlock();
}

/**
 * Driver function for summing a 2-dimensional matrix using static or
 * dynamic load balancing
 *
 * This function fills a 2-dimensional array with random numbers
 * seeded from 0x1234. getopt() is then used to check that any command
 * line arguments were passed correctly: -d to indicate use of dynamic
 * load balancing, -t to specify the number of threads to use. An additional
 * check is included to ensure the number of threads requested does not exceed
 * the value in std::thread::hardware_concurrency(). If it does, the maximum
 * number of allowed threads will be used. Otherwise, the program will
 * default to static load balancing using 2 threads. An array of threads
 * is initialized, and the tcount and sum vectors are resized based on
 * the number of requested threads. Then either sum_static() or sum_dynamic()
 * are called to sum the rows. The threads are then joined and deleted and
 * the sum vector is added together to produce the gross sum.
 *
 * @param argc Command line argument count
 * @param argv Command line arguments
 *
 * @return Returns 0 to indicate the program executed successfully
 *
 * @note This is how you can add an optional note about the function that
 *    may be of interest to someone using it.
 *
 ********************************************************************************/
int main(int argc, char **argv)
{
	int opt;
	int dflag = 0;
	unsigned int n_threads = 2;


	std::cout << std::thread::hardware_concurrency() << " concurrent threads supported." << std::endl;

	srand(0x1234);
	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
			work[i][j] = rand();
	}


	while ((opt = getopt(argc, argv, "dt:")) != -1)
	{
		switch (opt)
		{
			case 'd':
			{
				dflag = 1;
				break;
			}

			case 't':
			{
				n_threads = atoi(optarg);
				if (n_threads > std::thread::hardware_concurrency())
					n_threads = std::thread::hardware_concurrency();

				break;
			}

			case '?':
			{
				dflag = -1;
       				std::cerr << "Usage: reduce [-d] [-t num]" << std::endl;
        			std::cerr << "\t-d Use dynamic load-balancing." << std::endl;
        			std::cerr <<"\t-t Specify the number of threads to use" << std::endl;
				break;
			}

			default:
				break;
		}
	}

	if (dflag != -1)
        {

		std::vector<std::thread*> threads;
		tcount.resize(n_threads, 0);
		sum.resize(n_threads, 0);

		if (dflag == 1)
		{
			for (unsigned int i = 0; i < n_threads; ++i)
				threads.push_back(new std::thread(sum_dynamic, i));
		}
		else if (dflag == 0)
		{
			for (unsigned int i = 0; i < n_threads; ++i)
				threads.push_back(new std::thread(sum_static, i, n_threads));
		}

		int total_work = 0;

		for (unsigned int i = 0; i < n_threads; ++i)
               	{
                       	threads.at(i)->join();
                       	delete threads.at(i);
                       	total_work += tcount.at(i);
               	}

		uint64_t gross_sum = 0;
		for (uint64_t n : sum)
			gross_sum += n;

		std::cout << "main() exiting,  total_work=" << total_work << " gross_sum=" << gross_sum << std::endl;
	}

	return 0;
}
