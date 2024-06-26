#pragma once
#include "rprpp/vk/vk.h"

#include "rprpp/ContextObject.h"

namespace rprpp {
class Image;
}

namespace rprpp::filters {

class Filter : public ContextObject {
public:
    explicit Filter(Context* context);
    virtual vk::Semaphore run(std::optional<vk::Semaphore> waitSemaphore) = 0;
    virtual void setInput(Image* image) = 0;
    virtual void setOutput(Image* image) = 0;
};

}