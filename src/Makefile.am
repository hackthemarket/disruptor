bin_PROGRAMS=perftests 
perftests_SOURCES=perf_tests.cpp disruptor.cpp
AM_CPPFLAGS = -I ~/dev/tbb/include
AM_CXXFLAGS = -lboost_thread -lboost_system  -ltbb -O3
AM_LDFLAGS = -L ~/dev/tbb/build/linux_intel64_gcc_cc6.2.1_libc2.23_kernel4.7.6_release
#AM_LIBRARIES = -L /home/tingarg/dev/tbb30_20110427oss/build/linux_intel64_gcc_cc4.4.3_libc2.11.1_kernel2.6.32_release
