/*
 * disruptor.hpp
 *
 *  Created on: Aug 8, 2011
 *      Author: tingarg
 */

#ifndef DISRUPTOR_HPP_
#define DISRUPTOR_HPP_

#include <tbb/atomic.h>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace disruptor {

/**
  * Calculate the next power of 2, greater than or equal to x.<p>
  * From Hacker's Delight, Chapter 3, Harry S. Warren Jr.
  *
  * @param x Value to round up
  * @return The next power of 2 from x inclusive
  */
 int ceilingNextPowerOfTwo(const int x) {
     return 1 << (32 - Integer.numberOfLeadingZeros(x - 1));
 }

 /**
  * Get the minimum sequence from an array of {@link Consumer}s.
  *
  * @param consumers to compare.
  * @return the minimum sequence found or Long.MAX_VALUE if the array is empty.
  */
long getMinimumSequence(const std::vector<Consumer*> consumers)
 {
     long minimum = Long.MAX_VALUE;

//     for (Consumer consumer : consumers)
//     {
//         long sequence = consumer.getSequence();
//         minimum = minimum < sequence ? minimum : sequence;
//     }
//
//     return minimum;
 }



class AbstractEntry {
private:
    long _sequence;

public:

    virtual ~AbstractEntry() {}

    /**Get the sequence number assigned to this item in the series.    */
    virtual long getSequence () const { return _sequence; }

    /**
	 * Explicitly set the sequence number for this Entry and a CommitCallback
	 * for indicating when the producer is finished with assigning data
	 * for exchange.     */
    virtual void setSequence(const long sequence) { _sequence = sequence; }

    std::string toString() const {
    	std::string str ("AbstractEntry#");
    	str += _sequence;
    	return str;
    }

}; // class AbstractEntry


std::ostream& operator<<(std::ostream& os, AbstractEntry& entry) {
	os << entry.toString();
	return os;
}


/**
 * EntryConsumers waitFor {@link AbstractEntry}s to become available for
 * consumption from the {@link RingBuffer}
 */
class Consumer {
public:
	/**
	 * Get the sequence up to which this Consumer has consumed
	 * {@link AbstractEntry}s
	 */
    virtual long getSequence() = 0;

    /**
     * Signal that this Consumer should stop when it has finished consuming at
     * the next clean break.  It will call {@link ConsumerBarrier#alert()} to
     * notify the thread to check status.
     */
    virtual void halt() = 0;

    // Runnable...
    virtual void run() = 0;

}; // class consumer

/**
 * Coordination barrier for tracking the cursor for producers and sequence of
 * dependent {@link Consumer}s for a {@link RingBuffer}
 *
 * @param <T> {@link AbstractEntry} implementation stored in the {@link RingBuffer}
 */
template < typename T >
class ConsumerBarrier {
//public interface ConsumerBarrier<T extends AbstractEntry>
public:

	virtual ~ConsumerBarrier() {}

    /**
     * Get the {@link AbstractEntry} for a given sequence from the
     * underlying {@link RingBuffer}.
     */
    virtual T getEntry(long sequence);

    /**
     * Wait for the given sequence to be available for consumption.
     *
     * @param sequence to wait for
     * @return the sequence up to which is available
     * @throws AlertException if a status change has occurred for the Disruptor
     * @throws InterruptedException if the thread needs awaking on a condition variable.
     */
    virtual long waitFor(long sequence) ; //throws AlertException, InterruptedException;

    /**
     * Wait for the given sequence to be available for consumption with a time out.
     *
     * @param sequence to wait for
     * @param timeout value
     * @param units for the timeout value
     * @return the sequence up to which is available
     * @throws AlertException if a status change has occurred for the Disruptor
     * @throws InterruptedException if the thread needs awaking on a condition variable.
     */
    virtual long waitFor(long sequence, boost::posix_time::time_duration timeout) ;
//    long waitFor(long sequence, long timeout, TimeUnit units) throws AlertException, InterruptedException;

    /**
     * Delegate a call to the {@link RingBuffer#getCursor()}
     * @return value of the cursor for entries that have been published.
     */
    virtual long getCursor();

    /**
     * The current alert status for the barrier.
     *
     * @return true if in alert otherwise false.
     */
    virtual bool isAlerted();

