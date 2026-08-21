#include "raylib.h"
#include <iostream>

int main() {
    InitWindow(400, 300, "GLB Model Inspector");
    SetTraceLogLevel(LOG_INFO);

    const char* path = "assets/player/character.glb";
    std::cout << "Loading " << path << "...\n";

    Model model = LoadModel(path);
    std::cout << "Mesh Count    : " << model.meshCount << "\n";
    std::cout << "Material Count: " << model.materialCount << "\n";
    std::cout << "Bone Count    : " << model.boneCount << "\n";

    BoundingBox bbox = GetModelBoundingBox(model);
    float width = bbox.max.x - bbox.min.x;
    float height = bbox.max.y - bbox.min.y;
    float depth = bbox.max.z - bbox.min.z;
    std::cout << "Bounding Box  : Width=" << width << "m, Height=" << height << "m, Depth=" << depth << "m\n";

    int animsCount = 0;
    ModelAnimation* anims = LoadModelAnimations(path, &animsCount);
    std::cout << "Animation Count: " << animsCount << "\n";

    if (animsCount > 0 && anims != nullptr) {
        for (int i = 0; i < animsCount; i++) {
            const char* name = (anims[i].name[0] != '\0') ? anims[i].name : "Unnamed";
            std::cout << "  [" << i << "] Name: '" << name << "', Frames: " << anims[i].frameCount
                      << ", BoneCount: " << anims[i].boneCount << "\n";
        }
        UnloadModelAnimations(anims, animsCount);
    } else {
        std::cout << "  -> NO ANIMATIONS FOUND IN THIS GLB MODEL.\n";
    }

    UnloadModel(model);
    CloseWindow();
    return 0;
}
