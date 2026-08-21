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
    int currentLane;      // 0 = Left (-2.8f), 1 = Center (0.0f), 2 = Right (2.8f)
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

int main() {
    // ----------------------------------------------------------------------------------
    // Initialization
    // ----------------------------------------------------------------------------------
    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "3D Temple Runner - Raylib");

    // Load Rigged 3D Humanoid Character System with assets/player/character.glb
    PlayerCharacter player;
    const char* relativeModelPath = "assets/player/character.glb";
    if (!InitPlayerCharacter(player, relativeModelPath)) {
        TraceLog(LOG_ERROR, "FATAL ERROR: Could not load 3D rigged humanoid character from %s", relativeModelPath);
    }

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
            ClearBackground((Color){ 25, 30, 38, 255 });

            BeginMode3D(camera);

                // --- Draw Temple Run Stone Path & Ruin Walls ---
                const float segmentLength = 200.0f;
                int baseSegmentIndex = (int)(player.position.z / segmentLength);

                for (int i = -1; i <= 3; i++) {
                    float segZ = (baseSegmentIndex + i) * segmentLength + segmentLength / 2.0f;
                    Vector3 segCenter = { 0.0f, -0.5f, segZ };

                    // Temple Sandstone Road Floor
                    DrawCube(segCenter, roadWidth, 1.0f, segmentLength, (Color){ 120, 110, 95, 255 });
                    DrawCubeWires(segCenter, roadWidth, 1.0f, segmentLength, (Color){ 85, 75, 65, 255 });

                    // Ancient Mossy Ruin Walls / Side Rails
                    Vector3 leftWall = { -roadHalfWidth - 0.40f, 0.40f, segZ };
                    Vector3 rightWall = { roadHalfWidth + 0.40f, 0.40f, segZ };
                    DrawCube(leftWall, 0.8f, 0.8f, segmentLength, (Color){ 65, 80, 60, 255 });
                    DrawCubeWires(leftWall, 0.8f, 0.8f, segmentLength, (Color){ 45, 60, 40, 255 });

                    DrawCube(rightWall, 0.8f, 0.8f, segmentLength, (Color){ 65, 80, 60, 255 });
                    DrawCubeWires(rightWall, 0.8f, 0.8f, segmentLength, (Color){ 45, 60, 40, 255 });
                }

                // --- Draw Temple Stone Tile Seams ---
                float startGridZ = floorf((player.position.z - 20.0f) / 5.0f) * 5.0f;
                float endGridZ = player.position.z + 180.0f;
                for (float z = startGridZ; z <= endGridZ; z += 5.0f) {
                    if (z < 0.0f) continue;
                    Color lineCol = (fmodf(z, 20.0f) == 0.0f) ? (Color){ 210, 160, 70, 220 } : (Color){ 80, 70, 60, 160 };
                    DrawLine3D((Vector3){ -roadHalfWidth, 0.01f, z }, (Vector3){ roadHalfWidth, 0.01f, z }, lineCol);
                }

                // --- Draw Spawning Obstacles ---
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

    // Unload 3D Model and Resources
    UnloadPlayerCharacter(player);
    CloseWindow();

    return 0;
}
