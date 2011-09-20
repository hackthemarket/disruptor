/*
 * wait_strats.hpp
 *
 *  Created on: Sep 16, 2011
 *      Author: tingarg
 */

#ifndef WAIT_STRATS_HPP_
#define WAIT_STRATS_HPP_

#include <boost/date_time/posix_time/posix_time.hpp>

#include "event_processor.hpp"
#include "sequence.hpp"

namespace disruptor {

/**
 * Used to alert consumers waiting at a {@link ConsumerBarrier} of status changes.
 */
struct AlertException : public std::exception  {
  const static AlertException  Alert;
}; // AlertException

/**
 * Strategy employed for making {@link Consumer}s wait on a {@link RingBuffer}.
 */
template <typename event>
class WaitStrategy {
public:

    virtual long waitFor( std::vector<Sequence*>& dependents,
    		Sequence& cursor, SequenceBarrier<event>& barrier, long sequence,
    		boost::posix_time::time_duration timeout =
    				boost::posix_time::milliseconds(0) )  = 0;

    virtual void signalAll() = 0;

};


/**
 * Yielding Strategy employed for making {@link Consumer}s wait on a
 * {@link RingBuffer}.
 */
template <typename event>
class Yield : public WaitStrategy<event> {
private:
	static const int SPIN_TRIES = 100;

public:

    virtual long waitFor( std::vector<Sequence*>& dependents,
    		Sequence& cursor, SequenceBarrier<event>& barrier, long sequence,
    		boost::posix_time::time_duration timeout =
    				boost::posix_time::milliseconds(0) ) {

        boost::posix_time::ptime start =
        		boost::posix_time::microsec_clock::universal_time();
		long availableSequence;

        int counter = SPIN_TRIES;
        if (dependents.empty()) {
            while ((availableSequence = cursor.get()) < sequence) {
                counter = applyWaitMethod(barrier, counter);
		        boost::posix_time::time_period per(start,
		        		boost::posix_time::microsec_clock::universal_time());
				if (timeout < per.length()) { break; }
            }
        } else {
            while ((availableSequence =
            		getMinimumSequence(dependents)) < sequence) {
                counter = applyWaitMethod(barrier, counter);
		        boost::posix_time::time_period per(start,
		        		boost::posix_time::microsec_clock::universal_time());
				if (timeout < per.length()) { break; }
            }
        }

        return availableSequence;
    }

    virtual void signalAll() { }

private:

    int applyWaitMethod(SequenceBarrier<event>& barrier, int counter) {
    	if (barrier.isAlerted()) {
			throw AlertException::Alert;
    	}
    	if (0 == counter) { boost::this_thread::yield(); }
		else {
			--counter;
		}

    return counter;
}

}; // Yield


}; // namespace disruptor

#endif /* WAIT_STRATS_HPP_ */
