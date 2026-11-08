# mnArpee
JUCE 9 + C++23 minta-arpeggiator (VST3 + Standalone). Részletek: README.md.
- Build: `cmake -S . -B build` majd `cmake --build build --config Release`
- Tesztek (JUCE nélkül): `cmake -S . -B build-tests -DMNARPEE_TESTS_ONLY=ON -DMNARPEE_BUILD_TESTS=ON`, `cmake --build build-tests --config Release`, `build-tests/Release/mnArpeeTests.exe`
- Forrás: `Source/`, tesztek: `Tests/`, telepítő: `installer/` (Inno Setup)
- Ne olvasd be: `build/`, `*.pdf`, `*.png`