    /**
     * Alert the consumers of a status change and stay in this status until cleared.
     */
    virtual void alert();

    /**
     * Clear the current alert status.
     */
    virtual void clearAlert();

};

/**
 * Callback interface to be implemented for processing {@link AbstractEntry}s
 * as they become available in the {@link RingBuffer}
 *
 * @see BatchConsumer#setExceptionHandler(ExceptionHandler) if you want to
 * handle exceptions propagated out of the handler.
 *
 * @param <T> AbstractEntry implementation storing the data for sharing
 * during exchange or parallel coordination of an event.
 */
template <typename T>
class BatchHandler {
//public interface BatchHandler<T extends AbstractEntry>
public:

	virtual ~BatchHandler() {}

    /**
     * Called when a publisher has committed an {@link AbstractEntry} to
     * the {@link RingBuffer}
     *
     * @param entry committed to the {@link RingBuffer}
     * @throws Exception if the BatchHandler would like the exception handled
     * further up the chain.
     */
    virtual void onAvailable(T entry) ; //throws Exception;

    /**
     * Called after each batch of items has been have been processed before
     * the next waitFor call on a {@link ConsumerBarrier}.
     * <p>
     * This can be taken as a hint to do flush type operations before waiting
     * once again on the {@link ConsumerBarrier}.  The user should not expect
     * any pattern or frequency to the batch size.
     *
     * @throws Exception if the BatchHandler would like the exception handled
     * further up the chain.
     */
    virtual void onEndOfBatch();// throws Exception;
};

/**
 * Callback handler for uncaught exceptions in the {@link AbstractEntry}
 * processing cycle of the {@link BatchConsumer}
 */
class ExceptionHandler {
public:

	/**
     * Strategy for handling uncaught exceptions when processing an {@link AbstractEntry}.
     *
     * If the strategy wishes to suspend further processing by the {@link BatchConsumer}
     * then is should throw a {@link RuntimeException}.
     *
     * @param ex the exception that propagated from the {@link BatchHandler}
     * @param currentEntry being processed when the exception occurred.
     */
	virtual void handle(const void * ex, const AbstractEntry& currentEntry) = 0;
	//    void handle(Exception ex, AbstractEntry currentEntry);

};

/**
 * Convenience implementation of an exception handler that using standard JDK logging to log
 * the exception as {@link Level}.SEVERE and re-throw it wrapped in a {@link RuntimeException}
 */
class FatalExceptionHandler : public ExceptionHandler {

public:
//    private const static Logger LOGGER = Logger.getLogger(FatalExceptionHandler.class.getName());
//    private const Logger logger;

    FatalExceptionHandler() { }

    virtual void handle(const void * ex, const AbstractEntry& currentEntry) {
        //logger.log(Level.SEVERE, "Exception processing: " + currentEntry, ex);

    	std::cerr << "Exception processing " << currentEntry.toString() << std::endl;

//        throw new RuntimeException(ex);
    }
};

//class WaitStrategy;//fwd
//class WaitStrategyOption;//fwd
class ClaimStrategy; // fwd
class ClaimStrategyOption; //fwd

template <class T> class RingBuffer; // fwd

////////////////////////////////////////////////////////////////////
/// 			WaitStrategy

/**
 * Strategy employed for making {@link Consumer}s wait on a {@link RingBuffer}.
 */
class WaitStrategy {
    /**
     * Wait for the given sequence to be available for consumption in a {@link RingBuffer}
     *
     * @param consumers further back the chain that must advance first
     * @param ringBuffer on which to wait.
     * @param barrier the consumer is waiting on.
     * @param sequence to be waited on.
     * @return the sequence that is available which may be greater than the requested sequence.
     * @throws AlertException if the status of the Disruptor has changed.
     * @throws InterruptedException if the thread is interrupted.
     */
	template <typename T>
	long waitFor(std::vector<Consumer*> consumers, RingBuffer<T> ringBuffer,
    		ConsumerBarrier<T> barrier,  long sequence) {};
//        throws AlertException, InterruptedException;

