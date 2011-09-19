/*
 * EventProcessor.hpp
 *
 *  Created on: Sep 16, 2011
 *      Author: tingarg
 */

#ifndef EVENTPROCESSOR_HPP_
#define EVENTPROCESSOR_HPP_

namespace disruptor {

class Sequence;

/**
 *  EventProcessors waitFor events to become available for consumption
 *  from the {@link RingBuffer}
 *
 * An EventProcessor will be associated with a Thread for execution.
 *
 */
class EventProcessor {
public:
	/**
	 * Get the sequence up to which this Consumer has consumed
	 * {@link AbstractEntry}s
	 */
    virtual Sequence& getSequence() = 0;

    /**
     * Signal that this Consumer should stop when it has finished consuming at
     * the next clean break.  It will call {@link ConsumerBarrier#alert()} to
     * notify the thread to check status.
     */
    virtual void halt() = 0;

    // Runnable...
    virtual void run() = 0;

}; // consumer

}; // namespace disruptor {

#endif /* EVENTPROCESSOR_HPP_ */
