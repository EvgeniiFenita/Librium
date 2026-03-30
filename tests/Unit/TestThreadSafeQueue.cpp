#include <catch2/catch_test_macros.hpp>

#include "Utils/ThreadSafeQueue.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace Librium::Utils;

TEST_CASE("CThreadSafeQueue basic push/pop", "[queue]")
{
    SECTION("Push and Pop single item")
    {
        CThreadSafeQueue<int> q;
        q.Push(42);
        auto val = q.Pop();
        REQUIRE(val.has_value());
        REQUIRE(*val == 42);
    }

    SECTION("FIFO ordering preserved")
    {
        CThreadSafeQueue<int> q;
        q.Push(1);
        q.Push(2);
        q.Push(3);
        REQUIRE(q.Pop() == 1);
        REQUIRE(q.Pop() == 2);
        REQUIRE(q.Pop() == 3);
    }

    SECTION("Works with string values")
    {
        CThreadSafeQueue<std::string> q;
        q.Push("hello");
        q.Push("world");
        REQUIRE(q.Pop() == "hello");
        REQUIRE(q.Pop() == "world");
    }
}

TEST_CASE("CThreadSafeQueue convenience API", "[queue]")
{
    SECTION("TryPop on empty open queue returns nullopt immediately")
    {
        CThreadSafeQueue<int> q;
        auto val = q.TryPop();
        REQUIRE_FALSE(val.has_value());
        REQUIRE_FALSE(q.IsClosed());
    }

    SECTION("TryPop returns queued item without blocking")
    {
        CThreadSafeQueue<int> q;
        q.Push(7);

        auto val = q.TryPop();
        REQUIRE(val.has_value());
        REQUIRE(*val == 7);
        REQUIRE(q.Size() == 0);
    }

    SECTION("Emplace constructs item in place")
    {
        CThreadSafeQueue<std::pair<int, std::string>> q;
        REQUIRE(q.Emplace(3, "queued"));

        auto val = q.Pop();
        REQUIRE(val.has_value());
        REQUIRE(val->first == 3);
        REQUIRE(val->second == "queued");
    }

    SECTION("Emplace after Close is rejected")
    {
        CThreadSafeQueue<std::string> q;
        q.Close();
        REQUIRE_FALSE(q.Emplace("closed"));
        REQUIRE(q.Size() == 0);
    }
}

TEST_CASE("CThreadSafeQueue size tracking", "[queue]")
{
    SECTION("Size is 0 for empty queue")
    {
        CThreadSafeQueue<int> q;
        REQUIRE(q.Size() == 0);
    }

    SECTION("Size increments on Push")
    {
        CThreadSafeQueue<int> q;
        q.Push(1);
        REQUIRE(q.Size() == 1);
        q.Push(2);
        REQUIRE(q.Size() == 2);
    }

    SECTION("Size decrements on Pop")
    {
        CThreadSafeQueue<int> q;
        q.Push(1);
        q.Push(2);
        (void)q.Pop();
        REQUIRE(q.Size() == 1);
        (void)q.Pop();
        REQUIRE(q.Size() == 0);
    }
}

TEST_CASE("CThreadSafeQueue close semantics", "[queue]")
{
    SECTION("Pop on closed empty queue returns nullopt immediately")
    {
        CThreadSafeQueue<int> q;
        q.Close();
        auto val = q.Pop();
        REQUIRE_FALSE(val.has_value());
    }

    SECTION("Pop on closed non-empty queue returns remaining items")
    {
        CThreadSafeQueue<int> q;
        q.Push(10);
        q.Push(20);
        q.Close();
        // Items already in queue are still available
        auto first = q.Pop();
        REQUIRE(first.has_value());
        REQUIRE(*first == 10);
        auto second = q.Pop();
        REQUIRE(second.has_value());
        REQUIRE(*second == 20);
        // Now empty and closed
        auto third = q.Pop();
        REQUIRE_FALSE(third.has_value());
    }

    SECTION("Push after Close is silently ignored")
    {
        CThreadSafeQueue<int> q;
        q.Close();
        q.Push(99); // must not block or throw
        REQUIRE(q.Size() == 0);
    }

    SECTION("Close unblocks a waiting consumer")
    {
        CThreadSafeQueue<int> q;
        std::atomic<bool> gotNullopt{false};

        std::jthread consumer([&]()
        {
            auto val = q.Pop(); // will block until Close()
            gotNullopt = !val.has_value();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        q.Close();
        consumer.join();

        REQUIRE(gotNullopt);
    }
}

TEST_CASE("CThreadSafeQueue bounded capacity", "[queue]")
{
    SECTION("Bounded queue blocks producer when full")
    {
        CThreadSafeQueue<int> q(2); // maxSize = 2
        q.Push(1);
        q.Push(2);

        std::atomic<bool> pushed{false};
        std::jthread producer([&]()
        {
            q.Push(3); // should block
            pushed = true;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        REQUIRE_FALSE(pushed.load()); // still blocked

        (void)q.Pop(); // unblock producer
        producer.join();
        REQUIRE(pushed.load());
    }

    SECTION("Close unblocks a blocked producer")
    {
        CThreadSafeQueue<int> q(1);
        q.Push(1); // fill it

        std::atomic<bool> producerDone{false};
        std::jthread producer([&]()
        {
            q.Push(2); // will block - queue is full
            producerDone = true;
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        REQUIRE_FALSE(producerDone.load());

        q.Close(); // should unblock the producer
        producer.join();
        REQUIRE(producerDone.load());
    }
}

TEST_CASE("CThreadSafeQueue concurrent stress", "[queue]")
{
    SECTION("Multiple producers, single consumer")
    {
        CThreadSafeQueue<int> q;
        std::atomic<int> totalProduced{0};
        std::atomic<int> totalConsumed{0};
        constexpr int kPerProducer = 200;
        constexpr int kProducers   = 4;

        std::vector<std::jthread> producers;
        producers.reserve(kProducers);
        for (int i = 0; i < kProducers; ++i)
        {
            producers.emplace_back([&]()
            {
                for (int j = 0; j < kPerProducer; ++j)
                {
                    q.Push(j);
                    ++totalProduced;
                }
            });
        }

        producers.clear(); // join all producers

        q.Close();

        while (auto val = q.Pop())
            ++totalConsumed;

        REQUIRE(totalProduced == kProducers * kPerProducer);
        REQUIRE(totalConsumed == kProducers * kPerProducer);
    }

    SECTION("Single producer, multiple consumers")
    {
        constexpr int kItems     = 400;
        constexpr int kConsumers = 4;
        CThreadSafeQueue<int> q;
        std::atomic<int> consumed{0};

        std::vector<std::jthread> consumers;
        consumers.reserve(kConsumers);
        for (int i = 0; i < kConsumers; ++i)
        {
            consumers.emplace_back([&]()
            {
                while (auto val = q.Pop())
                    ++consumed;
            });
        }

        for (int i = 0; i < kItems; ++i)
            q.Push(i);

        q.Close();
        consumers.clear(); // join

        REQUIRE(consumed == kItems);
    }
}