    /**
     * Wait for the given sequence to be available for consumption in a {@link RingBuffer} with a timeout specified.
     *
     * @param consumers further back the chain that must advance first
     * @param ringBuffer on which to wait.
     * @param barrier the consumer is waiting on.
     * @param sequence to be waited on.
     * @param timeout value to abort after.
     * @param units of the timeout value.
     * @return the sequence that is available which may be greater than the requested sequence.
     * @throws AlertException if the status of the Disruptor has changed.
     * @throws InterruptedException if the thread is interrupted.
     */
	template <typename T>
    long waitFor(std::vector<Consumer*> consumers,
    		RingBuffer<T> ringBuffer, ConsumerBarrier<T> barrier, long sequence,
    		long timeout, boost::posix_time::time_duration units) {};
        //throws AlertException, InterruptedException;

    /**
     * Signal those waiting that the {@link RingBuffer} cursor has advanced.
     */
    virtual void signalAll();
}; // WaitStrategy


/**
 * Blocking strategy that uses a lock and condition variable for {@link Consumer}s waiting on a barrier.
 *
 * This strategy should be used when performance and low-latency are not as important as CPU resource.
 */
class BlockingStrategy : public  WaitStrategy {
private:

	const boost::shared_mutex _mutex;

//	const Lock lock = new ReentrantLock();
 //       const Condition consumerNotifyCondition = lock.newCondition();

public:

	template <typename T>
   	long waitFor(std::vector<Consumer*> consumers, RingBuffer<T> ringBuffer,
        		ConsumerBarrier<T> barrier,  long sequence) {

            long availableSequence;
            if ((availableSequence = ringBuffer.getCursor()) < sequence)
            {
            	boost::unique_lock< boost::shared_mutex > lock(lock);
            	while ((availableSequence = ringBuffer.getCursor()) < sequence)
            	{
            		if (barrier.isAlerted())
            		{
            			//throw ALERT_EXCEPTION;
            			std::cerr << "ALERT ... " << std::endl;
            		}
                        //consumerNotifyCondition.await();
                }
            }

            if (0 != consumers.size())
            {
                while ((availableSequence = getMinimumSequence(consumers)) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                    	std::cerr << "ALERT ... " << std::endl;
                     //  throw ALERT_EXCEPTION;
                    }
                }
            }

