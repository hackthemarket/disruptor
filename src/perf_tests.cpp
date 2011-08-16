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

class ValueEntry : public AbstractEntry {
private:
	long _value;

public:
	long getValue() { return _value; }

    void setValue(const long value) { _value = value; }

	static EntryFactory<ValueEntry> ENTRY_FACTORY = new EntryFactory<ValueEntry>()
    {
        ValueEntry create() { return new ValueEntry();}
    };
}


template <typename T>
class ValueAdditionHandler : public BatchHandler<T>
{
    private long value;

    public long getValue()
    {
        return value;
    }

    public void reset()
    {
        value = 0L;
    }

    @Override
    public void onAvailable(final ValueEntry entry) throws Exception
    {
        value += entry.getValue();
    }

    @Override
    public void onEndOfBatch() throws Exception
    {
    }
}



/**
 * <pre>
 * UniCast a series of items between 1 producer and 1 consumer.
 *
 * +----+    +----+
 * | P0 |--->| C0 |
 * +----+    +----+
 *
 *
 * Queue Based:
 * ============
 *
 *        put      take
 * +----+    +====+    +----+
 * | P0 |--->| Q0 |<---| C0 |
 * +----+    +====+    +----+
 *
 * P0 - Producer 0
 * Q0 - Queue 0
 * C0 - Consumer 0
 *
 *
 * Disruptor:
 * ==========
 *                   track to prevent wrap
 *             +-----------------------------+
 *             |                             |
 *             |                             v
 * +----+    +====+    +====+    +====+    +----+
 * | P0 |--->| PB |--->| RB |<---| CB |    | C0 |
 * +----+    +====+    +====+    +====+    +----+
 *                claim      get    ^        |
 *                                  |        |
 *                                  +--------+
 *                                    waitFor
 *
 * P0 - Producer 0
 * PB - ProducerBarrier
 * RB - RingBuffer
 * CB - ConsumerBarrier
 * C0 - Consumer 0
 *
 * </pre>
 */
class UniCast1P1CPerfTest : public AbstractPerfTestQueueVsDisruptor {
private:

	static const int SIZE = 1024 * 32;
    static const long ITERATIONS = 1000L * 1000L * 500L;
    const ExecutorService EXECUTOR = Executors.newSingleThreadExecutor();
    static const long expectedResult;

    const long CalcExpectedResult()
    {
        long temp = 0L;
        for (long i = 0L; i < ITERATIONS; i++)
        {
            temp += i;
        }

        return temp;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    const BlockingQueue<Long> blockingQueue = new ArrayBlockingQueue<Long>(SIZE);
    const ValueAdditionQueueConsumer queueConsumer = new ValueAdditionQueueConsumer(blockingQueue);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    const RingBuffer<ValueEntry> ringBuffer =
        new RingBuffer<ValueEntry>(ValueEntry.ENTRY_FACTORY, SIZE,
                                   ClaimStrategy.Option.SINGLE_THREADED,
                                   WaitStrategy.Option.YIELDING);
    const ConsumerBarrier<ValueEntry> consumerBarrier = ringBuffer.createConsumerBarrier();
	const ValueAdditionHandler handler = new ValueAdditionHandler();
    const BatchConsumer<ValueEntry> batchConsumer = new BatchConsumer<ValueEntry>(consumerBarrier, handler);
    const ProducerBarrier<ValueEntry> producerBarrier = ringBuffer.createProducerBarrier(batchConsumer);

    ///////////////////////////////////////////////////////////////////////////////////////////////

public :
   // UniCast1P1CPerfTest() : expectedResult()


    void shouldCompareDisruptorVsQueues()
       // throws Exception
    {
        testImplementations();
    }

//    virtual long runQueuePass(const int passNumber) //throws InterruptedException
//    {
//        queueConsumer.reset();
//        Future future = EXECUTOR.submit(queueConsumer);
//        long start = System.currentTimeMillis();
//
//        for (long i = 0; i < ITERATIONS; i++)
//        {
//            blockingQueue.put(Long.valueOf(i));
//        }
//
//        const long expectedSequence = ITERATIONS - 1L;
//        while (queueConsumer.getSequence() < expectedSequence)
//        {
//            // busy spin
//        }
//
//        long opsPerSecond = (ITERATIONS * 1000L) / (System.currentTimeMillis() - start);
//        queueConsumer.halt();
//        future.cancel(true);
//
//        assert(expectedResult == queueConsumer.getValue());
//
//        return opsPerSecond;
//    }

    virtual long runDisruptorPass(const int passNumber)// throws InterruptedException
    {
        handler.reset();
        EXECUTOR.submit(batchConsumer);
        long start = System.currentTimeMillis();

        for (long i = 0; i < ITERATIONS; i++)
        {
            ValueEntry entry = producerBarrier.nextEntry();
            entry.setValue(i);
            producerBarrier.commit(entry);
        }

        const long expectedSequence = ringBuffer.getCursor();
        while (batchConsumer.getSequence() < expectedSequence)
        {
            // busy spin
        }

        long opsPerSecond = (ITERATIONS * 1000L) / (System.currentTimeMillis() - start);
        batchConsumer.halt();

        assert(expectedResult == handler.getValue());

        return opsPerSecond;
    }
}; // UniCast1P1CPerfTest

void main(char** args) {

	UniCast1P1CPerfTest test();
	test.shouldCompareDisruptorVsQueues();

}
