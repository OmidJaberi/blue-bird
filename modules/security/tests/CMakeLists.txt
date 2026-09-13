bb_add_module_tests(
    security
    bluebird_security
)

# test_regression_invariants.c scans this module's own source files
# for the specific mistakes Phases 1-2 fixed (rand()/srand() in a
# security path, FNV password hashing) - point it at the real source
# directory rather than hardcoding a path in the test itself.
target_compile_definitions(
    security_test_regression_invariants
    PRIVATE
        BB_SECURITY_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../src"
)