# Blue-Bird

**A minimalist, modular web framework in C.**

## Overview

Blue-Bird is a lightweight web framework for C developers. It is designed for simplicity, modularity, and extensibility, allowing you to build web applications without the overhead of heavy frameworks.

## Features

- Minimalist core written in C  
- Modular design for easy extension  
- Routing and request handling  
- Logging and Persistence
- Builds examples and tests automatically  

## Repository Structure

```txt
blue-bird/
├── include/       # Public headers
├── src/           # Core framework source code
├── examples/      # Example web applications
├── tests/         # Unit and Integration tests
├── CMakeLists.txt # Build configuration
└── README.md
```

## Getting Started

### Build

```bash
mkdir build
cd build
cmake ..
make
```

This builds the framework, all examples, and tests automatically.

### Run Examples

After building, navigate to the `examples/` directory and run any of the built example programs.

## Example: Minimal Web Server

Here’s how to create a minimal web server using Blue-Bird:

```c
#include "core/server.h"

BBError root_handler(request_t *req, response_t *res)
{
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello, Blue-Bird :)");
    return BB_SUCCESS();
}

int main()
{
    Server server;
    init_server(&server, 8080);

    add_route(&server, "GET", "/", root_handler);
    
    start_server(&server);
    return 0;
}
```

## Contributing

Contributions are welcome! Open issues or submit pull requests for bug fixes and feature additions.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
