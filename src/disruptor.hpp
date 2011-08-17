/*
 * disruptor.hpp
 *
 *  Created on: Aug 8, 2011
 *      Author: tingarg
 */

#ifndef DISRUPTOR_HPP_
#define DISRUPTOR_HPP_

#include <climits>
//#include <assert.h>

#include <tbb/atomic.h>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace disruptor {

const static long INITIAL_CURSOR_VALUE = -1L;

/**
 *             ---- Hacked straight out of Java library. -----
 *
 * Returns the number of zero bits preceding the highest-order
 * ("leftmost") one-bit in the two's complement binary representation
 * of the specified <tt>int</tt> value.  Returns 32 if the
 * specified value has no one-bits in its two's complement representation,
 * in other words if it is equal to zero.
 *
 * <p>Note that this method is closely related to the logarithm base 2.
 * For all positive <tt>int</tt> values x:
 * <ul>
 * <li>floor(log<sub>2</sub>(x)) = <tt>31 - numberOfLeadingZeros(x)</tt>
 * <li>ceil(log<sub>2</sub>(x)) = <tt>32 - numberOfLeadingZeros(x - 1)</tt>
 * </ul>
 *
 * @return the number of zero bits preceding the highest-order
 *     ("leftmost") one-bit in the two's complement binary representation
 *     of the specified <tt>int</tt> value, or 32 if the value
 *     is equal to zero.
 * @since 1.5
 */
inline int numberOfLeadingZeros(int i) {
    // HD, Figure 5-6
    if (i == 0) return 32;
    int n = 1;
    if (i >> 16 == 0) { n += 16; i <<= 16; }
    if (i >> 24 == 0) { n +=  8; i <<=  8; }
    if (i >> 28 == 0) { n +=  4; i <<=  4; }
    if (i >> 30 == 0) { n +=  2; i <<=  2; }
    n -= i >> 31;
    return n;
}

/**
  * Calculate the next power of 2, greater than or equal to x.<p>
  * From Hacker's Delight, Chapter 3, Harry S. Warren Jr.
  *
  * @param x Value to round up
  * @return The next power of 2 from x inclusive
  */
