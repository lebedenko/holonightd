# Third-party notices

JSON for Modern C++ (MIT, Niels Lohmann) and toml++ (MIT, Mark Gillard) are
installed build prerequisites. Their headers are provided by the environment;
this repository no longer distributes vendored copies. toml++ >=3.4 is required.
SQLite3 and optional libsystemd are resolved from installed packages.
Google Test >=1.14 with Google Mock is required only when BUILD_TESTS is ON.
