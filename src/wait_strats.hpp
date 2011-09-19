/*
 * wait_strats.hpp
 *
 *  Created on: Sep 16, 2011
 *      Author: tingarg
 */

#ifndef WAIT_STRATS_HPP_
#define WAIT_STRATS_HPP_

#include <string>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "event_processor.hpp"
#include "sequence.hpp"

namespace disruptor {

//template <class event> class RingBuffer; // fwd
//template <class event> class SequenceBarrier;
//class Sequence;

/**
 * Used to alert consumers waiting at a {@link ConsumerBarrier} of status changes.
 */
struct AlertException : public std::exception  {
  const static AlertException  Alert;
}; // AlertException

//long getMinimumSequence(std::vector<EventProcessor*> consumers);

/** wait strategy traits */
//struct Yield;
//
//struct BusySpin;

/**
 * Strategy employed for making {@link Consumer}s wait on a {@link RingBuffer}.
 */
template <typename event>
class WaitStrategy {
private:
	static const int SPIN_TRIES = 100;

public:

    long waitFor( std::vector<Sequence*>& dependents,
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
				boost::posix_time::ptime now();
		        boost::posix_time::time_period per(start,
		        		boost::posix_time::microsec_clock::universal_time());
				if (timeout < per.length()) { break; }
            }
        } else {
            while ((availableSequence =
            		getMinimumSequence(dependents)) < sequence) {
                counter = applyWaitMethod(barrier, counter);
				boost::posix_time::ptime now();
		        boost::posix_time::time_period per(start,
		        		boost::posix_time::microsec_clock::universal_time());
				if (timeout < per.length()) { break; }
            }
        }

        return availableSequence;
    }

    void signalAll() { }

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

}; // WaitStrategy


}; // namespace disruptor

#endif /* WAIT_STRATS_HPP_ */