inline int ceilingNextPowerOfTwo(const int x) {
     return 1 << (32 - numberOfLeadingZeros(x - 1));
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


inline std::ostream& operator<<(std::ostream& os, AbstractEntry& entry) {
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
 * Get the minimum sequence from an array of {@link Consumer}s.
 *
 * @param consumers to compare.
 * @return the minimum sequence found or Long.MAX_VALUE if the array is empty.
 */
inline long getMinimumSequence(const std::vector<Consumer*> consumers) {
    long minimum = LONG_MAX;

    for (int i = 0; i < consumers.size(); i++) {
   	 Consumer* consumer = consumers.at(i);
        long sequence = consumer->getSequence();
        minimum = minimum < sequence ? minimum : sequence;
    }

    return minimum;
}

/**
 * Used to record the batch of sequences claimed in a {@link RingBuffer}.
 */
class SequenceBatch {
private:
	const int _size;
	long _end;

public:
    /**
     * Create a holder for tracking a batch of claimed sequences
     * in a {@link RingBuffer}
     * @param size of the batch to claim.
     */
    SequenceBatch(const int size) : _size(size), _end(INITIAL_CURSOR_VALUE)
    { }

    /**
     * Get the end sequence of a batch.
     *
     * @return the end sequence in a batch
     */
    long getEnd() { return _end; }

    /**
     * Set the end of the batch sequence.  To be used by
     * the {@link ProducerBarrier}.
     *
     * @param end sequence in the batch.
     */
    void setEnd(const long end) { this->_end = end; }

    /**
     * Get the size of the batch.
     *
     * @return the size of the batch.
     */
    int getSize() { return _size; }

    /**
     * Get the starting sequence for a batch.
     *
     * @return the starting sequence of a batch.
     */
    long getStart() { return _end - (_size - 1L); }
};


/**
 * Abstraction for claiming {@link AbstractEntry}s in a {@link RingBuffer}
 * while tracking dependent {@link Consumer}s
 *
 * @param <T> {@link AbstractEntry} implementation stored in
 * the {@link RingBuffer}
 */
template <typename T>
class ProducerBarrier {
public:
	/**
     * Claim the next {@link AbstractEntry} in sequence for a producer
     * on the {@link RingBuffer}
     *
     * @return the claimed {@link AbstractEntry}
     */
	virtual T nextEntry() = 0;

    /**
     * Claim the next batch of {@link AbstractEntry}s in sequence.
     *
     * @param sequenceBatch to be updated for the batch range.
     * @return the updated sequenceBatch.
     */
    virtual SequenceBatch nextEntries(SequenceBatch sequenceBatch)= 0;

    /**
     * Commit an entry back to the {@link RingBuffer} to make it visible
     * to {@link Consumer}s
     * @param entry to be committed back to the {@link RingBuffer}
     */
    virtual void commit(T entry) = 0;

    /**
     * Commit the batch of entries back to the {@link RingBuffer}.
     *
     * @param sequenceBatch to be committed.
     */
    virtual void commit(SequenceBatch sequenceBatch) = 0;

    /**
     * Get the {@link AbstractEntry} for a given sequence from the
     * underlying {@link RingBuffer}.
     *
     * @param sequence of the {@link AbstractEntry} to get.
     * @return the {@link AbstractEntry} for the sequence.
     */
    virtual T getEntry(long sequence) = 0;

    /**
     * Delegate a call to the {@link RingBuffer#getCursor()}
     *
     * @return value of the cursor for entries that have been published.
     */
    virtual long getCursor() = 0;
};

/**
 * Coordination barrier for tracking the cursor for producers and sequence of
 * dependent {@link Consumer}s for a {@link RingBuffer}
 *
 * @param <T> {@link AbstractEntry} implementation stored in
 * the {@link RingBuffer}
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
     * @throws AlertException if a status change has occurred for
     *  the Disruptor
     * @throws InterruptedException if the thread needs awaking on
     * a condition variable.
     */
    virtual long waitFor(long sequence) ;
    //throws AlertException, InterruptedException;

    /**
     * Wait for the given sequence to be available for consumption
     * with a time out.
     *
     * @param sequence to wait for
     * @param timeout value
     * @param units for the timeout value
     * @return the sequence up to which is available
     * @throws AlertException if a status change has occurred
     * for the Disruptor
     * @throws InterruptedException if the thread needs awaking
     *  on a condition variable.
     */
    virtual long waitFor(long sequence,
    		boost::posix_time::time_duration timeout) ;
    //throws AlertException, InterruptedException;

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
     * Alert the consumers of a status change and stay in this status
     *  until cleared.
     */
    virtual void alert();

    /**
     * Clear the current alert status.
     */
    virtual void clearAlert();

};
/**
 * Abstraction for claiming {@link AbstractEntry}s in a {@link RingBuffer} while tracking dependent {@link Consumer}s.
 *
 * This barrier can be used to pre-fill a {@link RingBuffer} but only when no other producers are active.
 *
 * @param <T> {@link AbstractEntry} implementation stored in the {@link RingBuffer}
 */
template <typename T>
class ForceFillProducerBarrier {
public:
    /**
     * Claim a specific sequence in the {@link RingBuffer} when only one producer is involved.
     *
     * @param sequence to be claimed.
     * @return the claimed {@link AbstractEntry}
     */
    T claimEntry(long sequence);

    /**
     * Commit an entry back to the {@link RingBuffer} to make it visible to {@link Consumer}s.
     * Only use this method when forcing a sequence and you are sure only one producer exists.
     * This will cause the {@link RingBuffer} to advance the {@link RingBuffer#getCursor()} to this sequence.
     *
     * @param entry to be committed back to the {@link RingBuffer}
     */
    void commit(T entry);

    /**
     * Delegate a call to the {@link RingBuffer#getCursor()}
     *
     * @return value of the cursor for entries that have been published.
     */
    long getCursor();
};// ForceFillProducerBarrier

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
 * Used by the {@link BatchConsumer} to set a callback allowing the {@link BatchHandler} to notify
 * when it has finished consuming an {@link AbstractEntry} if this happens after the {@link BatchHandler#onAvailable(AbstractEntry)} call.
 * <p>
 * Typically this would be used when the handler is performing some sort of batching operation such are writing to an IO device.
 * </p>
 * @param <T> AbstractEntry implementation storing the data for sharing during exchange or parallel coordination of an event.
 */
template <typename T>
class SequenceTrackingHandler : public BatchHandler<T>
{
    /**
     * Call by the {@link BatchConsumer} to setup the callback.
     *
     * @param sequenceTrackerCallback callback on which to notify the {@link BatchConsumer} that the sequence has progressed.
     */
    void setSequenceTrackerCallback(const BatchConsumer.SequenceTrackerCallback sequenceTrackerCallback);
};




/**
 * Callback handler for uncaught exceptions in the {@link AbstractEntry}
 * processing cycle of the {@link BatchConsumer}
 */
class ExceptionHandler {
public:

	/**
     * Strategy for handling uncaught exceptions when processing
     * an {@link AbstractEntry}.
     *
     * If the strategy wishes to suspend further processing by the
     * {@link BatchConsumer}
     * then is should throw a {@link RuntimeException}.
     *
     * @param ex the exception that propagated from the {@link BatchHandler}
     * @param currentEntry being processed when the exception occurred.
     */
	virtual void handle(const void * ex, const AbstractEntry& currentEntry) = 0;
	//    void handle(Exception ex, AbstractEntry currentEntry);

};

/**
 * Convenience implementation of an exception handler that using standard
 *  JDK logging to log
 * the exception as {@link Level}.SEVERE and re-throw it wrapped in a
 * {@link RuntimeException}
 */
class FatalExceptionHandler : public ExceptionHandler {

public:
//    private const static Logger LOGGER = Logger.getLogger
	//(FatalExceptionHandler.class.getName());
//    private const Logger logger;

    FatalExceptionHandler() { }

    virtual void handle(const void * ex, const AbstractEntry& currentEntry) {
        //logger.log(Level.SEVERE, "Exception processing: " + currentEntry, ex);

    	std::cerr << "Exception processing "
    			<< currentEntry.toString() << std::endl;

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
     * Wait for the given sequence to be available for consumption
     * in a {@link RingBuffer}
     *
     * @param consumers further back the chain that must advance first
     * @param ringBuffer on which to wait.
     * @param barrier the consumer is waiting on.
     * @param sequence to be waited on.
     * @return the sequence that is available which may be greater than
     * the requested sequence.
     * @throws AlertException if the status of the Disruptor has changed.
     * @throws InterruptedException if the thread is interrupted.
     */
	template <typename T>
	long waitFor(std::vector<Consumer*> consumers, RingBuffer<T> ringBuffer,
    		ConsumerBarrier<T> barrier,  long sequence) {};
//        throws AlertException, InterruptedException;

    /**
     * Wait for the given sequence to be available for consumption in a
     *  {@link RingBuffer} with a timeout specified.
     *
     * @param consumers further back the chain that must advance first
     * @param ringBuffer on which to wait.
     * @param barrier the consumer is waiting on.
     * @param sequence to be waited on.
     * @param timeout value to abort after.
     * @return the sequence that is available which may be greater than the
     *  requested sequence.
     * @throws AlertException if the status of the Disruptor has changed.
     * @throws InterruptedException if the thread is interrupted.
     */
	template <typename T>
    long waitFor(std::vector<Consumer*> consumers,
    		RingBuffer<T> ringBuffer, ConsumerBarrier<T> barrier, long sequence,
    		boost::posix_time::time_duration timeout) {};
        //throws AlertException, InterruptedException;

    /**
     * Signal those waiting that the {@link RingBuffer} cursor has advanced.
     */
    virtual void signalAll();
}; // WaitStrategy


/**
 * Blocking strategy that uses a lock and condition variable for
 * {@link Consumer}s waiting on a barrier.
 *
 * This strategy should be used when performance and low-latency are not
 *  as important as CPU resource.
 */
class BlockingStrategy : public WaitStrategy {
private:

	const boost::shared_mutex _mutex;

	//	const Lock lock = new ReentrantLock();
	//       const Condition consumerNotifyCondition = lock.newCondition();

public:

	template <typename T>
	long waitFor(std::vector<Consumer*> consumers, RingBuffer<T> ringBuffer,
			ConsumerBarrier<T> barrier, long sequence) {

		long availableSequence;
		if ((availableSequence = ringBuffer.getCursor()) < sequence)
		{
			boost::unique_lock< boost::shared_mutex > lock(_mutex);
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
			while ((availableSequence =
					getMinimumSequence(consumers)) < sequence)
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
			boost::posix_time::time_duration timeout) {
		long availableSequence;
		if ((availableSequence = ringBuffer.getCursor()) < sequence)
		{
			boost::unique_lock< boost::shared_mutex > lock(_mutex);
			while ((availableSequence = ringBuffer.getCursor()) < sequence)
			{
				if (barrier.isAlerted())
				{
					// throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}

				//                        if (!consumerNotifyCondition.await(timeout, units))
				//                        {
				//                            break;
				//                        }
			}
		}

		if (0 != consumers.size())
		{
			while ((availableSequence =
					getMinimumSequence(consumers)) < sequence)
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
		//boost::unique_lock< boost::shared_mutex > lock(lock);
		//                consumerNotifyCondition.signalAll();

	}
}; // BlockingStrategy

/**
 * Yielding strategy that uses a Thread.yield() for {@link Consumer}s waiting
 * on a barrier.
 *
 * This strategy is a good compromise between performance and CPU resource.
 */
class YieldingStrategy: public WaitStrategy {
public:

	template<typename T>
	long waitFor(std::vector<Consumer*> consumers, RingBuffer<T> ringBuffer,
			ConsumerBarrier<T> barrier, long sequence) {
		long availableSequence;

		if (0 == consumers.size()) {
			while ((availableSequence = ringBuffer.getCursor()) < sequence) {
				if (barrier.isAlerted()) {
					//throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}
				//Thread.yield();
				boost::this_thread::yield();
			}
		} else {
			while ((availableSequence = getMinimumSequence(consumers))
					< sequence) {
				if (barrier.isAlerted()) {
					//throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}
				boost::this_thread::yield();
				//				Thread.yield();
			}
		}

		return availableSequence;
	}

	template<typename T>
	long waitFor(std::vector<Consumer*> consumers, RingBuffer<T> ringBuffer,
			ConsumerBarrier<T> barrier, long sequence,
			boost::posix_time::time_duration timeout) {

		const boost::posix_time::ptime currentTime();//System.currentTimeMillis();
		long availableSequence;

		if (0 == consumers.size()) {
			while ((availableSequence = ringBuffer.getCursor()) < sequence) {
				if (barrier.isAlerted()) {
					//throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;

				}

				boost::this_thread::yield();
				//				Thread.yield();
				boost::posix_time::ptime now();
				boost::posix_time::time_period per(currentTime, now);
				if (timeout < per.length()) {
					break;
				}
			}
		} else {
			while ((availableSequence = getMinimumSequence(consumers))
					< sequence) {
				if (barrier.isAlerted()) {
					//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}
				boost::this_thread::yield();
				//			Thread.yield();
				boost::posix_time::ptime now();
				boost::posix_time::time_period per(currentTime, now);
				if (timeout < per.length()) {
					break;
				}
			}
		}

		return availableSequence;
	}

	void signalAll() {	}

}; // YieldingStrategy

    /**
 * Busy Spin strategy that uses a busy spin loop for {@link Consumer}s
 * waiting on a barrier.
 *
 * This strategy will use CPU resource to avoid syscalls which can introduce
 *  latency jitter.  It is best
 * used when threads can be bound to specific CPU cores.
 */
class BusySpinStrategy: public WaitStrategy {

public:

	template <typename T>
	long waitFor(std::vector<Consumer*> consumers, RingBuffer<T> ringBuffer,
			ConsumerBarrier<T> barrier, long sequence) {
		long availableSequence;

		if (0 == consumers.size())
		{
			while ((availableSequence = ringBuffer.getCursor()) < sequence)
			{
				if (barrier.isAlerted())
				{
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;

				}
			}
		}
		else
		{
			while ((availableSequence =
					getMinimumSequence(consumers)) < sequence)
			{
				if (barrier.isAlerted())
				{
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}
			}
		}

		return availableSequence;
	}

	template <typename T>
	long waitFor(std::vector<Consumer*> consumers,
			RingBuffer<T> ringBuffer, ConsumerBarrier<T> barrier, long sequence,
			boost::posix_time::time_duration timeout) {
		const boost::posix_time::ptime currentTime();//System.currentTimeMillis();
		long availableSequence;

		if (0 == consumers.size())
		{
			while ((availableSequence = ringBuffer.getCursor()) < sequence)
			{
				if (barrier.isAlerted())
				{
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;

				}

				boost::posix_time::ptime now();
				boost::posix_time::time_period per(currentTime, now);
				if (timeout < per.length()) {
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
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}

				boost::posix_time::ptime now();
				boost::posix_time::time_period per(currentTime, now);
				if (timeout < per.length()) {
					break;
				}
			}
		}

		return availableSequence;
	}

	void signalAll() {	}
}; // BusySpinStrategy

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

/**
 * Called by the {@link RingBuffer} to pre-populate all the {@link AbstractEntry}s to fill the RingBuffer.
 *
 * @param <T> AbstractEntry implementation storing the data for sharing during exchange or parallel coordination of an event.
 */
template <typename T>
class EntryFactory {
public:
    T* create();
};


/**
 * ConsumerBarrier handed out for gating consumers of the RingBuffer and
 * dependent {@link Consumer}(s)
 */
template<typename T>
class ConsumerTrackingConsumerBarrier: public ConsumerBarrier<T> {
private:
	tbb::atomic<bool> _alerted;
	const RingBuffer<T>& _ring;
  	const std::vector<Consumer*> _consumers;
public:

	ConsumerTrackingConsumerBarrier(const std::vector<Consumer*> consumers,
			const RingBuffer<T>& ring)
	: _consumers(consumers), _ring(ring) {
		_alerted = false;
	}

	T getEntry(const long sequence) {
		return _ring._entries[(int) sequence & _ring._ringModMask];
	}

	long waitFor(const long sequence)
	//    throws AlertException, InterruptedException
	{
		return	_ring._waitStrategy.waitFor
				(_consumers, _ring, this, _ring._sequence);
	}

	long waitFor(const long sequence, const boost::posix_time::time_duration timeout)
	//  throws AlertException, InterruptedException
	{
		return _ring._waitStrategy.waitFor
				(_consumers, _ring, this, sequence, timeout);
	}

	long getCursor() { return _ring._cursor; }

	bool isAlerted() {return _alerted;}

	void alert() {
		_alerted = true;
		_ring._waitStrategy.signalAll();
	}

	void clearAlert() {_alerted = false;}
}; // ConsumerTrackingConsumerBarrier

/**
 * {@link ProducerBarrier} that tracks multiple {@link Consumer}s when trying
 *  to claim
 * an {@link AbstractEntry} in the {@link RingBuffer}.
 */
template <typename T>
class ConsumerTrackingProducerBarrier : public ProducerBarrier<T> {
private :
	const std::vector<Consumer*> _consumers;
	long _lastConsumerMinimum;
	const RingBuffer<T>& _ring;

public:

	ConsumerTrackingProducerBarrier(const std::vector<Consumer*> consumers,
			const RingBuffer<T>& ring)
	: _consumers(consumers), _lastConsumerMinimum(INITIAL_CURSOR_VALUE),
	  _ring(ring) {

		if (0 == _consumers.size())
		{
			//throw new IllegalArgumentException("There must be at least one Consumer to track for preventing ring wrap");
			std::cerr << "There must be at least one Consumer to track for "
					"preventing ring wrap" << std::endl;
		}
	}

	T nextEntry() {
		const long sequence = _ring._claimStrategy.incrementAndGet();
		ensureConsumersAreInRange(sequence);

		AbstractEntry entry = _ring.entries[(int)sequence & _ring.ringModMask];
		entry.setSequence(sequence);

		return (T)entry;
	}

	void commit(const T entry)	{ commit(entry.getSequence(), 1); }

	SequenceBatch nextEntries(const SequenceBatch sequenceBatch) {
		const long sequence = _ring._claimStrategy
				.incrementAndGet(sequenceBatch.getSize());
		sequenceBatch.setEnd(sequence);
		ensureConsumersAreInRange(sequence);

		for (long i = sequenceBatch.getStart(), _end = sequenceBatch.getEnd(); i <= _end; i++)
		{
			AbstractEntry entry = _ring._entries[(int)i & _ring._ringModMask];
			entry.setSequence(i);
		}

		return sequenceBatch;
	}

	void commit(const SequenceBatch sequenceBatch)
	{
		commit(sequenceBatch.getEnd(), sequenceBatch.getSize());
	}

	T getEntry(const long sequence)	{
		return _ring._entries[(int) sequence & _ring._ringModMask];
	}

	long getCursor() { return _ring._cursor;	}

	void ensureConsumersAreInRange(const long sequence)
	{
		const long wrapPoint = sequence - _ring._entries.length;
		while (wrapPoint > _lastConsumerMinimum &&
				wrapPoint > (_lastConsumerMinimum = getMinimumSequence(_consumers)))
		{
			boost::thread::yield();
		}
	}

private:

	void commit(const long sequence, const long batchSize)
	{
//		if (ClaimStrategy.Option.MULTI_THREADED == claimStrategyOption )
//		{
//			const long expectedSequence = sequence - batchSize;
//			while (expectedSequence != _ring._cursor)
//			{
//				// busy spin
//			}
//		}
//
//		_ring._cursor = sequence;
//		_ring._waitStrategy.signalAll();
	}
};

/**
 * {@link ForceFillProducerBarrier} that tracks multiple {@link Consumer}s when trying to claim
 * a {@link AbstractEntry} in the {@link RingBuffer}.
 */
template <typename T>
class ForceFillConsumerTrackingProducerBarrier
	: public ForceFillProducerBarrier<T>
{
private:
		const std::vector<Consumer*> _consumers;
		long _lastConsumerMinimum;
		const RingBuffer<T>& _ring;

public:
		ForceFillConsumerTrackingProducerBarrier
			(const std::vector<Consumer*> consumers, const RingBuffer<T>& ring)
		: _consumers(consumers), _lastConsumerMinimum(INITIAL_CURSOR_VALUE),
		  _ring(ring) {
		if (0 == _consumers.size())
		{
//			throw new IllegalArgumentException("There must be at least one Consumer to track for preventing ring wrap");
			std::cerr << "There must be at least one Consumer to track for "
					"preventing ring wrap" << std::endl;

		}
	}

	T claimEntry(const long sequence)	{
		ensureConsumersAreInRange(sequence);

		T entry = _ring._entries[(int)sequence & _ring._ringModMask];
		entry.setSequence(sequence);

		return entry;
	}

	void commit(const T entry) {
		long sequence = entry.getSequence();
		_ring._claimStrategy.setSequence(sequence);
		_ring._cursor = sequence;
		_ring._waitStrategy.signalAll();
	}

	long getCursor() { return _ring.cursor;	}

private :

	void ensureConsumersAreInRange(const long sequence)	{
		const long wrapPoint = sequence - _ring._entries.size();
		while (wrapPoint > _lastConsumerMinimum &&
				wrapPoint > (_lastConsumerMinimum = getMinimumSequence(_consumers)))
		{
			boost::this_thread::yield();
//			Thread.yield();
		}
	}
}; // ForceFillConsumerTracingProducerBarrier


template <typename T>
class RingBuffer {
private:
	volatile long _cursor;
	const int _ringModMask;
	const std::vector<AbstractEntry> _entries;
	const ClaimStrategy* _claimStrategy;
	const ClaimStrategyOption* _claimStrategyOption;
	const WaitStrategy* _waitStrategy;

	friend class ConsumerTrackingConsumerBarrier<T>;
	friend class ConsumerTrackingProducerBarrier<T>;

public:
//	const static long INITIAL_CURSOR_VALUE = -1L;
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
		  _ringModMask(ceilingNextPowerOfTwo(size)-1),
		  _entries(_ringModMask+1),
		  _claimStrategy(claimStrategyOption->newInstance()),
		  _claimStrategyOption(claimStrategyOption),
		  _waitStrategy(waitStrategyOption->newInstance())
	{

		fill(entryFactory);
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
		return new ConsumerTrackingConsumerBarrier<T>(consumersToTrack, this);
	}

	/**
	 * Create a {@link ProducerBarrier} on this RingBuffer that tracks dependent
	 *  {@link Consumer}s.
	 *
	 * @param consumersToTrack to be tracked to prevent wrapping.
	 * @return a {@link ProducerBarrier} with the above configuration.
	 */
	 ProducerBarrier<T> createProducerBarrier
	 	 (const std::vector<Consumer> consumersToTrack)
	 {
		return new ConsumerTrackingProducerBarrier<T>(consumersToTrack, this);
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
		return new ForceFillConsumerTrackingProducerBarrier<T>(consumersToTrack);
	}

	/**
	 * The capacity of the RingBuffer to hold entries.
	 *
	 * @return the size of the RingBuffer.
	 */
	int getCapacity() { return _entries.size(); }

	/**
	 * Get the current sequence that producers have committed to the RingBuffer.
	 *
	 * @return the current committed sequence.
	 */
	long getCursor() { return _cursor; }

	/**
	 * Get the {@link AbstractEntry} for a given sequence in the RingBuffer.
	 *
	 * @param sequence for the {@link AbstractEntry}
	 * @return {@link AbstractEntry} for the sequence
	 */
	T getEntry(const long sequence) {
		return _entries[(int) sequence & _ringModMask];
	}

private:

	void fill(const EntryFactory<T> entryFactory) {
		for (int i = 0; i < _entries.size(); i++) {
			_entries[i] = entryFactory.create();
		}
	};

};

/**
 * Strategy to be used when there are multiple producer threads
 * claiming {@link AbstractEntry}s.
 */
class MultiThreadedStrategy : public ClaimStrategy  {
private:

    tbb::atomic<long> _sequence;

public:

    MultiThreadedStrategy() {
    	_sequence = INITIAL_CURSOR_VALUE ;
    }

    virtual long incrementAndGet() { return ++_sequence; }

    virtual long incrementAndGet(const int delta) {
        _sequence+=delta;
        return _sequence;
    }

    virtual void setSequence(const long seq) { _sequence = seq; }

}; // MultiThreadedStrategy

/**
 * Optimised strategy can be used when there is a single producer thread
 *  claiming {@link AbstractEntry}s.
 */
class SingleThreadedStrategy : public ClaimStrategy {
private:

	long _sequence;

public:
	SingleThreadedStrategy() : _sequence(INITIAL_CURSOR_VALUE) {}

	virtual long incrementAndGet() { return ++_sequence; }

	virtual long incrementAndGet(const int delta) {
        _sequence += delta;
        return _sequence;
    }

    virtual void setSequence(const long seq) { _sequence = seq; }
}; // SingleThreadedStrategy

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

/**
 * No operation version of a {@link Consumer} that simply tracks a {@link RingBuffer}.
 * This is useful in tests or for pre-filling a {@link RingBuffer} from a producer.
 */
template <class T>
class NoOpConsumer : public Consumer {
private:
	const RingBuffer<T>& _ringBuffer;

public:
    /**
     * Construct a {@link Consumer} that simply tracks a {@link RingBuffer}.
     *
     * @param ringBuffer to track.
     */
	NoOpConsumer(const RingBuffer<T>& ringBuffer)
    		: _ringBuffer(ringBuffer)  {  }


	virtual long getSequence() { return _ringBuffer.getCursor();  }

    virtual void halt() {  }

	virtual void run()  {  }
}; // NoOpConsumer



/**
 * Implement this interface to be notified when a thread for the {@link BatchConsumer} starts and shuts down.
 */
class LifecycleAware {
public:
	/**
     * Called once on thread start before first entry is available.
     */
    virtual void onStart() = 0;

    /**
     * Called once just before the thread is shutdown.
     */
    virtual void onShutdown() = 0;
}; // lifecycleaware




//////////////////////// batch consumer

/**
 * Convenience class for handling the batching semantics of consuming entries from a {@link RingBuffer}
 * and delegating the available {@link AbstractEntry}s to a {@link BatchHandler}.
 *
 * If the {@link BatchHandler} also implements {@link LifecycleAware} it will be notified just after the thread
 * is started and just before the thread is shutdown.
 *
 * @param <T> Entry implementation storing the data for sharing during exchange or parallel coordination of an event.
 */
template <typename T>
class BatchConsumer : public Consumer
{
private:

	friend class SequenceTrackerCallback;
	const ConsumerBarrier<T>& _consumerBarrier;
    const BatchHandler<T>& 	_handler;
    const ExceptionHandler* _exceptionHandler ;//= new FatalExceptionHandler();

    long p1, p2, p3, p4, p5, p6, p7;  // cache line padding
    volatile boolean _running = true;
    long p8, p9, p10, p11, p12, p13, p14; // cache line padding
    tbb::atomic<long> _sequence;// = RingBuffer.INITIAL_CURSOR_VALUE;
	long p15, p16, p17, p18, p19, p20; // cache line padding

public:
    /**
     * Construct a batch consumer that will automatically track the progress by updating its sequence when
     * the {@link BatchHandler#onAvailable(AbstractEntry)} method returns.
     *
     * @param consumerBarrier on which it is waiting.
     * @param handler is the delegate to which {@link AbstractEntry}s are dispatched.
     */
    BatchConsumer(const ConsumerBarrier<T>& consumerBarrier,
                         const BatchHandler<T>& handler)
    : _consumerBarrier(consumerBarrier), _handler(handler),
      _exceptionHandler(new FatalExceptionHandler()),
      _sequence(RingBuffer.INITIAL_CURSOR_VALUE)
    { }

    /**
     * Construct a batch consumer that will rely on the {@link SequenceTrackingHandler}
     * to callback via the {@link BatchConsumer.SequenceTrackerCallback} when it has
     * completed with a sequence within a batch.  Sequence will be updated at the end of
     * a batch regardless.
     *
     * @param consumerBarrier on which it is waiting.
     * @param entryHandler is the delegate to which {@link AbstractEntry}s are dispatched.
     */
//    public BatchConsumer(final ConsumerBarrier<T> consumerBarrier,
//                         final SequenceTrackingHandler<T> entryHandler)
//    {
//        this.consumerBarrier = consumerBarrier;
//        this.handler = entryHandler;
//
//        entryHandler.setSequenceTrackerCallback(new SequenceTrackerCallback());
//    }

	long getSequence() const { return _sequence; }

	void halt()
    {
        _running = false;
        _consumerBarrier.alert();
    }

    /**
     * Set a new {@link ExceptionHandler} for handling exceptions propagated out of the {@link BatchConsumer}
     *
     * @param exceptionHandler to replace the existing exceptionHandler.
     */
	void setExceptionHandler(const ExceptionHandler& exceptionHandler)
    {
        if (null == exceptionHandler)
        {
        	//throw new NullPointerException();
        	std::cerr << "BatchHandler.setExceptionHandler: NULL!" << std::endl;
        }
        if (!_exceptionHandler) delete _exceptionHandler;
        _exceptionHandler = exceptionHandler;
    }

    /**
     * Get the {@link ConsumerBarrier} the {@link Consumer} is waiting on.
     *
      * @return the barrier this {@link Consumer} is using.
     */
    ConsumerBarrier<T>& getConsumerBarrier() const
    	{ return consumerBarrier; }

    /**
     * It is ok to have another thread rerun this method after a halt().
     */
    void run()
    {
        _running = true;
//        if (LifecycleAware.class.isAssignableFrom(handler.getClass()))
//        {
//            ((LifecycleAware)handler).onStart();
//        }

        T entry = null;
        long nextSequence = sequence + 1 ;
        while (_running)
        {
//            try
//            {
//                const long availableSequence = consumerBarrier.waitFor(nextSequence);
//                for (; nextSequence <= availableSequence; nextSequence++)
//                {
//                    entry = consumerBarrier.getEntry(nextSequence);
//                    handler.onAvailable(entry);
//                }
//
//                handler.onEndOfBatch();
//                sequence = entry.getSequence();
//            }
//            catch (final AlertException ex)
//            {
//                // Wake up from blocking wait and check if we should continue to run
//            }
//            catch (final Exception ex)
//            {
//                exceptionHandler.handle(ex, entry);
//                sequence = entry.getSequence();
//                nextSequence = entry.getSequence() + 1;
//            }
        }

//        if (LifecycleAware.class.isAssignableFrom(handler.getClass()))
//        {
//            ((LifecycleAware)handler).onShutdown();
//        }
    }

}; // batchconsumer
/**
 * Used by the {@link BatchHandler} to signal when it has completed consuming a given sequence.
 */
template <typename T>
class SequenceTrackerCallback
{
private:
	BatchConsumer<T>& _batchConsumer;

public:

	SequenceTrackerCallback(BatchConsumer<T>& batchConsumer)
	: _batchConsumer(batchConsumer) {}

    /**
     * Notify that the handler has consumed up to a given sequence.
     *
     * @param sequence that has been consumed.
     */
    void onCompleted(const long sequence)  {
    	_batchConsumer._sequence = sequence;
    }
}; // SequenceTrackerCallback

//////////////////////// batch consumer

}; // namespace disruptor


#endif /* DISRUPTOR_HPP_ */
