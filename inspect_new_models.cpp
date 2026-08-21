#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>

void InspectModel(const std::string& label, const std::string& path) {
    std::cout << "\n======================================================\n";
    std::cout << "MODEL: " << label << " (" << path << ")\n";
    std::cout << "======================================================\n";

    if (!FileExists(path.c_str())) {
        std::cout << "ERROR: File does not exist at " << path << "\n";
        return;
    }

    Model model = LoadModel(path.c_str());
    if (model.meshCount == 0) {
        std::cout << "ERROR: Failed to load model meshes.\n";
        return;
    }

    BoundingBox bbox = GetModelBoundingBox(model);
    float width = bbox.max.x - bbox.min.x;
    float height = bbox.max.y - bbox.min.y;
    float depth = bbox.max.z - bbox.min.z;

    std::cout << "Mesh Count    : " << model.meshCount << "\n";
    std::cout << "Material Count: " << model.materialCount << "\n";
    std::cout << "Bone Count    : " << model.boneCount << "\n";
    std::cout << "Bounding Box  : Width=" << width << "m, Height=" << height << "m, Depth=" << depth << "m\n";

    int animsCount = 0;
    ModelAnimation* anims = LoadModelAnimations(path.c_str(), &animsCount);

    std::cout << "Animation Count: " << animsCount << "\n";

    if (animsCount > 0 && anims != nullptr) {
        for (int i = 0; i < animsCount; i++) {
            const char* name = (anims[i].name[0] != '\0') ? anims[i].name : "Unnamed";
            std::cout << "  [" << i << "] Name: '" << name << "', Frames: " << anims[i].frameCount
                      << ", BoneCount: " << anims[i].boneCount << "\n";
        }
        UnloadModelAnimations(anims, animsCount);
    } else {
        std::cout << "  -> NO ANIMATIONS FOUND IN THIS MODEL.\n";
    }

    UnloadModel(model);
}

int main() {
    InitWindow(400, 300, "GLB Model Inspector");
    SetTraceLogLevel(LOG_WARNING);

    std::vector<std::pair<std::string, std::string>> modelsToInspect = {
        { "Man.glb", "assets/player/Man.glb" },
        { "Man in Long Sleeves.glb", "assets/player/Man in Long Sleeves.glb" },
        { "Man in Suit.glb", "assets/player/Man in Suit.glb" },
        { "Man-fjHyMd5Wxw.glb", "assets/player/Man-fjHyMd5Wxw.glb" }
    };

    for (const auto& item : modelsToInspect) {
        InspectModel(item.first, item.second);
    }

    CloseWindow();
    return 0;
}