            return availableSequence;
        }

	template <typename T>
   long waitFor(std::vector<Consumer*> consumers,
        		RingBuffer<T> ringBuffer, ConsumerBarrier<T> barrier, long sequence,
        		long timeout, boost::posix_time::time_duration units) {
            long availableSequence;
            if ((availableSequence = ringBuffer.getCursor()) < sequence)
            {
            	boost::unique_lock< boost::shared_mutex > lock(lock);
                    while ((availableSequence = ringBuffer.getCursor()) < sequence)
                    {
                        if (barrier.isAlerted())
                        {
                           // throw ALERT_EXCEPTION;
                        	std::cerr << "ALERT ... " << std::endl;
                       }

                        if (!consumerNotifyCondition.await(timeout, units))
                        {
                            break;
                        }
                    }
            }

            if (0 != consumers.length)
            {
                while ((availableSequence = getMinimumSequence(consumers)) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                    	std::cerr << "ALERT ... " << std::endl;
//                  throw ALERT_EXCEPTION;
                    }
                }
            }

            return availableSequence;
        }

        void signalAll()
        {
        	boost::unique_lock< boost::shared_mutex > lock(lock);
//                consumerNotifyCondition.signalAll();
            }
        }
    }

    /**
     * Yielding strategy that uses a Thread.yield() for {@link Consumer}s waiting on a barrier.
     *
     * This strategy is a good compromise between performance and CPU resource.
     */
    const class YieldingStrategy : public WaitStrategy
    {
    public:
    	long waitFor(const Consumer[] consumers, const RingBuffer ringBuffer, const ConsumerBarrier barrier, const long sequence)
            throws AlertException, InterruptedException
        {
            long availableSequence;

            if (0 == consumers.length)
            {
                while ((availableSequence = ringBuffer.getCursor()) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }
                    Thread.yield();
                }
            }
            else
            {
                while ((availableSequence = getMinimumSequence(consumers)) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }
                    Thread.yield();
                }
            }

            return availableSequence;
        }

        long waitFor(const Consumer[] consumers, const RingBuffer ringBuffer, const ConsumerBarrier barrier,
                            const long sequence, const long timeout, const TimeUnit units)
            throws AlertException, InterruptedException
        {
            const long timeoutMs = units.convert(timeout, TimeUnit.MILLISECONDS);
            const long currentTime = System.currentTimeMillis();
            long availableSequence;

            if (0 == consumers.length)
            {
                while ((availableSequence = ringBuffer.getCursor()) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }

                    Thread.yield();
                    if (timeoutMs < (System.currentTimeMillis() - currentTime))
                    {
                        break;
                    }
                }
            }
            else
            {
                while ((availableSequence = getMinimumSequence(consumers)) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }

                    Thread.yield();
                    if (timeoutMs < (System.currentTimeMillis() - currentTime))
                    {
                        break;
                    }
                }
            }

            return availableSequence;
        }

        void signalAll()  { }
    }

    /**
     * Busy Spin strategy that uses a busy spin loop for {@link Consumer}s waiting on a barrier.
     *
     * This strategy will use CPU resource to avoid syscalls which can introduce latency jitter.  It is best
     * used when threads can be bound to specific CPU cores.
     */
    const class BusySpinStrategy : public WaitStrategy    {

        public long waitFor(const Consumer[] consumers, const RingBuffer ringBuffer, const ConsumerBarrier barrier, const long sequence)
            throws AlertException, InterruptedException
        {
            long availableSequence;

            if (0 == consumers.length)
            {
                while ((availableSequence = ringBuffer.getCursor()) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }
                }
            }
            else
            {
                while ((availableSequence = getMinimumSequence(consumers)) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }
                }
            }

            return availableSequence;
        }

        long waitFor(const Consumer[] consumers, const RingBuffer ringBuffer, const ConsumerBarrier barrier,
                            const long sequence, const long timeout, const TimeUnit units)
            throws AlertException, InterruptedException
        {
            const long timeoutMs = units.convert(timeout, TimeUnit.MILLISECONDS);
            const long currentTime = System.currentTimeMillis();
            long availableSequence;

            if (0 == consumers.length)
            {
                while ((availableSequence = ringBuffer.getCursor()) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }

                    if (timeoutMs < (System.currentTimeMillis() - currentTime))
                    {
                        break;
                    }
                }
            }
            else
            {
                while ((availableSequence = getMinimumSequence(consumers)) < sequence)
                {
                    if (barrier.isAlerted())
                    {
                        throw ALERT_EXCEPTION;
                    }

                    if (timeoutMs < (System.currentTimeMillis() - currentTime))
                    {
                        break;
                    }
                }
            }

            return availableSequence;
        }

        void signalAll(){ }
    }

/**
 * Strategy options which are available to those waiting on a {@link RingBuffer}
 */
struct WaitStrategyOption
{
/**
  * Used by the {@link com.lmax.disruptor.RingBuffer} as a polymorphic
  * constructor.
  *
  * @return a new instance of the WaitStrategy
  */
	virtual WaitStrategy* newInstance() = 0;
};

/** This strategy uses a condition variable inside a lock to block the
 * consumer which saves CPU resource as the expense of lock contention. */
struct BlockingWait : public WaitStrategyOption {
	virtual WaitStrategy* newInstance() { return new BlockingStrategy(); }
};

/** This strategy calls Thread.yield() in a loop as a waiting strategy which
 *  reduces contention at the expense of CPU resource. */
struct YieldingWait : public WaitStrategyOption {
	virtual WaitStrategy* newInstance() { return new YieldingStrategy(); }
};

/** This strategy call spins in a loop as a waiting strategy which is
 * lowest and most consistent latency but ties up a CPU */
struct BusyWait : public WaitStrategyOption {
	virtual WaitStrategy* newInstance() { return new BusySpinStrategy(); }
};



/// 			WaitStrategy
////////////////////////////////////////////////////////////////////


/**
 * Called by the {@link RingBuffer} to pre-populate all the {@link AbstractEntry}s to fill the RingBuffer.
 *
 * @param <T> AbstractEntry implementation storing the data for sharing during exchange or parallel coordination of an event.
 */
template <typename T>
class EntryFactory {
public:
    T create();
};

