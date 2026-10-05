#pragma once

#include <string>
#include <string_view>
#include <typeinfo>

#if defined(__GNUG__)
    #include <cstdlib>
    #include <cxxabi.h>
    #include <memory>
#endif

namespace ecs
{
    template<typename T>
    const std::string& typeName()
    {
        static const std::string name = []
        {
            std::string result = typeid(T).name();

#if defined(__GNUG__)
            int status = 0;
            std::unique_ptr<char, void (*)(void*)> demangled(
                abi::__cxa_demangle(result.c_str(), nullptr, nullptr, &status), std::free);

            if (status == 0 && demangled)
            {
                result = demangled.get();
            }
#endif

            for (std::string_view prefix : { "struct ", "class " })
            {
                if (result.rfind(prefix, 0) == 0)
                {
                    result.erase(0, prefix.size());
                }
            }

            return result;
        }();

        return name;
    }
}
