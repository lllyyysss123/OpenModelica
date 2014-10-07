#pragma once

#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
#include <boost/lockfree/queue.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/thread.hpp>
#endif

#define CONTAINER_COUNT 2

template<size_t dim_1, size_t dim_2, size_t dim_3, size_t dim_4>
class Writer
{
 public:
    typedef StatArrayDim1<double, dim_1> value_type_v;
    typedef StatArrayDim1<double, dim_2> value_type_dv;
    typedef StatArrayDim1<double, dim_4> value_type_p;

    Writer()
            : _writeContainers()
            ,_freeContainers()
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
            ,_freeContainerMutex(1)
            ,_writeContainerMutex(1)
            ,_nempty(CONTAINER_COUNT)
            ,_writerThread(&Writer::writeThread, this)
            ,_threadWorkDone(false)
#endif
    {
        for (int i = 0; i < CONTAINER_COUNT; ++i)
        {
            value_type_v *v = new value_type_v();
            value_type_dv *dv = new value_type_dv();
            boost::tuple<value_type_v*, value_type_dv*, double> *tpl = new boost::tuple<value_type_v*, value_type_dv*, double>();
            get < 0 > (*tpl) = v;
            get < 1 > (*tpl) = dv;
            _freeContainers.push_back(tpl);
        }
    }

    virtual ~Writer()
    {
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        //wait until the writer-thread has written all results
        _threadWorkDone = true;
        _writerThread.join();
#endif
    }

    virtual void write(value_type_v& v_list, value_type_dv& v2_list, double time) = 0;

    boost::tuple<value_type_v*, value_type_dv*, double>* getFreeContainer()
    {
        boost::tuple<value_type_v*, value_type_dv*, double>* container = NULL;
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _nempty.wait();
        _freeContainerMutex.wait();
#endif
        container = _freeContainers.front();
        _freeContainers.pop_front();
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _freeContainerMutex.post();
#endif
        return container;
    };

    void addContainerToWriteQueue(boost::tuple<value_type_v*, value_type_dv*, double> *container)
    {
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _writeContainerMutex.wait();
#endif
        _writeContainers.push_back(container);
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _writeContainerMutex.post();
#else
        writeContainer();
#endif
    };

 protected:
    void writeContainer()
    {
        boost::tuple<value_type_v*, value_type_dv*, double>* container = NULL;

#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _writeContainerMutex.wait();
#endif
        if (!_writeContainers.empty())
            container = _writeContainers.front();
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _writeContainerMutex.post();
#endif

        if (container == NULL)
        {
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
            usleep(1);
#endif
            return;
        }

        value_type_v *v_list = get<0>(*container);
        value_type_dv *v2_list = get<1>(*container);
        double time = get<2>(*container);

        write(*v_list, *v2_list, time);

#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _writeContainerMutex.wait();
#endif
        _writeContainers.pop_front();
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _writeContainerMutex.post();
#endif

#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _freeContainerMutex.wait();
#endif
        _freeContainers.push_back(container);
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        _freeContainerMutex.post();
        _nempty.post();
#endif
    }

    void writeThread()
    {
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
        while(!_threadWorkDone)
        {
            writeContainer();
        }

        while(!_writeContainers.empty())
            writeContainer();
#endif
    }

    std::deque<boost::tuple<value_type_v*, value_type_dv*, double>*> _writeContainers;
    std::deque<boost::tuple<value_type_v*, value_type_dv*, double>*> _freeContainers;
#if defined USE_PARALLEL_OUTPUT && defined USE_BOOST_THREAD
    boost::interprocess::interprocess_semaphore _freeContainerMutex;
    boost::interprocess::interprocess_semaphore _writeContainerMutex;
    boost::interprocess::interprocess_semaphore _nempty;
    boost::thread _writerThread;
    bool _threadWorkDone;
#endif

};
