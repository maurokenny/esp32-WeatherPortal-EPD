# Contributing to ESP32 Weather E-Paper Display

Thank you for your interest in contributing to this project! We welcome contributions from the community.

## How to Contribute

1. **Fork the repository** on GitHub
2. **Create a feature branch** from `main`
3. **Make your changes** following the code style guidelines
4. **Test thoroughly** - build, flash, and test on hardware
5. **Submit a pull request** with a clear description

## Code Style Guidelines

- Use C++17 features consistently
- Follow the existing naming conventions (camelCase for functions/variables)
- Add Doxygen-style comments for new functions
- Keep functions focused and modular
- Test changes in both mockup and live modes

## Development Setup

1. Install PlatformIO
2. Clone and enter the project directory
3. Optionally copy `.env.example` to `.env` and configure WiFi credentials (or use the web portal for setup)
4. Build and test: `cd platformio && pio run`

## Reporting Issues

When reporting bugs, please include:
- Hardware setup (ESP32 board, display type)
- Steps to reproduce
- Expected vs actual behavior
- Serial output logs

## License

By contributing, you agree that your contributions will be licensed under the same GPL v3.0 license as the project.