template <typename T>
class RingBuffer {
private:
	volatile long _cursor;
	const std::vector<AbstractEntry> _entries;
	const int _ringModMask;
	const ClaimStrategy* _claimStrategy;
	const ClaimStrategyOption* _claimStrategyOption;
	const WaitStrategy* _waitStrategy;

public:
	const static long INITIAL_CURSOR_VALUE = -1L;
	long p1, p2, p3, p4, p5, p6, p7; // cache line padding
	long p8, p9, p10, p11, p12, p13, p14; // cache line padding



/**
 * Construct a RingBuffer with the full option set.
 *
 * @param entryFactory to create {@link AbstractEntry}s for filling the RingBuffer
 * @param size of the RingBuffer that will be rounded up to the next power of 2
 * @param claimStrategyOption threading strategy for producers claiming {@link AbstractEntry}s in the ring.
 * @param waitStrategyOption waiting strategy employed by consumers waiting on {@link AbstractEntry}s becoming available.
 */
RingBuffer(const EntryFactory<T> entryFactory, const int size,
                  const ClaimStrategyOption* claimStrategyOption,
                  const WaitStrategyOption* waitStrategyOption)
	: _cursor(INITIAL_CURSOR_VALUE) ,
	  _ringModMask(ceilingNextPowerOfTwo(size)-1)
		_claimStrategy(claimStrategyOption->newInstance())
	  _claimStrategyOption(claimStrategyOption),
	  _waitStrategy(waitStrategyOption->newInstance())

{
    int sizeAsPowerOfTwo = ceilingNextPowerOfTwo(size);
    ringModMask = sizeAsPowerOfTwo - 1;
    entries = new AbstractEntry[sizeAsPowerOfTwo];

    _claimStrategy = claimStrategyOption.newInstance();
    _waitStrategy = waitStrategyOption.newInstance();

    fill(entryFactory);
}

/**
 * Construct a RingBuffer with default strategies of:
 * {@link ClaimStrategy.Option#MULTI_THREADED} and {@link WaitStrategy.Option#BLOCKING}
 *
 * @param entryFactory to create {@link AbstractEntry}s for filling the RingBuffer
 * @param size of the RingBuffer that will be rounded up to the next power of 2
 */
RingBuffer(const EntryFactory<T> entryFactory, const int size)
{
    this(entryFactory, size,
         MultiThreadedStrategy,
         WaitStrategy.Option.BLOCKING);
}

/**
 * Create a {@link ConsumerBarrier} that gates on the RingBuffer and a list of
 * {@link Consumer}s
 *
 * @param consumersToTrack this barrier will track
 * @return the barrier gated as required
 */
ConsumerBarrier<T>
createConsumerBarrier(const std::vector<Consumer> consumersToTrack) {
    return new ConsumerTrackingConsumerBarrier(consumersToTrack);
}

/**
 * Create a {@link ProducerBarrier} on this RingBuffer that tracks dependent
 *  {@link Consumer}s.
 *
 * @param consumersToTrack to be tracked to prevent wrapping.
 * @return a {@link ProducerBarrier} with the above configuration.
 */
 ProducerBarrier<T> createProducerBarrier(const std::vector<Consumer> consumersToTrack)
{
    return new ConsumerTrackingProducerBarrier(consumersToTrack);
}

/**
 * Create a {@link ForceFillProducerBarrier} on this RingBuffer that tracks
 * dependent {@link Consumer}s.  This barrier is to be used for filling a
 * RingBuffer when no other producers exist.
 *
 * @param consumersToTrack to be tracked to prevent wrapping.
 * @return a {@link ForceFillProducerBarrier} with the above configuration.
 */
ForceFillProducerBarrier<T> createForceFillProducerBarrier(const std::vector<Consumer> consumersToTrack)
{
    return new ForceFillConsumerTrackingProducerBarrier(consumersToTrack);
}

/**
 * The capacity of the RingBuffer to hold entries.
 *
 * @return the size of the RingBuffer.
 */
int getCapacity() { return entries.length; }

/**
 * Get the current sequence that producers have committed to the RingBuffer.
 *
 * @return the current committed sequence.
 */
long getCursor() { return cursor; }

/**
 * Get the {@link AbstractEntry} for a given sequence in the RingBuffer.
 *
 * @param sequence for the {@link AbstractEntry}
 * @return {@link AbstractEntry} for the sequence
 */
T getEntry(const long sequence) {
    return (T)entries[(int)sequence & ringModMask];
}

private:

void fill(const EntryFactory<T> entryFactory) {
    for (int i = 0; i < entries.length; i++) {
        entries[i] = entryFactory.create();
    }
};

/**
 * ConsumerBarrier handed out for gating consumers of the RingBuffer and
 * dependent {@link Consumer}(s)
 */
class ConsumerTrackingConsumerBarrier public: ConsumerBarrier<T>
{
private:
    volatile boolean alerted = false;
    private const Consumer[] consumers;
public:

