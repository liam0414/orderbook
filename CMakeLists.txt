# ------------------------------------------------------------ #
# -------------- DO NOT TOUCH BELOW THIS LINE ---------------- #
# ------------------------------------------------------------ #
# execute_process(COMMAND bash util/setup.sh)
cmake_minimum_required(VERSION 3.5...3.15)
project(ORDER_BOOK LANGUAGES CXX)
# we use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(
 -Wall
 -Wextra
 -Werror
 -pedantic-errors
 -Wconversion
 -Wsign-conversion
 -Wdouble-promotion
 -Wcast-align
 -Wformat=2
 -Wuninitialized
 -Wnull-dereference
 -Wnon-virtual-dtor
 -Woverloaded-virtual
 -Wdeprecated-copy-dtor
 -Wold-style-cast
 -Wzero-as-null-pointer-constant
 -Wsuggest-override
 -fstack-protector-strong
)
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-fsanitize=address,undefined")
set(CMAKE_CXX_EXE_LINKER_FLAGS_DEBUG_INIT "-fsanitize=address,undefined")
set(CMAKE_CXX_EXE_LINKER_FLAGS_RELWITHDEBINFO_INIT "-fsanitize=address,undefined")
# add the testing library Catch2
enable_testing()
add_library(catch2_main lib/catch2_main.cpp)
target_include_directories(catch2_main PUBLIC lib)
# link the library so that other programs can get it
link_libraries(catch2_main)

# ------------------------------------------------------------ #
# -------------- DO NOT MODIFY ABOVE THIS LINE --------------- #
# ------------------------------------------------------------ #

add_library(order 
    src/order.cpp
    src/order_book.cpp
    src/price_level.cpp
    src/trade.cpp
)


target_include_directories(order PUBLIC include)
link_libraries(order)

add_executable(client src/client.cpp)

# add_executable(order_test_exe tests/order.test.cpp)
# add_executable(trade_test_exe tests/trade.test.cpp)
# add_executable(price_level_test_exe tests/pricelevel.test.cpp)
add_executable(order_book_test_exe tests/order_book.test.cpp)

# add_test(order_test order_test_exe)
# add_test(trade_test trade_test_exe)
# add_test(price_level_test price_level_test_exe)
add_test(order_book_test order_book_test_exe)
