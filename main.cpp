#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <vector>
#include <cstdlib>
#include <string>

struct Obstacle {
    Vector3 position;
    Vector3 size;
    Color color;
    Color wireColor;
};

struct Coin {
    Vector3 position;
    bool collected;
    float rotation; // Y-axis rotation angle in degrees
};

// Rigged 3D Humanoid Player Structure
struct PlayerCharacter {
    Vector3 position;
    float collisionRadius;
    int currentLane;      // 0 = Visually Left (+2.8f), 1 = Center (0.0f), 2 = Visually Right (-2.8f)
    float targetX;        // Target X coordinate for smooth lane sliding
    float verticalVelocity;
    bool isGrounded;

    // 3D Model & Skeletal Animations
    Model model;
    bool isModelLoaded;
    ModelAnimation* animations;
    int animsCount;
    int runningAnimIndex;
    int idleAnimIndex;
    int currentAnimIndex;
    float animTime; // DeltaTime accumulated animation time

    // Model transform tuning
    Vector3 modelScale;
    float rotationY;    // Y-axis rotation in degrees to face +Z
    Vector3 drawOffset; // Vertical offset to align feet with ground (y=0)
};

// Natural Mountain Valley Environment System
struct MountainValleySystem {
    Model leftMountainModel;
    Model rightMountainModel;
    BoundingBox leftBbox;
    BoundingBox rightBbox;
    float leftPosX;
    float rightPosX;
    bool isLoaded;
    Vector3 mountainSize;
};