    ConsumerTrackingConsumerBarrier(const Consumer... consumers) {
    	this.consumers = consumers;
    }

    T getEntry(const long sequence) {
        return (T)entries[(int)sequence & ringModMask];
    }

    long waitFor(const long sequence)
    //    throws AlertException, InterruptedException
    {
        return waitStrategy.waitFor(consumers, RingBuffer.this, this, sequence);
    }

    long waitFor(const long sequence, const long timeout, const TimeUnit units)
      //  throws AlertException, InterruptedException
    {
        return waitStrategy.waitFor(consumers, RingBuffer.this, this, sequence, timeout, units);
    }

    public long getCursor()
    {
        return cursor;
    }

    public boolean isAlerted()
    {
        return alerted;
    }

    public void alert()
    {
        alerted = true;
        waitStrategy.signalAll();
    }

    public void clearAlert()
    {
        alerted = false;
    }
}

/**
 * {@link ProducerBarrier} that tracks multiple {@link Consumer}s when trying to claim
 * an {@link AbstractEntry} in the {@link RingBuffer}.
 */
private const class ConsumerTrackingProducerBarrier implements ProducerBarrier<T>
{
    private const Consumer[] consumers;
    private long lastConsumerMinimum = RingBuffer.INITIAL_CURSOR_VALUE;

    public ConsumerTrackingProducerBarrier(const Consumer... consumers)
    {
        if (0 == consumers.length)
        {
            throw new IllegalArgumentException("There must be at least one Consumer to track for preventing ring wrap");
        }
        this.consumers = consumers;
    }

    public T nextEntry()
    {
        const long sequence = claimStrategy.incrementAndGet();
        ensureConsumersAreInRange(sequence);

        AbstractEntry entry = entries[(int)sequence & ringModMask];
        entry.setSequence(sequence);

        return (T)entry;
    }

    public void commit(const T entry)
    {
        commit(entry.getSequence(), 1);
    }

    public SequenceBatch nextEntries(const SequenceBatch sequenceBatch)
    {
        const long sequence = claimStrategy.incrementAndGet(sequenceBatch.getSize());
        sequenceBatch.setEnd(sequence);
        ensureConsumersAreInRange(sequence);

        for (long i = sequenceBatch.getStart(), end = sequenceBatch.getEnd(); i <= end; i++)
        {
            AbstractEntry entry = entries[(int)i & ringModMask];
            entry.setSequence(i);
        }

        return sequenceBatch;
    }

    public void commit(const SequenceBatch sequenceBatch)
    {
        commit(sequenceBatch.getEnd(), sequenceBatch.getSize());
    }

    public T getEntry(const long sequence)
    {
        return (T)entries[(int)sequence & ringModMask];
    }

    public long getCursor()
    {
        return cursor;
    }

    private void ensureConsumersAreInRange(const long sequence)
    {
        const long wrapPoint = sequence - entries.length;
        while (wrapPoint > lastConsumerMinimum &&
               wrapPoint > (lastConsumerMinimum = getMinimumSequence(consumers)))
        {
            Thread.yield();
        }
    }

    private void commit(const long sequence, const long batchSize)
    {
        if (ClaimStrategy.Option.MULTI_THREADED == claimStrategyOption)
        {
            const long expectedSequence = sequence - batchSize;
            while (expectedSequence != cursor)
            {
                // busy spin
            }
        }

        cursor = sequence;
        waitStrategy.signalAll();
    }
}

/**
 * {@link ForceFillProducerBarrier} that tracks multiple {@link Consumer}s when trying to claim
 * a {@link AbstractEntry} in the {@link RingBuffer}.
 */
private const class ForceFillConsumerTrackingProducerBarrier implements ForceFillProducerBarrier<T>
{
    private const Consumer[] consumers;
    private long lastConsumerMinimum = RingBuffer.INITIAL_CURSOR_VALUE;

    public ForceFillConsumerTrackingProducerBarrier(const Consumer... consumers)
    {
        if (0 == consumers.length)
        {
            throw new IllegalArgumentException("There must be at least one Consumer to track for preventing ring wrap");
        }
        this.consumers = consumers;
    }

    public T claimEntry(const long sequence)
    {
        ensureConsumersAreInRange(sequence);

        AbstractEntry entry = entries[(int)sequence & ringModMask];
        entry.setSequence(sequence);

        return (T)entry;
    }

    public void commit(const T entry)
    {
        long sequence = entry.getSequence();
        claimStrategy.setSequence(sequence);
        cursor = sequence;
        waitStrategy.signalAll();
    }

    public long getCursor()
    {
        return cursor;
    }

    private void ensureConsumersAreInRange(const long sequence)
    {
        const long wrapPoint = sequence - entries.length;
        while (wrapPoint > lastConsumerMinimum &&
               wrapPoint > (lastConsumerMinimum = getMinimumSequence(consumers)))
        {
            Thread.yield();
        }
    }
}

};

