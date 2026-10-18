#include "scenes/ScenePackage.h"

#include <iostream>

#ifdef _WIN32
int wmain(int count, wchar_t* arguments[]) {
#else
int main(int count, char* arguments[]) {
#endif
    if (count != 2) {
        std::cerr << "Usage: AetherWall.SceneCheck SCENE_DIRECTORY_OR_AWSCENE\n";
        return 2;
    }
    try {
        const auto package = aetherwall::loadScenePackage(std::filesystem::path(arguments[1]));
        std::cout << "Valid manifest, package and image headers: " << package->scene.title
                  << "; layers=" << package->scene.layers.size()
                  << "; animated=" << package->scene.animated()
                  << "\nFull pixel decoding is verified by Core on Windows.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Scene rejected: " << error.what() << '\n';
        return 1;
    }
}