// Sphere vs AABB Box 3D Collision Check
bool CheckCollisionSphereBox(Vector3 center, float radius, BoundingBox box) {
    float closestX = Clamp(center.x, box.min.x, box.max.x);
    float closestY = Clamp(center.y, box.min.y, box.max.y);
    float closestZ = Clamp(center.z, box.min.z, box.max.z);

    float dx = center.x - closestX;
    float dy = center.y - closestY;
    float dz = center.z - closestZ;

    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

// Initialize 3D Rigged Humanoid Model and Skeletal Animations
bool InitPlayerCharacter(PlayerCharacter& player, const char* modelPath) {
    player.position = (Vector3){ 0.0f, 0.0f, 0.0f };
    player.collisionRadius = 0.9f;
    player.currentLane = 1; // Start in Center Lane (0.0f)
    player.targetX = 0.0f;
    player.verticalVelocity = 0.0f;
    player.isGrounded = true;
    player.isModelLoaded = false;
    player.animations = nullptr;
    player.animsCount = 0;
    player.runningAnimIndex = 0;
    player.idleAnimIndex = 0;
    player.currentAnimIndex = 0;
    player.animTime = 0.0f;

    player.modelScale = (Vector3){ 1.0f, 1.0f, 1.0f };
    player.rotationY = 0.0f; // Alignment angle so character faces +Z (away from camera)
    player.drawOffset = (Vector3){ 0.0f, 0.0f, 0.0f };

    if (!FileExists(modelPath)) {
        TraceLog(LOG_ERROR, "ERROR: 3D human character model file not found at path: %s", modelPath);
        return false;
    }

    player.model = LoadModel(modelPath);
    if (player.model.meshCount == 0) {
        TraceLog(LOG_ERROR, "ERROR: Failed to load 3D human model mesh from: %s", modelPath);
        return false;
    }

    player.isModelLoaded = true;
    TraceLog(LOG_INFO, "SUCCESS: 3D Human Character model loaded from %s (Meshes: %d, Bones: %d)",
             modelPath, player.model.meshCount, player.model.boneCount);

    // Calculate model BoundingBox and scale
    BoundingBox bbox = GetModelBoundingBox(player.model);
    TraceLog(LOG_INFO, "MODEL BBOX min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)",
             bbox.min.x, bbox.min.y, bbox.min.z,
             bbox.max.x, bbox.max.y, bbox.max.z);

    float height = bbox.max.y - bbox.min.y;
    if (height > 0.01f) {
        float scaleFactor = 2.5f / height;
        player.modelScale = (Vector3){ scaleFactor, scaleFactor, scaleFactor };
        player.drawOffset.y = -bbox.min.y * scaleFactor;
        TraceLog(LOG_INFO, "Calculated model scaleFactor: %.4f (Target Height: 2.5m)", scaleFactor);
    }

    // Load Skeletal Animations & Search Animation Names Dynamically
    player.animations = LoadModelAnimations(modelPath, &player.animsCount);
    TraceLog(LOG_INFO, "=== Loaded %d Model Animations from %s ===", player.animsCount, modelPath);

    if (player.animsCount > 0) {
        for (int i = 0; i < player.animsCount; i++) {
            const char* animName = (player.animations[i].name[0] != '\0') ? player.animations[i].name : "Human_Anim";
            TraceLog(LOG_INFO, "Animation [%d]: '%s' (%d frames, %d bones)",
                     i, animName, player.animations[i].frameCount, player.animations[i].boneCount);

            std::string nameStr = animName;
            if (nameStr.find("mixamo") != std::string::npos || nameStr.find("Run") != std::string::npos || nameStr.find("run") != std::string::npos) {
                player.runningAnimIndex = i;
            }
        }
        player.currentAnimIndex = player.runningAnimIndex;
        TraceLog(LOG_INFO, "--> Selected Mixamo In-Place Running Animation Index [%d]: '%s'",
                 player.runningAnimIndex, player.animations[player.runningAnimIndex].name);
    } else {
        TraceLog(LOG_WARNING, "WARNING: No animations found in %s", modelPath);
    }

    return true;
}

// Generate Large Organic 3D Mountain Valley Wall Mesh with Realistic Sandstone Strata & Lighting Shading
Model GenerateMountainValleyModel(bool isLeftSide, Vector3 size) {
    const int width = 128;
    const int height = 256;

    // Generate multi-octave Perlin noise heightmaps for rugged fractal mountain peaks
    Image perlin1 = GenImagePerlinNoise(width, height, isLeftSide ? 0 : 500, 0, 3.5f);
    Image perlin2 = GenImagePerlinNoise(width, height, isLeftSide ? 250 : 750, 100, 8.0f);
    Image perlin3 = GenImagePerlinNoise(width, height, isLeftSide ? 400 : 900, 200, 18.0f);

    Color* pix1 = LoadImageColors(perlin1);
    Color* pix2 = LoadImageColors(perlin2);
    Color* pix3 = LoadImageColors(perlin3);

    Color* pixels = (Color*)RL_MALLOC(width * height * sizeof(Color));

    // Sculpt natural canyon cliff profile (road-facing inner edge u=0 has zero height, outer peak edge u=1 has max height)
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            int idx = z * width + x;
            float n1 = (float)pix1[idx].r / 255.0f;
            float n2 = (float)pix2[idx].r / 255.0f;
            float n3 = (float)pix3[idx].r / 255.0f;

            // Multi-fractal noise synthesis
            float rawVal = n1 * 0.60f + n2 * 0.28f + n3 * 0.12f;

            // u = 0 for road-facing inner edge, u = 1 for outer mountain peak edge
            float normX = (float)x / (float)(width - 1);
            float u = isLeftSide ? (1.0f - normX) : normX;

            // Height slope curve (u=0 -> 0 height near road, u=1 -> max height at outer edge)
            float heightSlope = powf(u, 1.8f);
            float finalFactor = rawVal * heightSlope;

            // Explicitly force the first ~25% (u < 0.25) to remain flat/low at ground level
            if (u < 0.25f) {
                finalFactor *= (u / 0.25f);
            }

            unsigned char hVal = (unsigned char)Clamp(finalFactor * 255.0f, 0.0f, 255.0f);
            pixels[idx] = (Color){ hVal, hVal, hVal, 255 };
        }
    }

    UnloadImageColors(pix1);
    UnloadImageColors(pix2);
    UnloadImageColors(pix3);
    UnloadImage(perlin1);
    UnloadImage(perlin2);
    UnloadImage(perlin3);

    Image customHeightmap = {
        .data = pixels,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    Mesh mesh = GenMeshHeightmap(customHeightmap, size);
    UnloadImage(customHeightmap); // Unload calls RL_FREE on pixels

    // Apply realistic sandstone geological strata coloring & directional sun lighting to vertices
    if (mesh.colors != nullptr && mesh.vertices != nullptr) {
        // Directional sun vector for canyon wall illumination
        Vector3 sunDir = Vector3Normalize((Vector3){ -0.5f, 0.75f, -0.4f });

        for (int i = 0; i < mesh.vertexCount; i++) {
            float vx = mesh.vertices[i * 3 + 0];
            float vy = mesh.vertices[i * 3 + 1];
            float vz = mesh.vertices[i * 3 + 2];

            // Extract vertex normal vector
            Vector3 normal = { 0.0f, 1.0f, 0.0f };
            if (mesh.normals != nullptr) {
                normal = (Vector3){
                    mesh.normals[i * 3 + 0],
                    mesh.normals[i * 3 + 1],
                    mesh.normals[i * 3 + 2]
                };
            }

            // Directional sun lighting factor
            float diff = Vector3DotProduct(normal, sunDir);
            float lightFactor = Clamp(diff * 0.55f + 0.45f, 0.25f, 1.0f);

            // Geological sandstone strata layer calculation (horizontal wavy bands)
            float strataWave = sinf(vy * 0.42f + vx * 0.05f + vz * 0.03f);

            // Sandstone palette interpolation
            Vector3 baseCol;
            if (strataWave > 0.35f) {
                // Bright Tan / Beige Sandstone Layer
                baseCol = (Vector3){ 208.0f, 162.0f, 112.0f };
            } else if (strataWave > -0.25f) {
                // Rich Terracotta Red / Orange Sandstone Strata
                baseCol = (Vector3){ 182.0f, 96.0f, 54.0f };
            } else {
                // Dark Weathered Brown Basalt / Iron Rock Layer
                baseCol = (Vector3){ 98.0f, 58.0f, 38.0f };
            }

            // Slope shading: Sheer vertical cliffs receive darker crevice shadows
            float slopeFactor = Clamp(normal.y, 0.0f, 1.0f);
            if (slopeFactor < 0.45f) {
                float crevice = (0.45f - slopeFactor) / 0.45f;
                baseCol = Vector3Lerp(baseCol, (Vector3){ 48.0f, 30.0f, 20.0f }, crevice * 0.65f);
            }

            // Apply directional sun lighting
            baseCol.x *= lightFactor;
            baseCol.y *= lightFactor;
            baseCol.z *= lightFactor;

            mesh.colors[i * 4 + 0] = (unsigned char)Clamp(baseCol.x, 0.0f, 255.0f);
            mesh.colors[i * 4 + 1] = (unsigned char)Clamp(baseCol.y, 0.0f, 255.0f);
            mesh.colors[i * 4 + 2] = (unsigned char)Clamp(baseCol.z, 0.0f, 255.0f);
            mesh.colors[i * 4 + 3] = 255;
        }
    }

    Model model = LoadModelFromMesh(mesh);
    return model;
}

