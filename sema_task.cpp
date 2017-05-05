
#include <functional>

namespace ssvi {
    class sema_task {
        public: sema_task(
            std::function<void()>start
            , std::function<void()>complete
        ) : 
            _start(start),
            _complete(complete) {}

        std::function<void()> _start;
        std::function<void()> _complete;
    };
}
