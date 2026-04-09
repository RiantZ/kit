#pragma once

#include <gtest/gtest.h>
#include <kit/shared_mem.hpp>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <random>
#include <chrono>
#include <vector>

#ifndef G_OS_WINDOWS

////////////////////////////////////////////////////////////////////////////////
class c_shared_test : public ::testing::Test
{
protected:
    static int s_iCounter;

    c_shared::h_shared m_hShared = nullptr;
    std::string        m_sName;

    void SetUp() override
    {
        m_sName = "test_" + std::to_string(getpid()) + "_" + std::to_string(s_iCounter++);
    }

    void TearDown() override
    {
        if(m_hShared)
        {
            c_shared::close(m_hShared);
            m_hShared = nullptr;
        }
        c_shared::unlink(m_sName.c_str());
    }
};

int c_shared_test::s_iCounter = 0;

////////////////////////////////////////////////////////////////////////////////
// create + close
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, CreateAndClose)
{
    const uint8_t l_pData[] = { 0xDE, 0xAD, 0xBE, 0xEF };

    bool l_bRes             = c_shared::create(&m_hShared, m_sName.c_str(), l_pData, sizeof(l_pData));
    ASSERT_TRUE(l_bRes);
    ASSERT_NE(m_hShared, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// create + read back same data
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, CreateThenRead)
{
    const uint8_t l_pData[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

    bool l_bRes             = c_shared::create(&m_hShared, m_sName.c_str(), l_pData, sizeof(l_pData));
    ASSERT_TRUE(l_bRes);

    uint8_t l_pBuf[sizeof(l_pData)] = {};
    l_bRes                          = c_shared::read(m_sName.c_str(), l_pBuf, sizeof(l_pBuf));
    ASSERT_TRUE(l_bRes);

    EXPECT_EQ(0, memcmp(l_pData, l_pBuf, sizeof(l_pData)));
}

////////////////////////////////////////////////////////////////////////////////
// create + write new data + read back
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, CreateWriteRead)
{
    const uint8_t l_pInitial[] = { 0xAA, 0xBB, 0xCC, 0xDD };

    bool l_bRes                = c_shared::create(&m_hShared, m_sName.c_str(), l_pInitial, sizeof(l_pInitial));
    ASSERT_TRUE(l_bRes);

    const uint8_t l_pNew[] = { 0x11, 0x22, 0x33, 0x44 };
    l_bRes                 = c_shared::write(m_sName.c_str(), l_pNew, sizeof(l_pNew));
    ASSERT_TRUE(l_bRes);

    uint8_t l_pBuf[sizeof(l_pNew)] = {};
    l_bRes                         = c_shared::read(m_sName.c_str(), l_pBuf, sizeof(l_pBuf));
    ASSERT_TRUE(l_bRes);

    EXPECT_EQ(0, memcmp(l_pNew, l_pBuf, sizeof(l_pNew)));
}

////////////////////////////////////////////////////////////////////////////////
// get_name returns user-supplied name
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, GetName)
{
    const uint8_t l_pData[] = { 0x42 };

    bool l_bRes             = c_shared::create(&m_hShared, m_sName.c_str(), l_pData, sizeof(l_pData));
    ASSERT_TRUE(l_bRes);

    const tXCHAR *l_pName = c_shared::get_name(m_hShared);
    ASSERT_NE(l_pName, nullptr);
    EXPECT_STREQ(l_pName, m_sName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
// get_name with nullptr handle
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, GetNameNull)
{
    EXPECT_EQ(nullptr, c_shared::get_name(nullptr));
}

////////////////////////////////////////////////////////////////////////////////
// lock + unlock cycle
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, LockUnlock)
{
    const uint8_t l_pData[] = { 0xFF };

    bool l_bRes             = c_shared::create(&m_hShared, m_sName.c_str(), l_pData, sizeof(l_pData));
    ASSERT_TRUE(l_bRes);

    c_shared::h_sem  l_hSem  = nullptr;
    c_shared::e_lock l_eLock = c_shared::lock(m_sName.c_str(), l_hSem, 1000);
    ASSERT_EQ(c_shared::e_ok, l_eLock);
    ASSERT_NE(l_hSem, nullptr);

    l_eLock = c_shared::unlock(l_hSem);
    EXPECT_EQ(c_shared::e_ok, l_eLock);
    EXPECT_EQ(l_hSem, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// lock on non-existent shared object
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, LockNotExists)
{
    c_shared::h_sem  l_hSem  = nullptr;
    c_shared::e_lock l_eLock = c_shared::lock("nonexistent_object", l_hSem, 100);
    EXPECT_EQ(c_shared::e_not_exists, l_eLock);
    EXPECT_EQ(l_hSem, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// unlock with nullptr
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, UnLockNull)
{
    c_shared::h_sem  l_hSem  = nullptr;
    c_shared::e_lock l_eLock = c_shared::unlock(l_hSem);
    EXPECT_EQ(c_shared::e_not_exists, l_eLock);
}

////////////////////////////////////////////////////////////////////////////////
// close(nullptr) returns false
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, CloseNull)
{
    EXPECT_FALSE(c_shared::close(nullptr));
}

////////////////////////////////////////////////////////////////////////////////
// unlink(nullptr) returns false
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, UnLinkNull)
{
    EXPECT_FALSE(c_shared::unlink(nullptr));
}

