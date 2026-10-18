#pragma once

namespace workloom {

class Executor {
public:
    explicit Executor(std::size_t thread_size);

    ~Executor();
};

}