// Initialize Natural Mountain Valley Environment System
void InitMountainValleySystem(MountainValleySystem& valley) {
    valley.mountainSize = (Vector3){ 48.0f, 42.0f, 160.0f }; // Width 48m, Peak Height 42m, Segment Length 160m

    valley.leftMountainModel = GenerateMountainValleyModel(true, valley.mountainSize);
    valley.rightMountainModel = GenerateMountainValleyModel(false, valley.mountainSize);

    valley.leftBbox = GetModelBoundingBox(valley.leftMountainModel);
    valley.rightBbox = GetModelBoundingBox(valley.rightMountainModel);

    TraceLog(LOG_INFO, "Left Mountain bbox min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)",
             valley.leftBbox.min.x, valley.leftBbox.min.y, valley.leftBbox.min.z,
             valley.leftBbox.max.x, valley.leftBbox.max.y, valley.leftBbox.max.z);

    TraceLog(LOG_INFO, "Right Mountain bbox min=(%.2f %.2f %.2f) max=(%.2f %.2f %.2f)",
             valley.rightBbox.min.x, valley.rightBbox.min.y, valley.rightBbox.min.z,
             valley.rightBbox.max.x, valley.rightBbox.max.y, valley.rightBbox.max.z);

    // HARD GEOMETRIC GUARANTEE:
    // leftPos.x + leftMaxX <= -15.0f  ==> leftPos.x = -15.0f - leftBbox.max.x
    // rightPos.x + rightMinX >= 15.0f ==> rightPos.x = 15.0f - rightBbox.min.x
    valley.leftPosX = -15.0f - valley.leftBbox.max.x;
    valley.rightPosX = 15.0f - valley.rightBbox.min.x;

    TraceLog(LOG_INFO, "HARD GEOMETRIC GUARANTEE: LeftPosX = %.2f (Max Mesh X = %.2f <= -15.0m), RightPosX = %.2f (Min Mesh X = %.2f >= +15.0m)",
             valley.leftPosX, valley.leftPosX + valley.leftBbox.max.x,
             valley.rightPosX, valley.rightPosX + valley.rightBbox.min.x);

    valley.isLoaded = true;
}

// Unload Mountain Valley Models
void UnloadMountainValleySystem(MountainValleySystem& valley) {
    if (valley.isLoaded) {
        UnloadModel(valley.leftMountainModel);
        UnloadModel(valley.rightMountainModel);
        valley.isLoaded = false;
    }
}

// Update Skeletal Model Animation Frame for pure In-Place running cycle
void UpdatePlayerAnimation(PlayerCharacter& player, bool isRunning, float deltaTime) {
    if (!player.isModelLoaded || player.animsCount == 0) return;

    if (isRunning) {
        ModelAnimation anim = player.animations[player.runningAnimIndex];

        // 30 FPS playback rate for Mixamo running sprint animation
        const float animSpeed = 30.0f;
        player.animTime += deltaTime * animSpeed;

        int currentFrame = (int)player.animTime % anim.frameCount;
        UpdateModelAnimation(player.model, anim, currentFrame);
    }
}