////////////////////////////////////////////////////////////////////////////////
// create with nullptr params fails
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, CreateNullParams)
{
    const uint8_t l_pData[] = { 0x01 };

    EXPECT_FALSE(c_shared::create(nullptr, m_sName.c_str(), l_pData, sizeof(l_pData)));
    EXPECT_FALSE(c_shared::create(&m_hShared, nullptr, l_pData, sizeof(l_pData)));
    EXPECT_FALSE(c_shared::create(&m_hShared, m_sName.c_str(), nullptr, sizeof(l_pData)));
    EXPECT_FALSE(c_shared::create(&m_hShared, m_sName.c_str(), l_pData, 0));
}

////////////////////////////////////////////////////////////////////////////////
// read with nullptr params fails
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, ReadNullParams)
{
    uint8_t l_pBuf[4] = {};

    EXPECT_FALSE(c_shared::read(nullptr, l_pBuf, sizeof(l_pBuf)));
    EXPECT_FALSE(c_shared::read(m_sName.c_str(), nullptr, sizeof(l_pBuf)));
    EXPECT_FALSE(c_shared::read(m_sName.c_str(), l_pBuf, 0));
}

////////////////////////////////////////////////////////////////////////////////
// write with nullptr params fails
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, WriteNullParams)
{
    const uint8_t l_pData[] = { 0x01 };

    EXPECT_FALSE(c_shared::write(nullptr, l_pData, sizeof(l_pData)));
    EXPECT_FALSE(c_shared::write(m_sName.c_str(), nullptr, sizeof(l_pData)));
    EXPECT_FALSE(c_shared::write(m_sName.c_str(), l_pData, 0));
}

////////////////////////////////////////////////////////////////////////////////
// lock with nullptr name
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, LockNullName)
{
    c_shared::h_sem  l_hSem  = nullptr;
    c_shared::e_lock l_eLock = c_shared::lock(nullptr, l_hSem, 100);
    EXPECT_EQ(c_shared::e_error, l_eLock);
}

////////////////////////////////////////////////////////////////////////////////
// larger data block
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, LargerData)
{
    const uint16_t l_wSize = 4096;
    uint8_t        l_pData[l_wSize];
    for(uint16_t i = 0; i < l_wSize; i++)
    {
        l_pData[i] = (uint8_t)(i & 0xFF);
    }

    bool l_bRes = c_shared::create(&m_hShared, m_sName.c_str(), l_pData, l_wSize);
    ASSERT_TRUE(l_bRes);

    uint8_t l_pBuf[l_wSize] = {};
    l_bRes                  = c_shared::read(m_sName.c_str(), l_pBuf, l_wSize);
    ASSERT_TRUE(l_bRes);

    EXPECT_EQ(0, memcmp(l_pData, l_pBuf, l_wSize));
}

