find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_localbackend tst_localbackend.cpp)
target_link_libraries(tst_localbackend PRIVATE StorageKit::StorageKit Qt6::Test)
add_test(NAME tst_localbackend COMMAND tst_localbackend)
