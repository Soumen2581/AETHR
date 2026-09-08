# External dependencies

CMake fetches pinned third-party sources into this folder at configure time:

| Folder | Source | Pin |
|--------|--------|-----|
| `JUCE/` | [juce-framework/JUCE](https://github.com/juce-framework/JUCE) | tag from `AETHR_JUCE_TAG` |
| `Catch2/` | [catchorg/Catch2](https://github.com/catchorg/Catch2) | tag from `AETHR_CATCH2_TAG` |

These checkouts are **not** committed. Do not hand-edit them — change the tags in
`CMakeLists.txt` / CMake cache instead.
