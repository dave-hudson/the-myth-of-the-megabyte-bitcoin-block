/*
 * mb_block.c
 *	Code used to generate the reports in "The Myth Of The Megabyte Bitcoin Block"
 *	on hashingit.com
 *
 * Copyright (C) 2015 David Hudson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cmath>

/*
 * pool_blocksize
 *	Custom pool info record.
 *
 * Yes, I know that this isn't "good" C++ style and I should wrap things in
 * accessor functions, but life's too short :-)
 */
class pool_blocksize {
public:
	time_t time;
	char poolname[32];
	int largest_block;
	double fraction;
};

/*
 * time_rec
 *	Simple time record.
 */
class time_rec {
protected:
	time_t time;
	double datum;

public:
	time_rec(time_t t, double d);
	time_t get_time();
	double get_datum();
};

/*
 * time_rec::time_rec()
 *	Simple constructor.
 */
inline time_rec::time_rec(time_t t, double d)
{
	time = t;
	datum = d;
}

/*
 * time_rec::get_time()
 *	Simple accessor function.
 */
inline time_t time_rec::get_time()
{
	return time;
}

/*
 * time_rec::get_datum()
 *	Simple accessor function.
 */
inline double time_rec::get_datum()
{
	return datum;
}

std::vector<pool_blocksize> pool_blocksize_by_weeks;
std::vector<time_rec> max_blocksize_by_weeks;
std::vector<time_rec> blocksize_by_days;
std::vector<time_rec> blocksize_by_weeks;
std::vector<time_rec> conf_delay_by_days;
std::vector<time_rec> conf_delay_by_weeks;
std::vector<time_rec> hash_rate_by_days;
std::vector<time_rec> hash_rate_by_weeks;

/*
 * scan_pool_blocksize_csv()
 *	Scan the pool_blocksize.csv file.
 *
 * Return the time of the first record.
 */
int scan_pool_blocksize_csv()
{
	std::ifstream f;
	
	f.open("pool_blocksize.csv", std::ios::in);

	/*
	 * Seriously lazy code with no error handling!  We *know* that the file has a very
	 * specific format.
	 */
	char s[256];
	while (f) {
		f.getline(s, 256);
		if (f.eof()) {
			break;
		}

		int time = atoi(&s[0]);
		char *comma1 = strchr(s, ',');
		int largest_block = atoi(comma1 + 1);
		char *comma2 = strchr(comma1 + 1, ',');
		char *comma3 = strchr(comma2 + 1, ',');
		double fraction = atof(comma3 + 1);

		pool_blocksize pb;
		pb.time = time;
		pb.largest_block = largest_block;
		pb.fraction = fraction;
		int namesz = comma3 - comma2;
		memcpy(pb.poolname, &comma2 + 1, namesz);
		pb.poolname[namesz] = '\0';
		pool_blocksize_by_weeks.push_back(pb);
	}

	int start_time = pool_blocksize_by_weeks.front().time;
	double total_sz = 0.0;
	for (std::vector<pool_blocksize>::iterator i = pool_blocksize_by_weeks.begin();
	     i != pool_blocksize_by_weeks.end();
	     i++) {
		if (i->time == start_time) {
			total_sz += (i->fraction * i->largest_block);
		} else {
			time_rec tr(start_time, total_sz);
			max_blocksize_by_weeks.push_back(tr);

			total_sz = (i->fraction * i->largest_block);
			start_time = i->time;
		}
	}

	return pool_blocksize_by_weeks.front().time;
}

/*
 * scan_blockchain_csv()
 *	Scan a blockchain.info CSV file.
 */
void scan_blockchain_csv(const char *file_name, int discard_until, std::vector<time_rec> &day_vec,
			 std::vector<time_rec> &week_vec)
{
	std::ifstream f;
	
	f.open(file_name, std::ios::in);

	/*
	 * Seriously lazy code with no error handling!  We *know* that the file has a very
	 * specific format.
	 */
	char s[256];
	while (f) {
		f.getline(s, 256);
		if (f.eof()) {
			break;
		}

		int day = atoi(&s[0]);
		int month = atoi(&s[3]);
		int year = atoi(&s[6]);

		struct tm t;
		t.tm_sec = 0;
		t.tm_min = 0;
		t.tm_hour = 0;
		t.tm_mday = day;
		t.tm_mon = month - 1;
		t.tm_year = year - 1900;
		time_t tim = mktime(&t);

		/*
		 * Ignore anything prior to "discard_until".
		 */
		if (tim < discard_until) {
			continue;
		}

		time_rec tr(tim, atof(&s[20]));
		day_vec.push_back(tr);
	}

	/*
	 * Having scanned the block sizes by day, convert to block sizes by week.
	 */
	int start_time = day_vec.front().get_time();
	int end_time = start_time + (24 * 3600 * 7);
	double tot_data = 0.0;
	for (std::vector<time_rec>::iterator i = day_vec.begin(); i != day_vec.end(); i++) {
		if (i->get_time() < end_time) {
			tot_data += i->get_datum();
		} else {
			time_rec tr(start_time, (tot_data / 7.0));
			week_vec.push_back(tr);

			start_time = end_time;
			end_time += (24 * 3600 * 7);
			tot_data = i->get_datum();
		}
	}
}

/*
 * main()
 */
int main(int argc, char **argv)
{
	/*
	 * Start by scanning our input files and establishing baseline data.
	 */
	int discard_before = scan_pool_blocksize_csv();
	scan_blockchain_csv("blocksize.csv", discard_before, blocksize_by_days, blocksize_by_weeks);
	scan_blockchain_csv("conf_delay.csv", discard_before, conf_delay_by_days, conf_delay_by_weeks);
	scan_blockchain_csv("hash_rate.csv", discard_before, hash_rate_by_days, hash_rate_by_weeks);

	/*
	 * We now have vectors of data arranged by week from the same time index.  Render them!
	 */
	int largest_index = static_cast<int>(blocksize_by_weeks.size());
	for (int i = 0; i < largest_index; i++) {
		/*
		 * We want date and Unix time outputs.
		 */
		time_t t = max_blocksize_by_weeks[i].get_time();
		struct tm *tm = gmtime(&t);

		/*
		 * Data for simple block size estimate and block occupancy.  That let's
		 * us compute the percentage occupancy.
		 */
		double max_bs = max_blocksize_by_weeks[i].get_datum();
		double bs = blocksize_by_weeks[i].get_datum() * 1000000.0;

		/*
		 * Look at whether the hash rate changes can be used to compute anything
		 * interesting in terms of mean block finding rates over time.
		 */
		double hr = hash_rate_by_weeks[i].get_datum();
		double next_hr = hash_rate_by_weeks[i + 1].get_datum();
		double block_find_rate = 10.0 / (next_hr / hr);

		/*
		 * Confirmation delay info.
		 */
		double cd = conf_delay_by_weeks[i].get_datum();

		/*
		 * Render!
		 */
		char s[128];
		sprintf(s, "%02d:%02d:%04d | %10d | %7.0f | %7.0f | %3.1f | %8.1f | %5.2f | %5.2f | %5.2f\n",
			tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900, static_cast<int>(t),
			max_bs, bs, (bs / max_bs) * 100.0, hr / 1000.0, block_find_rate,
			cd, cd * (next_hr / hr));
		std::cout << s;
	}
}

