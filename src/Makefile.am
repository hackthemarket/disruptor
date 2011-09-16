bin_PROGRAMS=perftests 
perftests_SOURCES=perf_tests.cpp disruptor.cpp
INCLUDES = -I /home/tingarg/dev/tbb/include
#AM_LIBRARIES = -L /home/tingarg/dev/tbb30_20110427oss/build/linux_intel64_gcc_cc4.4.3_libc2.11.1_kernel2.6.32_release
AM_CXXFLAGS = -lboost_thread  -ltbb -O3
AM_LDFLAGS = -L /home/tingarg/dev/tbb30_20110427oss/build/linux_intel64_gcc_cc4.4.3_libc2.11.1_kernel2.6.32_release