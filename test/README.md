# PAM module testing

1. Install Google Test (e.g. `libgtest-dev` on Debian/Ubuntu or `gtest-devel` on Rocky/Fedora).
2. Run mock server `./mock_server.py`.
3. In a new terminal window execute `ctest --test-dir build --output-on-failure` (or `cmake --build build --target check`) to run the tests.
