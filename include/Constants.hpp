#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <string>

// This file defines the IPC endpoints for the system.

namespace constants {

// App 1 (Generator) publishes to this endpoint
const std::string GENERATOR_ENDPOINT = "tcp://*:5555";

// App 2 (Extractor) connects to App 1 on this endpoint
const std::string GENERATOR_CONNECT_TO = "tcp://localhost:5555";

// App 2 (Extractor) publishes to this endpoint
const std::string EXTRACTOR_ENDPOINT = "tcp://*:5556";

// App 3 (Logger) connects to App 2 on this endpoint
const std::string EXTRACTOR_CONNECT_TO = "tcp://localhost:5556";

} // namespace constants

#endif // CONSTANTS_HPP