// Render Rigged 3D Humanoid Model
void DrawPlayerCharacter(const PlayerCharacter& player) {
    if (!player.isModelLoaded) return;

    Vector3 drawPos = Vector3Add(player.position, player.drawOffset);
    DrawModelEx(
        player.model,
        drawPos,
        (Vector3){ 0.0f, 1.0f, 0.0f },
        player.rotationY,
        player.modelScale,
        WHITE
    );
}

// Unload Model and Animations
void UnloadPlayerCharacter(PlayerCharacter& player) {
    if (player.isModelLoaded) {
        if (player.animations != nullptr && player.animsCount > 0) {
            UnloadModelAnimations(player.animations, player.animsCount);
            player.animations = nullptr;
            player.animsCount = 0;
        }
        UnloadModel(player.model);
        player.isModelLoaded = false;
    }
}

// Helper function to render a colored 3D quad using Raylib rlgl
void DrawQuad3D(Vector3 p1, Vector3 p2, Vector3 p3, Vector3 p4, Color color) {
    rlSetTexture(0);
    rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(p1.x, p1.y, p1.z);
        rlVertex3f(p2.x, p2.y, p2.z);
        rlVertex3f(p3.x, p3.y, p3.z);
        rlVertex3f(p4.x, p4.y, p4.z);
    rlEnd();
}

// Render Continuous Animated Canyon River & Sandy Banks (Parallel to road on RIGHT side: X = +8.0m to +13.5m)
void DrawRiverAndBanks(float playerZ, float time) {
    const float riverStartX = 8.0f;
    const float riverWidth  = 5.5f;
    const float riverEndX   = riverStartX + riverWidth; // +13.5m (well inside mountain exclusion zone +15.0m!)

    // --- 1. Natural Sandy / Wet Riverbed Banks (X = +5.4m to +14.0m) ---
    const float segLength = 6.0f;
    int startSeg = (int)((playerZ - 40.0f) / segLength);
    int endSeg   = (int)((playerZ + 240.0f) / segLength);

    for (int i = startSeg; i <= endSeg; i++) {
        float segZ = i * segLength + segLength / 2.0f;

        // Sandy shore bank between road curb and river
        Vector3 bankPos = { 6.7f, -0.2f, segZ };
        DrawCube(bankPos, 2.6f, 0.5f, segLength, (Color){ 175, 140, 100, 255 });
        DrawCubeWires(bankPos, 2.6f, 0.5f, segLength, (Color){ 140, 110, 75, 255 });

        // Dark wet riverbed under water
        Vector3 bedPos = { (riverStartX + riverEndX) / 2.0f, -0.4f, segZ };
        DrawCube(bedPos, riverWidth + 0.6f, 0.5f, segLength, (Color){ 70, 60, 48, 255 });
    }

    // --- 2. Animated Procedural Teal Water Surface with Wave Ripples ---
    const float quadLength = 4.0f;
    int startQuad = (int)((playerZ - 40.0f) / quadLength);
    int endQuad   = (int)((playerZ + 240.0f) / quadLength);

    for (int q = startQuad; q <= endQuad; q++) {
        float z1 = q * quadLength;
        float z2 = z1 + quadLength;

        // Animate vertical water wave oscillation
        float waveY1 = 0.04f + 0.035f * sinf(z1 * 0.25f + time * 3.2f);
        float waveY2 = 0.04f + 0.035f * sinf(z2 * 0.25f + time * 3.2f);

        // Water surface main body (Teal / Deep Canyon Water)
        Vector3 p1 = { riverStartX, waveY1, z1 };
        Vector3 p2 = { riverEndX,   waveY1, z1 };
        Vector3 p3 = { riverEndX,   waveY2, z2 };
        Vector3 p4 = { riverStartX, waveY2, z2 };

        Color waterCol = (Color){ 30, 125, 165, 220 };
        DrawQuad3D(p1, p2, p3, p4, waterCol);

        // Lighter aqua foam / water ripple highlights along river center
        float midX1 = riverStartX + 1.2f + 0.5f * sinf(z1 * 0.4f + time * 2.0f);
        float midX2 = riverStartX + 3.8f + 0.5f * cosf(z2 * 0.4f + time * 2.0f);

        Vector3 r1 = { midX1, waveY1 + 0.005f, z1 };
        Vector3 r2 = { midX2, waveY1 + 0.005f, z1 };
        Vector3 r3 = { midX2, waveY2 + 0.005f, z2 };
        Vector3 r4 = { midX1, waveY2 + 0.005f, z2 };

        Color rippleCol = (Color){ 100, 210, 240, 180 };
        DrawQuad3D(r1, r2, r3, r4, rippleCol);
    }
}

