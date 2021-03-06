#include "Task.h"

#include <iostream>

Task::Task(std::shared_ptr<ThreadPool> fast_task_thread_pool,
    std::shared_ptr<ThreadPool> slow_task_thread_pool,
    std::shared_ptr<boost::asio::ip::tcp::socket> socket)
    : m_fast_task_thread_pool(std::move(fast_task_thread_pool))
    , m_slow_task_thread_pool(std::move(slow_task_thread_pool))
    , m_sock(std::move(socket))
{

}

TaskFuture<uint64_t> Task::handle_task(const std::string& request)
{
    check_session();

    const auto task_category = get_task_type(request);
    auto tasks_count = get_data_from_request_string(request);

    switch (task_category)
    {
        case task_category::slow:
        {
            return m_slow_task_thread_pool->submit([this, tasks_count]
            {
                return emulate_task(tasks_count);
            });
        }

        case task_category::fast:
        {
            uint64_t result = 0;
            try
            {
                std::queue<TaskFuture<uint64_t>> results;

                for (uint32_t i = 0; i < tasks_count; ++i)
                {
                    results.emplace(m_fast_task_thread_pool->submit([this, tasks_count]
                    {
                        return emulate_task(tasks_count);
                    }));
                }

                while (!results.empty())
                {
                    result += results.front().get();
                    results.pop();
                }
            }
            catch (const std::exception& exception)
            {
                std::cout << exception.what() << std::endl;
            }

            std::promise<uint64_t> promise;
            promise.set_value(result);
            return TaskFuture<uint64_t>(promise.get_future());
        }
    }

    return TaskFuture<uint64_t>(std::future<uint64_t>());
}

void Task::check_session()
{
    std::lock_guard<std::mutex> lock(m_socket_mtx);
    if (!is_session_alive(m_sock))
    {
        throw std::runtime_error("task was canceled");
    }
}

uint64_t Task::emulate_task(uint32_t number)
{
    check_session();

    // emulate long computation operation
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return number * 2;
}

task_category Task::get_task_type(const std::string& request_string)
{
    if (request_string.find(to_str(task_category::slow)) != std::string::npos)
    {
        return task_category::slow;
    }

    if (request_string.find(to_str(task_category::fast)) != std::string::npos)
    {
        return task_category::fast;
    }

    throw std::runtime_error("unknown task_category");
}

uint32_t Task::get_data_from_request_string(const std::string& request)
{
    return std::stoi(request.substr(request.find(data_delimiter) + 1));
}
