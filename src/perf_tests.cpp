/*
 * perf_tests.cpp
 *
 *  Created on: Aug 12, 2011
 *      Author: tingarg
 */

#include "disruptor.hpp"


class AbstractPerfTestQueueVsDisruptor {

public :

    void testImplementations()
    //throws Exception
    {
        const int RUNS = 3;
        long disruptorOps = 0L;
        long queueOps = 0L;

        for (int i = 0; i < RUNS; i++)
        {
//            System.gc();

            disruptorOps = runDisruptorPass(i);
            queueOps = runQueuePass(i);

            printResults(disruptorOps, queueOps, i);
        }

 //       Assert.assertTrue("Performance degraded", disruptorOps > queueOps);
    }


    void printResults(const long disruptorOps, const long queueOps, const int i)
    {
    	std::cout << testName() << " OpsPerSecond run #" << i
    			<< " : BlockingQueue=" << queueOps << ", Disruptor="
    			<< disruptorOps << std::endl;
    }

    virtual long runQueuePass(int passNumber) = 0; //throws Exception;

    virtual long runDisruptorPass(int passNumber) = 0; //throws Exception;

    virtual void shouldCompareDisruptorVsQueues() = 0; // throws Exception;

    virtual std::string testName() = 0;
}; // AbstractPerfTestQueueVsDisruptor