// Render Ancient Sandstone Road & Large Continuous Mountain Valley (Zero Obstructive Collisions)
void DrawMountainValleyEnvironment(const MountainValleySystem& valley, float playerZ, float roadWidth) {
    const float roadHalfWidth = roadWidth / 2.0f;

    // --- 1. Ancient Sandstone Tile Road ---
    const float tileLength = 6.0f;
    int startTile = (int)((playerZ - 40.0f) / tileLength);
    int endTile = (int)((playerZ + 250.0f) / tileLength);

    for (int i = startTile; i <= endTile; i++) {
        float tileZ = i * tileLength + tileLength / 2.0f;
        Vector3 tilePos = { 0.0f, -0.5f, tileZ };

        // Alternating worn sandstone tile tones
        Color tileColor = (i % 2 == 0) ? (Color){ 145, 130, 110, 255 } : (Color){ 128, 115, 96, 255 };
        Color wireColor = (Color){ 65, 55, 45, 255 };

        DrawCube(tilePos, roadWidth, 1.0f, tileLength, tileColor);
        DrawCubeWires(tilePos, roadWidth, 1.0f, tileLength, wireColor);

        // Ancient Gold Border Trims & Side Curbs
        Vector3 leftCurb = { -roadHalfWidth - 0.45f, 0.35f, tileZ };
        Vector3 rightCurb = { roadHalfWidth + 0.45f, 0.35f, tileZ };
        Color curbColor = (Color){ 75, 70, 62, 255 };
        Color goldTrimColor = (Color){ 195, 145, 50, 255 };

        DrawCube(leftCurb, 0.9f, 0.7f, tileLength, curbColor);
        DrawCubeWires(leftCurb, 0.9f, 0.7f, tileLength, goldTrimColor);

        DrawCube(rightCurb, 0.9f, 0.7f, tileLength, curbColor);
        DrawCubeWires(rightCurb, 0.9f, 0.7f, tileLength, goldTrimColor);
    }

    // --- 2. Tile Seams & Lane Seams ---
    float startSeamZ = floorf((playerZ - 30.0f) / 6.0f) * 6.0f;
    float endSeamZ = playerZ + 220.0f;
    for (float z = startSeamZ; z <= endSeamZ; z += 6.0f) {
        if (z < 0.0f) continue;
        Color seamColor = (fmodf(z, 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
        DrawLine3D((Vector3){ -roadHalfWidth, 0.015f, z }, (Vector3){ roadHalfWidth, 0.015f, z }, seamColor);
    }

    // Lane division lines (Visual guidance)
    for (float z = startSeamZ; z <= endSeamZ; z += 12.0f) {
        if (z < 0.0f) continue;
        DrawLine3D((Vector3){ -1.4f, 0.015f, z }, (Vector3){ -1.4f, 0.015f, z + 6.0f }, (Color){ 100, 90, 75, 120 });
        DrawLine3D((Vector3){ 1.4f, 0.015f, z }, (Vector3){ 1.4f, 0.015f, z + 6.0f }, (Color){ 100, 90, 75, 120 });
    }

    // --- 3. Render Continuous Animated Canyon River & Sandy Banks (Right Side X = +8.0m to +13.5m) ---
    DrawRiverAndBanks(playerZ, (float)GetTime());

    // --- 4. Rendering Large Continuous Mountain Valley Formations on Both Sides ---
    if (!valley.isLoaded) return;

    const float segLength = valley.mountainSize.z;
    int baseChunk = (int)(playerZ / segLength);

    // Draw 3 continuous repeating mountain chunks along Z (covering playerZ - 60m to playerZ + 260m)
    for (int c = -1; c <= 2; c++) {
        float chunkZ = (baseChunk + c) * segLength;

        // Position Left Mountain Range (X centered at valley.leftPosX, guaranteed max mesh X <= -15.0m)
        Vector3 leftPos = { valley.leftPosX, 0.0f, chunkZ };
        DrawModel(valley.leftMountainModel, leftPos, 1.0f, WHITE);

        // Position Right Mountain Range (X centered at valley.rightPosX, guaranteed min mesh X >= +15.0m)
        Vector3 rightPos = { valley.rightPosX, 0.0f, chunkZ };
        DrawModel(valley.rightMountainModel, rightPos, 1.0f, WHITE);
    }
}

int main() {
    // ----------------------------------------------------------------------------------
    // Initialization
    // ----------------------------------------------------------------------------------
    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "3D Temple Runner - Mountain Valley Canyon");

    // Load Rigged 3D Humanoid Character System with assets/player/character.glb
    PlayerCharacter player;
    const char* relativeModelPath = "assets/player/character.glb";
    if (!InitPlayerCharacter(player, relativeModelPath)) {
        TraceLog(LOG_ERROR, "FATAL ERROR: Could not load 3D rigged humanoid character from %s", relativeModelPath);
    }

    // Generate Natural 3D Mountain Valley Canyon System
    MountainValleySystem valley;
    InitMountainValleySystem(valley);

    const float forwardSpeed = 14.0f;   // Auto forward running velocity (units/sec)

    // Vertical Jump Physics Constants (Smooth ~0.87s arc, clears 2.0m obstacles)
    const float jumpVelocity = 10.5f;   // Vertical launch velocity
    const float gravity      = -24.0f;  // Downward gravity acceleration

    // Road parameters (Temple Run Stone Path)
    const float roadWidth = 10.0f;
    const float roadHalfWidth = roadWidth / 2.0f;

    // Discrete 3-Lane System Coordinates (Camera view: +X is visually LEFT, -X is visually RIGHT)
    const float laneX[3] = { 2.8f, 0.0f, -2.8f }; // 0: Visually Left (+2.8f), 1: Center (0.0f), 2: Visually Right (-2.8f)
    const float laneSlideSpeed = 16.0f;          // Exponential slide interpolation rate (~0.20s)

    // Obstacle Spawning & Tuning Constants
    const Vector3 obstacleSize = { 2.2f, 2.0f, 2.0f };
    const float minObstacleSpacing = 28.0f; // Z distance between obstacles
    const float spawnAheadDistance = 250.0f; // Distance ahead of player to generate
    const float initialSpawnZ = 50.0f;      // Start spawning obstacles after a safe runway

    std::vector<Obstacle> obstacles;
    float nextSpawnZ = initialSpawnZ;

    // Coin Spawning & Tuning Constants
    const float coinRadius = 0.6f;
    const float coinHeight = 0.15f;
    const float coinFloatHeight = 1.0f;
    const float initialCoinSpawnZ = 30.0f;

    std::vector<Coin> coins;
    float nextCoinSpawnZ = initialCoinSpawnZ;
    int score = 0;

    // Game state flags
    bool isGameOver = false;

    // Reset game helper lambda
    auto ResetGame = [&]() {
        player.position = (Vector3){ 0.0f, 0.0f, 0.0f };
        player.currentLane = 1; // Center lane
        player.targetX = 0.0f;
        player.verticalVelocity = 0.0f;
        player.isGrounded = true;
        player.animTime = 0.0f;
        obstacles.clear();
        coins.clear();
        nextSpawnZ = initialSpawnZ;
        nextCoinSpawnZ = initialCoinSpawnZ;
        score = 0;
        isGameOver = false;
    };

    // 3D Third-person camera setup following the 3D runner
    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 50.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    SetTargetFPS(60);

    // ----------------------------------------------------------------------------------
    // Main Game Loop
    // ----------------------------------------------------------------------------------
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        // Restart input listener (R key restarts ONLY when in Game Over state)
        if (isGameOver && IsKeyPressed(KEY_R)) {
            ResetGame();
        }

        if (!isGameOver) {
            // --- 1. Forward Speed & Lane Switching Input ---
            player.position.z += forwardSpeed * deltaTime;

            // Discrete 3-Lane Input Switching (IsKeyPressed prevents holding auto-repeat)
            if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
                if (player.currentLane > 0) {
                    player.currentLane--;
                    player.targetX = laneX[player.currentLane];
                }
            }

            if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
                if (player.currentLane < 2) {
                    player.currentLane++;
                    player.targetX = laneX[player.currentLane];
                }
            }

            // Smooth Lateral Interpolation toward Target Lane X Position (~0.20s slide)
            player.position.x = Lerp(player.position.x, player.targetX, laneSlideSpeed * deltaTime);
            if (fabsf(player.targetX - player.position.x) < 0.005f) {
                player.position.x = player.targetX;
            }

            // --- Vertical Jump Physics (SPACE Key) ---
            if (player.isGrounded && IsKeyPressed(KEY_SPACE)) {
                player.verticalVelocity = jumpVelocity;
                player.isGrounded = false;
            }

            if (!player.isGrounded) {
                player.verticalVelocity += gravity * deltaTime;
                player.position.y += player.verticalVelocity * deltaTime;

                if (player.position.y <= 0.0f) {
                    player.position.y = 0.0f;
                    player.verticalVelocity = 0.0f;
                    player.isGrounded = true;
                }
            }

            // Update Skeletal Model Running Animation continuously (In-Place loop)
            UpdatePlayerAnimation(player, true, deltaTime);

            // --- 2. Dynamic Obstacle Spawning ---
            while (nextSpawnZ < player.position.z + spawnAheadDistance) {
                int laneIndex = GetRandomValue(0, 2);
                float posX = laneX[laneIndex];
                Vector3 obsPos = { posX, obstacleSize.y / 2.0f, nextSpawnZ };

                Obstacle obs;
                obs.position = obsPos;
                obs.size = obstacleSize;
                obs.color = (Color){ 180, 60, 50, 255 };      // Temple Stone Pillar / Wall
                obs.wireColor = (Color){ 240, 160, 40, 255 }; // Ancient Rune Gold Accent

                obstacles.push_back(obs);
                nextSpawnZ += minObstacleSpacing + (float)GetRandomValue(0, 12);
            }

            // --- 3. Dynamic Coin Sequence Spawning ---
            while (nextCoinSpawnZ < player.position.z + spawnAheadDistance) {
                int coinLane = GetRandomValue(0, 2);
                float coinX = laneX[coinLane];
                int coinSequenceCount = GetRandomValue(3, 5); // Sequence trail of coins
                float spacing = 4.5f;

                for (int c = 0; c < coinSequenceCount; c++) {
                    Coin coin;
                    coin.position = (Vector3){ coinX, coinFloatHeight, nextCoinSpawnZ + c * spacing };
                    coin.collected = false;
                    coin.rotation = (float)GetRandomValue(0, 360);
                    coins.push_back(coin);
                }

                nextCoinSpawnZ += coinSequenceCount * spacing + (float)GetRandomValue(15, 30);
            }

            // --- 4. Coin Rotation Animation & Collection Detection ---
            for (auto& coin : coins) {
                if (coin.collected) continue;

                coin.rotation += 150.0f * deltaTime;
                if (coin.rotation >= 360.0f) coin.rotation -= 360.0f;

                Vector3 currentCoinPos = coin.position;
                currentCoinPos.y += 0.15f * sinf(coin.rotation * (float)DEG2RAD * 2.0f);

                // Sphere-to-sphere collision check with runner body
                Vector3 playerCenter = { player.position.x, player.position.y + 1.0f, player.position.z };
                float hitRadius = player.collisionRadius + coinRadius;

                if (Vector3DistanceSqr(playerCenter, currentCoinPos) <= hitRadius * hitRadius) {
                    coin.collected = true;
                    score++;
                }
            }

            // --- 5. Garbage Collection / Memory Optimization ---
            for (size_t i = 0; i < obstacles.size(); ) {
                if (obstacles[i].position.z < player.position.z - 30.0f) {
                    obstacles.erase(obstacles.begin() + i);
                } else {
                    i++;
                }
            }

            for (size_t i = 0; i < coins.size(); ) {
                if (coins[i].collected || coins[i].position.z < player.position.z - 30.0f) {
                    coins.erase(coins.begin() + i);
                } else {
                    i++;
                }
            }

            // --- 6. Obstacle Collision Detection ---
            Vector3 playerCenter = { player.position.x, player.position.y + 1.0f, player.position.z };
            for (const auto& obs : obstacles) {
                BoundingBox obsBox = {
                    (Vector3){ obs.position.x - obs.size.x / 2.0f, obs.position.y - obs.size.y / 2.0f, obs.position.z - obs.size.z / 2.0f },
                    (Vector3){ obs.position.x + obs.size.x / 2.0f, obs.position.y + obs.size.y / 2.0f, obs.position.z + obs.size.z / 2.0f }
                };

                if (CheckCollisionSphereBox(playerCenter, player.collisionRadius, obsBox)) {
                    isGameOver = true;
                    player.verticalVelocity = 0.0f;
                    break;
                }
            }
        }

        // --- Third-Person Camera Tracking ---
        camera.position = (Vector3){
            player.position.x,
            player.position.y + 4.0f,
            player.position.z - 8.0f
        };

        camera.target = (Vector3){
            player.position.x,
            player.position.y + 1.0f,
            player.position.z + 8.0f
        };

        // ------------------------------------------------------------------------------
        // Draw / Render
        // ------------------------------------------------------------------------------
        BeginDrawing();
            // Atmosphere: Bright Warm Desert/Sandstone Canyon Sky Color
            ClearBackground((Color){ 135, 190, 225, 255 });

            BeginMode3D(camera);

                // --- Draw Natural 3D Mountain Valley Canyon Environment & Sandstone Road ---
                DrawMountainValleyEnvironment(valley, player.position.z, roadWidth);

                // --- Draw Spawning Gameplay Obstacles ---
                for (const auto& obs : obstacles) {
                    DrawCube(obs.position, obs.size.x, obs.size.y, obs.size.z, obs.color);
                    DrawCubeWires(obs.position, obs.size.x + 0.02f, obs.size.y + 0.02f, obs.size.z + 0.02f, obs.wireColor);
                }

                // --- Draw Rotating 3D Gold Coins ---
                for (const auto& coin : coins) {
                    if (coin.collected) continue;

                    float bobOffset = 0.15f * sinf(coin.rotation * (float)DEG2RAD * 2.0f);
                    Vector3 drawPos = { coin.position.x, coin.position.y + bobOffset, coin.position.z };

                    rlPushMatrix();
                        rlTranslatef(drawPos.x, drawPos.y, drawPos.z);
                        rlRotatef(coin.rotation, 0.0f, 1.0f, 0.0f);
                        rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);

                        DrawCylinder((Vector3){ 0, -coinHeight / 2.0f, 0 }, coinRadius, coinRadius, coinHeight, 16, (Color){ 255, 215, 0, 255 });
                        DrawCylinderWires((Vector3){ 0, -coinHeight / 2.0f, 0 }, coinRadius + 0.01f, coinRadius + 0.01f, coinHeight + 0.01f, 16, (Color){ 220, 140, 0, 255 });
                        DrawCircle3D((Vector3){ 0, coinHeight / 2.0f + 0.005f, 0 }, coinRadius * 0.55f, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){ 255, 245, 140, 255 });
                        DrawCircle3D((Vector3){ 0, -coinHeight / 2.0f - 0.005f, 0 }, coinRadius * 0.55f, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){ 255, 245, 140, 255 });
                    rlPopMatrix();

                    DrawCircle3D((Vector3){ drawPos.x, 0.02f, drawPos.z }, coinRadius * 0.85f, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){ 255, 215, 0, 70 });
                }

                // --- Draw Rigged 3D Humanoid Character (character.glb) ---
                DrawPlayerCharacter(player);

            EndMode3D();

            // --- HUD Overlay ---
            DrawRectangle(15, 15, 340, 202, Fade((Color){ 15, 18, 30, 255 }, 0.85f));
            DrawRectangleLines(15, 15, 340, 202, (Color){ 210, 150, 40, 255 });

            DrawText("TEMPLE RUNNER 3D", 30, 25, 20, (Color){ 240, 170, 50, 255 });
            DrawText("A / Left Arrow  : Switch Lane Left", 30, 52, 13, RAYWHITE);
            DrawText("D / Right Arrow : Switch Lane Right", 30, 70, 13, RAYWHITE);
            DrawText("SPACE Key       : Jump", 30, 88, 13, (Color){ 100, 240, 255, 255 });
            DrawText("R Key           : Restart (Game Over)", 30, 106, 13, (Color){ 200, 200, 220, 255 });

            DrawText(TextFormat("Distance: %.1f m", player.position.z), 30, 128, 16, YELLOW);
            DrawText(TextFormat("Coins: %d", score), 30, 150, 16, (Color){ 255, 215, 0, 255 });
            DrawText(TextFormat("Speed: %.1f m/s", isGameOver ? 0.0f : forwardSpeed), 30, 172, 16, isGameOver ? RED : GREEN);

            DrawFPS(GetScreenWidth() - 100, 20);

            // --- Game Over Modal Overlay ---
            if (isGameOver) {
                int bannerWidth = 480;
                int bannerHeight = 250;
                int bannerX = (GetScreenWidth() - bannerWidth) / 2;
                int bannerY = (GetScreenHeight() - bannerHeight) / 2;

                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));

                DrawRectangle(bannerX, bannerY, bannerWidth, bannerHeight, (Color){ 25, 28, 42, 245 });
                DrawRectangleLines(bannerX, bannerY, bannerWidth, bannerHeight, (Color){ 255, 60, 80, 255 });
                DrawRectangleLines(bannerX + 2, bannerY + 2, bannerWidth - 4, bannerHeight - 4, (Color){ 255, 140, 0, 255 });

                const char* titleText = "GAME OVER";
                int titleWidth = MeasureText(titleText, 40);
                DrawText(titleText, bannerX + (bannerWidth - titleWidth) / 2, bannerY + 25, 40, (Color){ 255, 60, 80, 255 });

                const char* scoreText = TextFormat("Distance Survived: %.1f m", player.position.z);
                int scoreWidth = MeasureText(scoreText, 22);
                DrawText(scoreText, bannerX + (bannerWidth - scoreWidth) / 2, bannerY + 85, 22, YELLOW);

                const char* coinText = TextFormat("Coins Collected: %d", score);
                int coinWidth = MeasureText(coinText, 22);
                DrawText(coinText, bannerX + (bannerWidth - coinWidth) / 2, bannerY + 115, 22, (Color){ 255, 215, 0, 255 });

                const char* restartText = "Press [ R ] to Restart";
                int restartWidth = MeasureText(restartText, 20);
                DrawText(restartText, bannerX + (bannerWidth - restartWidth) / 2, bannerY + 175, 20, (Color){ 0, 230, 255, 255 });
            }

        EndDrawing();
    }

    // Unload 3D Models and Resources
    UnloadMountainValleySystem(valley);
    UnloadPlayerCharacter(player);
    CloseWindow();

    return 0;
}