////////////////////////////////////////////////////////////////////////////////
// 32 threads race to create the same shared memory object.
// Winner writes 8KB random pattern; losers lock, read, verify, unlock.
// Main thread waits for all "ready" (10s timeout), then signals shutdown.
// Creator destroys the object; fixture TearDown does final unlink cleanup.
////////////////////////////////////////////////////////////////////////////////
TEST_F(c_shared_test, ConcurrentCreateRead)
{
    static constexpr int      THREAD_COUNT = 32;
    static constexpr uint16_t DATA_SIZE    = 8192;

    std::mt19937         l_oRng(12345);
    std::vector<uint8_t> l_vPattern(DATA_SIZE);
    for(auto &b : l_vPattern)
    {
        b = static_cast<uint8_t>(l_oRng() & 0xFF);
    }

    enum e_state
    {
        STATE_INITIAL = 0,
        STATE_WORKING = 1,
        STATE_READY   = 2
    };

    std::atomic<int> l_pStates[THREAD_COUNT];
    for(auto &s : l_pStates)
    {
        s.store(STATE_INITIAL, std::memory_order_relaxed);
    }

    std::atomic<bool> l_bStart { false };
    std::atomic<bool> l_bShutdown { false };
    std::atomic<int>  l_iCreatorIdx { -1 };

    std::thread l_pThreads[THREAD_COUNT];

    for(int i = 0; i < THREAD_COUNT; i++)
    {
        l_pThreads[i] = std::thread(
            [&, i]()
            {
                l_pStates[i].store(STATE_WORKING, std::memory_order_release);

                while(!l_bStart.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }

                ////////////////////////////////////////////////////////////////
                // Every thread tries to be the creator
                ////////////////////////////////////////////////////////////////
                c_shared::h_shared l_hHandle = nullptr;
                bool l_bCreated = c_shared::create(&l_hHandle, m_sName.c_str(), l_vPattern.data(), DATA_SIZE);

                if(l_bCreated)
                {
                    ////////////////////////////////////////////////////////////
                    // Creator: data is already written by create, semaphore
                    // is already posted -> readers can lock immediately.
                    ////////////////////////////////////////////////////////////
                    l_iCreatorIdx.store(i, std::memory_order_release);
                    l_pStates[i].store(STATE_READY, std::memory_order_release);

                    while(!l_bShutdown.load(std::memory_order_acquire))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }

                    c_shared::close(l_hHandle);
                    c_shared::unlink(m_sName.c_str());
                }
                else
                {
                    ////////////////////////////////////////////////////////////
                    // Reader: lock -> read -> verify -> unlock
                    ////////////////////////////////////////////////////////////
                    std::vector<uint8_t> l_vBuf(DATA_SIZE);
                    bool                 l_bVerified = false;

                    for(int attempt = 0; attempt < 2000 && !l_bVerified; attempt++)
                    {
                        c_shared::h_sem  l_hSem  = nullptr;
                        c_shared::e_lock l_eLock = c_shared::lock(m_sName.c_str(), l_hSem, 100);
                        if(c_shared::e_ok == l_eLock)
                        {
                            if(c_shared::read(m_sName.c_str(), l_vBuf.data(), DATA_SIZE))
                            {
                                if(0 == memcmp(l_vBuf.data(), l_vPattern.data(), DATA_SIZE))
                                {
                                    l_bVerified = true;
                                }
                            }

                            c_shared::unlock(l_hSem);
                        }

                        if(!l_bVerified)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }

                    if(l_bVerified)
                    {
                        l_pStates[i].store(STATE_READY, std::memory_order_release);
                    }
                }
            });
    }

    // Wait until every thread is at least STATE_WORKING
    for(int i = 0; i < THREAD_COUNT; i++)
    {
        while(l_pStates[i].load(std::memory_order_acquire) < STATE_WORKING)
        {
            std::this_thread::yield();
        }
    }

    l_bStart.store(true, std::memory_order_release);

    // Wait up to 10 seconds for all threads to reach STATE_READY
    auto l_tDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool l_bAllReady = false;

    while(std::chrono::steady_clock::now() < l_tDeadline)
    {
        l_bAllReady = true;
        for(int i = 0; i < THREAD_COUNT; i++)
        {
            if(l_pStates[i].load(std::memory_order_acquire) != STATE_READY)
            {
                l_bAllReady = false;
                break;
            }
        }
        if(l_bAllReady)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Signal creator to destroy the shared memory object
    l_bShutdown.store(true, std::memory_order_release);

    for(auto &t : l_pThreads)
    {
        t.join();
    }

    // Verify results
    EXPECT_GE(l_iCreatorIdx.load(), 0) << "No thread managed to create shared memory";

    if(!l_bAllReady)
    {
        for(int i = 0; i < THREAD_COUNT; i++)
        {
            int st = l_pStates[i].load();
            if(st != STATE_READY)
            {
                ADD_FAILURE() << "Thread " << i << " stuck in state " << st
                              << (i == l_iCreatorIdx.load() ? " (creator)" : " (reader)");
            }
        }
        FAIL() << "Not all threads reached 'ready' state within 10 seconds";
    }

    // Creator already called close + unlink.
    // Prevent fixture TearDown from double-closing.
    m_hShared = nullptr;
}

#endif // !G_OS_WINDOWS
