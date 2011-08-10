/*
 * disruptor.hpp
 *
 *  Created on: Aug 8, 2011
 *      Author: tingarg
 */

#ifndef DISRUPTOR_HPP_
#define DISRUPTOR_HPP_

#include <atomic.h>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace disruptor {


class AbstractEntry {
private:
    long _sequence;

public:
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
//    private final static Logger LOGGER = Logger.getLogger(FatalExceptionHandler.class.getName());
//    private final Logger logger;

    FatalExceptionHandler() { }

    virtual void handle(const void * ex, const AbstractEntry& currentEntry) {
        //logger.log(Level.SEVERE, "Exception processing: " + currentEntry, ex);

    	std::cerr << "Exception processing " << currentEntry.toString() << std::endl;

//        throw new RuntimeException(ex);
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

    /**
     * Indicates the threading policy to be applied for claiming {@link
     * AbstractEntry}s by producers to the {@link RingBuffer}
     */
    struct Option {
        /**
         * Used by the {@link RingBuffer} as a polymorphic constructor.
         *
         * @return a new instance of the ClaimStrategy
         */
        virtual ClaimStrategy newInstance() = 0;
    }; // Option

	/** Makes the {@link RingBuffer} thread safe for claiming
     * {@link AbstractEntry}s by multiple producing threads. */
    struct MultiThreaded : public Option {
    	virtual ClaimStrategy newInstance() {
    		return new MultiThreadedStrategy();
         }
    }; // MultiThreaded

    /** Optimised {@link RingBuffer} for use by single thread
     * claiming {@link AbstractEntry}s as a producer. */
    struct SingleThreaded : public Option {
    	virtual ClaimStrategy newInstance() {
            return new SingleThreadedStrategy();
         }
    }; // SingleThreaded

    /**
     * Strategy to be used when there are multiple producer threads
     * claiming {@link AbstractEntry}s.
     */
    class MultiThreadedStrategy : public ClaimStrategy  {
    private:

        const Atomic<long> sequence = RingBuffer::INITIAL_CURSOR_VALUE;

    public:
        virtual long incrementAndGet() { return ++sequence; }

        virtual long incrementAndGet(const int delta) {
            sequence+=delta;
            return sequence;
        }

        virtual void setSequence(const long seq) { sequence = sequence; }

    }; // MultiThreadedStrategy

    /**
     * Optimised strategy can be used when there is a single producer thread
     *  claiming {@link AbstractEntry}s.
     */
    class SingleThreadedStrategy : public ClaimStrategy {
    private:

    	long sequence = RingBuffer::INITIAL_CURSOR_VALUE;

    public:

    	virtual long incrementAndGet() { return ++sequence; }

    	virtual long incrementAndGet(const int delta) {
            sequence += delta;
            return sequence;
        }

        virtual void setSequence(const long seq) { sequence = seq; }
    }; // SingleThreadedStrategy
}; // ClaimStrategy






}; // namespace disruptor

#endif /* DISRUPTOR_HPP_ */
