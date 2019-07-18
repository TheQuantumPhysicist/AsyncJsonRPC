#include "gtest/gtest.h"

#include "asyncjsonrpc/detail/ThreadPool.h"

void SumZeroToNumber(long& num)
{
    long val = 0;
    for (long i = 0; i <= static_cast<long>(num); i++) {
        val += i;
    }
    num = val;
}

TEST(ThreadPool, main)
{
    for (int tries = 0; tries < 2; tries++) {
        std::cout << "Try number: " << tries + 1 << "... ";

        std::vector<long> nums(100000);
        std::vector<long> numsResults(nums.size());
        std::vector<long> numsSequentialResults(nums.size());
        try {
            ThreadPool pool;
            for (unsigned i = 0; i < nums.size(); i++) {
                nums[i]                  = rand() % 1000;
                numsResults[i]           = nums[i];
                numsSequentialResults[i] = nums[i];
                // this is just a copy to be passed and
                // modified, while the original remains
                // unchanged
                pool.push(std::bind(SumZeroToNumber, std::ref(numsResults[i])));
            }
            long                  num = 10;
            std::function<void()> task(std::bind(SumZeroToNumber, std::ref(num)));
            pool.push(std::move(task));

            //    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            pool.finish();
        } catch (std::exception& ex) {
            std::cout << "An exception was thrown while processing data. Exception says: " << ex.what()
                      << std::endl;
            std::exit(1);
        }
        //  test results sequentially
        for (unsigned i = 0; i < nums.size(); i++) {
            SumZeroToNumber(numsSequentialResults[i]);
            EXPECT_EQ(numsSequentialResults[i], numsResults[i]);
        }
        std::cout << "success." << std::endl;
    }
}
