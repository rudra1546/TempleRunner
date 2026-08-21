#include "raylib.h"
#include <iostream>

int main() {
    InitWindow(400, 300, "Model Inspector");

    const char* modelPath = "assets/player/player.glb";
    if (!FileExists(modelPath)) {
        std::cout << "ERROR: File does not exist: " << modelPath << std::endl;
        CloseWindow();
        return 1;
    }

    Model model = LoadModel(modelPath);
    std::cout << "=== Model Info ===" << std::endl;
    std::cout << "Mesh Count: " << model.meshCount << std::endl;
    std::cout << "Material Count: " << model.materialCount << std::endl;
    std::cout << "Bone Count: " << model.boneCount << std::endl;

    BoundingBox bbox = GetModelBoundingBox(model);
    std::cout << "BBox Min: (" << bbox.min.x << ", " << bbox.min.y << ", " << bbox.min.z << ")" << std::endl;
    std::cout << "BBox Max: (" << bbox.max.x << ", " << bbox.max.y << ", " << bbox.max.z << ")" << std::endl;

    int animsCount = 0;
    ModelAnimation* anims = LoadModelAnimations(modelPath, &animsCount);
    std::cout << "\n=== Animations Loaded: " << animsCount << " ===" << std::endl;

    for (int i = 0; i < animsCount; i++) {
        std::cout << "Animation [" << i << "]: "
                  << "Name: '" << (anims[i].name[0] ? anims[i].name : "Unnamed") << "', "
                  << "FrameCount: " << anims[i].frameCount << ", "
                  << "BoneCount: " << anims[i].boneCount << std::endl;
    }

    if (animsCount > 0) {
        UnloadModelAnimations(anims, animsCount);
    }
    UnloadModel(model);
    CloseWindow();

    return 0;
}