/**
 * Strategies employed for claiming the sequence of {@link AbstractEntry}s in
 * the {@link RingBuffer} by producers.
 *
 * The {@link AbstractEntry} index is a the sequence value mod the
 * {@link RingBuffer} capacity.
 */
class ClaimStrategy  {
public:

	/**
     * Claim the next sequence index in the {@link RingBuffer} and increment.
     *
     * @return the {@link AbstractEntry} index to be used for the producer.
     */
    virtual long incrementAndGet() = 0;

    /**
     * Increment by a delta and get the result.
     *
     * @param delta to increment by.
     * @return the result after incrementing.
     */
    virtual long incrementAndGet(int delta) = 0;

    /**
     * Set the current sequence value for claiming {@link AbstractEntry} in
     * the {@link RingBuffer}
     *
     * @param sequence to be set as the current value.
     */
    virtual void setSequence(long sequence) = 0;


}; // ClaimStrategy

/**
 * Strategy to be used when there are multiple producer threads
 * claiming {@link AbstractEntry}s.
 */
class MultiThreadedStrategy : public ClaimStrategy  {
private:

    tbb::atomic<long> sequence;

public:

    MultiThreadedStrategy() {
    	sequence =  RingBuffer::INITIAL_CURSOR_VALUE;
    } ;

    virtual long incrementAndGet() { return ++sequence; }

    virtual long incrementAndGet(const int delta) {
        sequence+=delta;
        return sequence;
    }

    virtual void setSequence(const long seq) { sequence = seq; }

}; // MultiThreadedStrategy

/**
 * Optimised strategy can be used when there is a single producer thread
 *  claiming {@link AbstractEntry}s.
 */
class SingleThreadedStrategy : public ClaimStrategy {
private:

	long sequence;

public:
	SingleThreadedStrategy() : sequence(RingBuffer::INITIAL_CURSOR_VALUE) {}

	virtual long incrementAndGet() { return ++sequence; }

	virtual long incrementAndGet(const int delta) {
        sequence += delta;
        return sequence;
    }

    virtual void setSequence(const long seq) { sequence = seq; }
}; // SingleThreadedStrategy

/**
 * Indicates the threading policy to be applied for claiming {@link
 * AbstractEntry}s by producers to the {@link RingBuffer}
 */
struct ClaimStrategyOption {
    /**
     * Used by the {@link RingBuffer} as a polymorphic constructor.
     *
     * @return a new instance of the ClaimStrategy
     */
    virtual ClaimStrategy* newInstance() = 0;
}; // Option

/** Makes the {@link RingBuffer} thread safe for claiming
 * {@link AbstractEntry}s by multiple producing threads. */
struct MultiThreaded : public ClaimStrategyOption {
	virtual ClaimStrategy* newInstance() {
		return new MultiThreadedStrategy();
     }
}; // MultiThreaded

/** Optimised {@link RingBuffer} for use by single thread
 * claiming {@link AbstractEntry}s as a producer. */
struct SingleThreaded : public ClaimStrategyOption {
	virtual ClaimStrategy* newInstance() {
        return new SingleThreadedStrategy();
     }
}; // SingleThreaded




};
}; // namespace disruptor



#endif /* DISRUPTOR_HPP_ */
