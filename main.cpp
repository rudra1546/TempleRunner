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

// Mountain Valley Mass System state
struct MountainValleySystem {
    bool isLoaded;
    float segLength;
};

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

EM_JS(void, InitWebOrientationBridge, (), {
    window.floodrunner_gamma = 0.0;
    window.floodrunner_raw_gamma = 0.0;
    window.floodrunner_raw_beta = 0.0;
    window.floodrunner_screen_angle = 0;
    window.floodrunner_event_count = 0;
    window.floodrunner_axis_code = 0; // 0: gamma (portrait), 1: beta (landscape 90), 2: -beta (landscape 270), 3: -gamma (portrait 180), 4: accel-fallback
    window.floodrunner_permission_status = 0; // 0: unrequested, 1: granted, 2: denied, 3: unavailable
    window.floodrunner_motion_supported = (typeof window !== 'undefined' && ('DeviceOrientationEvent' in window || 'DeviceMotionEvent' in window));

    if (!window.floodrunner_motion_supported) {
        window.floodrunner_permission_status = 3;
    }

    window.onOrientationEvent = function(e) {
        if (!e) return;
        window.floodrunner_event_count++;
        var g = (e.gamma !== null && e.gamma !== undefined) ? e.gamma : 0.0;
        var b = (e.beta !== null && e.beta !== undefined) ? e.beta : 0.0;
        window.floodrunner_raw_gamma = g;
        window.floodrunner_raw_beta = b;

        var angle = 0;
        if (screen.orientation && typeof screen.orientation.angle === 'number') {
            angle = screen.orientation.angle;
        } else if (typeof window.orientation === 'number') {
            angle = window.orientation;
        }
        window.floodrunner_screen_angle = angle;

        var tilt = g;
        var code = 0;
        if (angle === 90) {
            tilt = b;
            code = 1;
        } else if (angle === -90 || angle === 270) {
            tilt = -b;
            code = 2;
        } else if (angle === 180) {
            tilt = -g;
            code = 3;
        } else {
            tilt = g;
            code = 0;
        }
        window.floodrunner_axis_code = code;
        window.floodrunner_gamma = tilt;
    };

    window.onMotionEvent = function(e) {
        if (!e || !e.accelerationIncludingGravity) return;
        var ax = e.accelerationIncludingGravity.x || 0;
        var ay = e.accelerationIncludingGravity.y || 0;
        
        // If deviceorientation hasn't fired or is producing zero, fallback to gravity accelerometer tilt
        if (window.floodrunner_event_count === 0 || (window.floodrunner_raw_gamma === 0 && window.floodrunner_raw_beta === 0)) {
            window.floodrunner_event_count++;
            var angle = window.floodrunner_screen_angle;
            var tilt = -(ax / 9.8) * 35.0; // map gravity vector to degrees
            if (angle === 90) {
                tilt = (ay / 9.8) * 35.0;
            } else if (angle === -90 || angle === 270) {
                tilt = -(ay / 9.8) * 35.0;
            }
            window.floodrunner_axis_code = 4;
            window.floodrunner_gamma = tilt;
        }
    };

    window.attachMotionListeners = function() {
        try {
            window.addEventListener('deviceorientation', window.onOrientationEvent, true);
            window.addEventListener('deviceorientationabsolute', window.onOrientationEvent, true);
            window.addEventListener('devicemotion', window.onMotionEvent, true);
        } catch (err) {
            console.warn("Error attaching motion listeners:", err);
        }
    };

    window.requestFloodRunnerMotion = function() {
        if (!window.floodrunner_motion_supported) {
            window.floodrunner_permission_status = 3;
            return;
        }

        if (typeof DeviceOrientationEvent !== 'undefined' && typeof DeviceOrientationEvent.requestPermission === 'function') {
            DeviceOrientationEvent.requestPermission()
                .then(function(state) {
                    if (state === 'granted') {
                        window.floodrunner_permission_status = 1;
                        window.attachMotionListeners();
                        var btn = document.getElementById('floodrunner-motion-btn');
                        if (btn) {
                            btn.innerText = 'Gyro: Active';
                            btn.style.backgroundColor = 'rgba(20, 140, 60, 0.9)';
                            btn.style.borderColor = '#40ff80';
                            setTimeout(function() { if (btn) btn.style.display = 'none'; }, 2500);
                        }
                    } else {
                        window.floodrunner_permission_status = 2;
                        var btn = document.getElementById('floodrunner-motion-btn');
                        if (btn) {
                            btn.innerText = 'Gyro: Denied';
                            btn.style.backgroundColor = 'rgba(160, 30, 30, 0.9)';
                            btn.style.borderColor = '#ff6060';
                        }
                    }
                })
                .catch(function(err) {
                    console.warn("DeviceOrientation error:", err);
                    window.floodrunner_permission_status = 2;
                });
        } else {
            // Standard non-iOS browsers (e.g. Android Chrome)
            window.floodrunner_permission_status = 1;
            window.attachMotionListeners();
            var btn = document.getElementById('floodrunner-motion-btn');
            if (btn) {
                btn.innerText = 'Gyro: Active';
                btn.style.backgroundColor = 'rgba(20, 140, 60, 0.9)';
                setTimeout(function() { if (btn) btn.style.display = 'none'; }, 2500);
            }
        }
    };

    // Create a prominent DOM overlay button directly on document.body for iOS permission user gesture
    var setupDOMButton = function() {
        if (typeof document === 'undefined' || !document.body) {
            setTimeout(setupDOMButton, 100);
            return;
        }

        var existingBtn = document.getElementById('floodrunner-motion-btn');
        if (!existingBtn) {
            var btn = document.createElement('button');
            btn.id = 'floodrunner-motion-btn';
            btn.innerHTML = '&#x1F4F1; Enable Gyro / Tilt Controls';
            btn.style.position = 'fixed';
            btn.style.top = '14px';
            btn.style.right = '14px';
            btn.style.zIndex = '999999';
            btn.style.padding = '12px 18px';
            btn.style.fontSize = '14px';
            btn.style.fontWeight = 'bold';
            btn.style.fontFamily = 'Arial, sans-serif';
            btn.style.color = '#ffffff';
            btn.style.backgroundColor = 'rgba(15, 75, 160, 0.92)';
            btn.style.border = '2px solid #60b0ff';
            btn.style.borderRadius = '8px';
            btn.style.boxShadow = '0 4px 14px rgba(0,0,0,0.6)';
            btn.style.cursor = 'pointer';
            btn.style.userSelect = 'none';
            btn.style.webkitUserSelect = 'none';

            var directHandler = function(e) {
                if (e) {
                    e.preventDefault();
                    e.stopPropagation();
                }
                window.requestFloodRunnerMotion();
            };

            btn.addEventListener('click', directHandler, { passive: false });
            btn.addEventListener('touchend', directHandler, { passive: false });

            document.body.appendChild(btn);
        }

        // Global touch handler on document to capture first touch gesture
        var docTouchHandler = function() {
            if (window.floodrunner_permission_status === 0) {
                window.requestFloodRunnerMotion();
            }
            document.removeEventListener('touchend', docTouchHandler);
        };
        document.addEventListener('touchend', docTouchHandler, { passive: true });
    };

    setupDOMButton();

    // Auto-register on platforms that do not require explicit requestPermission
    if (window.floodrunner_motion_supported && typeof DeviceOrientationEvent.requestPermission !== 'function') {
        window.requestFloodRunnerMotion();
    }
});

EM_JS(float, GetWebDeviceTilt, (), {
    return (typeof window.floodrunner_gamma === 'number') ? window.floodrunner_gamma : 0.0;
});

EM_JS(float, GetWebRawGamma, (), {
    return (typeof window.floodrunner_raw_gamma === 'number') ? window.floodrunner_raw_gamma : 0.0;
});

EM_JS(float, GetWebRawBeta, (), {
    return (typeof window.floodrunner_raw_beta === 'number') ? window.floodrunner_raw_beta : 0.0;
});

EM_JS(int, GetWebScreenAngle, (), {
    return (typeof window.floodrunner_screen_angle === 'number') ? window.floodrunner_screen_angle : 0;
});

EM_JS(int, GetWebSelectedAxisCode, (), {
    return (typeof window.floodrunner_axis_code === 'number') ? window.floodrunner_axis_code : 0;
});

EM_JS(bool, IsWebMotionSupported, (), {
    return !!window.floodrunner_motion_supported;
});

EM_JS(int, GetWebPermissionStatus, (), {
    return (typeof window.floodrunner_permission_status === 'number') ? window.floodrunner_permission_status : 0;
});

EM_JS(int, GetWebOrientationEventCount, (), {
    return (typeof window.floodrunner_event_count === 'number') ? window.floodrunner_event_count : 0;
});

EM_JS(void, RequestDOMMotionPermission, (), {
    if (window.requestFloodRunnerMotion) {
        window.requestFloodRunnerMotion();
    }
});
#endif

// ==============================================================================
// Web & Mobile Input System (Keyboard + Touch Drag + Gyroscope Orientation)
// ==============================================================================

struct GyroSteeringSystem {
    float sensitivity = 1.5f;       // Configurable device tilt multiplier
    float deadzone = 0.08f;         // Neutral zone to eliminate noise when device is held flat
    float filteredInput = 0.0f;     // Low-pass filtered normalized lateral input [-1.0f, +1.0f]
    float filterSpeed = 15.0f;      // Responsiveness factor for sensor smoothing
    bool isAvailable = false;       // Set true if physical mobile sensor is active
};

struct TouchSteeringState {
    bool isTouching = false;
    Vector2 touchStartPos = { 0.0f, 0.0f };
    Vector2 currentPos = { 0.0f, 0.0f };
    float lateralInput = 0.0f;      // [-1.0f (Left), +1.0f (Right)]
    bool jumpTriggered = false;
    bool tapRestartTriggered = false;
};

// Keyboard lateral steering: -1.0f (Left) to +1.0f (Right)
float GetKeyboardSteering() {
    float steer = 0.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) steer -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) steer += 1.0f;
    return steer;
}

// Touch lateral drag/hold steering: -1.0f (Left) to +1.0f (Right)
float UpdateTouchSteering(TouchSteeringState& touch, bool isGameOver) {
    bool isDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || (GetTouchPointCount() > 0);
    bool isPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    Vector2 pos = GetMousePosition();
    if (GetTouchPointCount() > 0) {
        pos = GetTouchPosition(0);
    }

    touch.jumpTriggered = false;
    touch.tapRestartTriggered = false;

    if (isPressed) {
        touch.isTouching = true;
        touch.touchStartPos = pos;
        touch.currentPos = pos;

#if defined(__EMSCRIPTEN__)
        // Trigger motion permission on user tap (for non-iOS or fallback)
        RequestDOMMotionPermission();
#endif

        if (isGameOver) {
            touch.tapRestartTriggered = true;
        } else {
            // Tap top 35% of screen to jump
            if (pos.y < (float)GetScreenHeight() * 0.35f) {
                touch.jumpTriggered = true;
            }
        }
    }

    if (isDown && touch.isTouching) {
        touch.currentPos = pos;
        float deltaX = pos.x - touch.touchStartPos.x;
        float dragThreshold = (float)GetScreenWidth() * 0.10f; // 10% screen width drag = full deflection

        if (fabsf(deltaX) > 4.0f) {
            touch.lateralInput = Clamp(deltaX / dragThreshold, -1.0f, 1.0f);
        } else {
            // Direct screen half hold fallback if not dragging
            float screenCenter = (float)GetScreenWidth() * 0.5f;
            float distFromCenter = (pos.x - screenCenter) / (screenCenter * 0.75f);
            touch.lateralInput = Clamp(distFromCenter, -1.0f, 1.0f);
        }
    } else {
        touch.isTouching = false;
        touch.lateralInput = 0.0f;
    }

    return touch.lateralInput;
}

// Platform-independent gyro/device orientation sensor reading abstraction
float GetGyroSteeringInput() {
#if defined(__EMSCRIPTEN__)
    float tilt = GetWebDeviceTilt();
    // tilt is left/right tilt angle in degrees (-90 to +90)
    // ~25 degrees tilt corresponds to full lateral steering
    return Clamp(tilt / 25.0f, -1.0f, 1.0f);
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__)
    return 0.0f;
#elif defined(PLATFORM_IOS) || defined(__APPLE__)
    return 0.0f;
#else
    return 0.0f;
#endif
}

// Update filtered gyro lateral steering value each frame
// Output: normalized lateral steering in range [-1.0f (Left), +1.0f (Right)]
float UpdateGyroSteering(GyroSteeringSystem& gyro, float deltaTime) {
    float rawInput = GetGyroSteeringInput();
    
    // Check deadzone
    if (fabsf(rawInput) < gyro.deadzone) {
        rawInput = 0.0f;
    } else {
        // Remap output past deadzone smoothly to [0, 1]
        float sign = (rawInput > 0.0f) ? 1.0f : -1.0f;
        rawInput = sign * (fabsf(rawInput) - gyro.deadzone) / (1.0f - gyro.deadzone);
    }

    // Apply sensitivity
    rawInput = Clamp(rawInput * gyro.sensitivity, -1.0f, 1.0f);

    // Low-pass exponential smoothing filter to eliminate sensor jitter
    float filterAlpha = Clamp(gyro.filterSpeed * deltaTime, 0.0f, 1.0f);
    gyro.filteredInput = Lerp(gyro.filteredInput, rawInput, filterAlpha);

    return gyro.filteredInput;
}

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

// Initialize Mountain Environment System (Mountain geometry removed for open map visibility)
void InitMountainValleySystem(MountainValleySystem& valley) {
    valley.segLength = 160.0f;
    valley.isLoaded = false;
}

// Unload Mountain Valley Models
void UnloadMountainValleySystem(MountainValleySystem& valley) {
    valley.isLoaded = false;
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

// Helper: Fast Deterministic Pseudorandom Generator for Segment Decoration
inline unsigned int HashSegment(int segmentIndex, unsigned int salt) {
    unsigned int h = (unsigned int)segmentIndex * 2654435761u + salt * 1013904223u;
    h ^= (h >> 16);
    h *= 0x85ebca6bu;
    h ^= (h >> 13);
    return h;
}

// Helper function to render a road curb with gold trim wireframe
void DrawCurb(Vector3 pos, Vector3 size) {
    Color curbColor = (Color){ 75, 70, 62, 255 };
    Color goldTrimColor = (Color){ 195, 145, 50, 255 };
    DrawCube(pos, size.x, size.y, size.z, curbColor);
    DrawCubeWires(pos, size.x, size.y, size.z, goldTrimColor);
}

// Helper function to render a single sandstone road tile
void DrawTileSurface(Vector3 centerPos, float widthX, float widthZ, int index) {
    Color tileColor = (index % 2 == 0) ? (Color){ 145, 130, 110, 255 } : (Color){ 128, 115, 96, 255 };
    Color wireColor = (Color){ 65, 55, 45, 255 };
    DrawCube(centerPos, widthX, 1.0f, widthZ, tileColor);
    DrawCubeWires(centerPos, widthX, 1.0f, widthZ, wireColor);
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

// Helper function to check if player position is on solid road ground for 90-Degree L-Shaped Map
inline bool IsOnRoadSurface(Vector3 pos) {
    // 1. Straight Road Segment (0m <= Z <= 503m, X centered at 0 within 3.5m)
    if (pos.z >= -10.0f && pos.z <= 503.0f) {
        if (pos.x >= -3.5f && pos.x <= 3.5f) return true;
    }
    // 2. 90-Degree Turn Corner & Turned Horizontal Path (Z approx 500m, X extending along +X from -3.5m to 350m)
    if (pos.z >= 496.5f && pos.z <= 503.5f) {
        if (pos.x >= -3.5f && pos.x <= 350.0f) return true;
    }
    return false;
}

// Render Continuous Animated Canyon River & Sandy Banks
void DrawRiverAndBanks(float playerZ, float time) {
    const float riverWidth = 5.5f;

    // --- 1. Natural Sandy / Wet Riverbed Banks ---
    const float segLength = 6.0f;
    int startSeg = (int)((playerZ - 40.0f) / segLength);
    int endSeg   = (int)((playerZ + 240.0f) / segLength);

    for (int i = startSeg; i <= endSeg; i++) {
        float segZ = i * segLength + segLength / 2.0f;
        if (segZ > 505.0f) continue; // End river at turn boundary

        float riverStartX = -8.0f;
        float riverEndX   = riverStartX - riverWidth;

        // Sandy shore bank between right road curb and river
        Vector3 bankPos = { -6.7f, -0.2f, segZ };
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
        if (z1 > 505.0f) continue;

        float riverStartX1 = -8.0f;
        float riverEndX1   = riverStartX1 - riverWidth;
        float riverStartX2 = -8.0f;
        float riverEndX2   = riverStartX2 - riverWidth;

        float waveY1 = 0.04f + 0.035f * sinf(z1 * 0.25f + time * 3.2f);
        float waveY2 = 0.04f + 0.035f * sinf(z2 * 0.25f + time * 3.2f);

        Vector3 p1 = { riverStartX1, waveY1, z1 };
        Vector3 p2 = { riverEndX1,   waveY1, z1 };
        Vector3 p3 = { riverEndX2,   waveY2, z2 };
        Vector3 p4 = { riverStartX2, waveY2, z2 };

        Color waterCol = (Color){ 30, 125, 165, 220 };
        DrawQuad3D(p1, p2, p3, p4, waterCol);

        float midX1 = riverStartX1 - 1.2f - 0.5f * sinf(z1 * 0.4f + time * 2.0f);
        float midX2 = riverStartX2 - 3.8f - 0.5f * cosf(z2 * 0.4f + time * 2.0f);

        Vector3 r1 = { midX1, waveY1 + 0.005f, z1 };
        Vector3 r2 = { midX1 - 2.5f, waveY1 + 0.005f, z1 };
        Vector3 r3 = { midX2 - 2.5f, waveY2 + 0.005f, z2 };
        Vector3 r4 = { midX2, waveY2 + 0.005f, z2 };

        Color rippleCol = (Color){ 100, 210, 240, 180 };
        DrawQuad3D(r1, r2, r3, r4, rippleCol);
    }
}

// Render Clean 90-Degree L-Shaped Road Environment (Zero Overlaps, Zero Obstructions, Zero Trees)
void DrawMountainValleyEnvironment(const MountainValleySystem& valley, float playerZ, float roadWidth) {
    const float narrowPathWidth = 6.0f; // 6.0m path width (X in [-3.0, 3.0] on straight, Z in [497.0, 503.0] on turned)
    const float tileLength = 6.0f;

    // --- 1. Straight Road Segment (0m <= Z < 497.0m) ---
    float minRenderZ = fmaxf(0.0f, playerZ - 40.0f);
    float maxRenderZ = fminf(497.0f, playerZ + 250.0f);

    for (float z = 0.0f; z < 497.0f; z += tileLength) {
        float nextZ = fminf(z + tileLength, 497.0f);
        float len = nextZ - z;
        float centerZ = z + len / 2.0f;

        if (centerZ + len / 2.0f < minRenderZ || centerZ - len / 2.0f > maxRenderZ) continue;

        int tileIdx = (int)(z / tileLength);
        DrawTileSurface((Vector3){ 0.0f, -0.5f, centerZ }, narrowPathWidth, len, tileIdx);

        // West curb (Right side when running +Z, X = -3.45m)
        DrawCurb((Vector3){ -3.45f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });

        // East curb (Left side when running +Z, X = +3.45m) - stops cleanly at Z = 497.0m!
        DrawCurb((Vector3){ 3.45f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });
    }

    // --- 2. Clean 90-Degree Turn Corner Tile (Z in [497.0m, 503.0m], X in [-3.0m, 3.0m]) ---
    if (playerZ + 250.0f >= 497.0f) {
        // Corner road surface (seamlessly joins straight road at Z=497 and turned road at X=3.0)
        DrawTileSurface((Vector3){ 0.0f, -0.5f, 500.0f }, narrowPathWidth, narrowPathWidth, 83);

        // West Curb continuation (X = -3.45m, Z from 497 to 503)
        DrawCurb((Vector3){ -3.45f, 0.35f, 500.0f }, (Vector3){ 0.9f, 0.7f, 6.0f });

        // North Back Wall Curb (Z = 503.45m, X from -3.9m to 3.0m)
        DrawCurb((Vector3){ -0.45f, 0.35f, 503.45f }, (Vector3){ 6.9f, 0.7f, 0.9f });
        // (East side at X=+3.0m and South side at Z=497.0m are wide open for turning!)
    }

    // --- 3. Turned Horizontal Road Segment (X from 3.0m to 350.0m, centered at Z = 500.0m) ---
    if (playerZ + 250.0f >= 480.0f) {
        for (float x = 3.0f; x <= 350.0f; x += tileLength) {
            float nextX = fminf(x + tileLength, 350.0f);
            float len = nextX - x;
            float centerX = x + len / 2.0f;
            int tileIdx = (int)(x / tileLength);

            // Road surface
            DrawTileSurface((Vector3){ centerX, -0.5f, 500.0f }, len, narrowPathWidth, tileIdx);

            // North Curb (Z = 503.45m)
            DrawCurb((Vector3){ centerX, 0.35f, 503.45f }, (Vector3){ len, 0.7f, 0.9f });

            // South Curb (Z = 496.55m)
            DrawCurb((Vector3){ centerX, 0.35f, 496.55f }, (Vector3){ len, 0.7f, 0.9f });
        }
    }

    // --- 4. Tile Seams ---
    float startSeamZ = floorf((playerZ - 30.0f) / 6.0f) * 6.0f;
    float endSeamZ = fminf(playerZ + 220.0f, 497.0f);
    for (float z = startSeamZ; z <= endSeamZ; z += 6.0f) {
        if (z < 0.0f || z > 497.0f) continue;
        Color seamColor = (fmodf(z, 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
        DrawLine3D((Vector3){ -narrowPathWidth / 2.0f, 0.015f, z }, (Vector3){ narrowPathWidth / 2.0f, 0.015f, z }, seamColor);
    }

    if (playerZ + 250.0f >= 480.0f) {
        for (float x = 3.0f; x <= 350.0f; x += 6.0f) {
            Color seamColor = (fmodf(x, 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
            DrawLine3D((Vector3){ x, 0.015f, 497.0f }, (Vector3){ x, 0.015f, 503.0f }, seamColor);
        }
    }
}

struct GameState {
    PlayerCharacter player;
    MountainValleySystem valley;
    GyroSteeringSystem gyro;
    TouchSteeringState touch;

    float forwardSpeed = 14.0f;
    float jumpVelocity = 10.5f;
    float gravity = -24.0f;
    float roadWidth = 10.0f;
    float horizontalSpeed = 10.0f;

    float initialSpawnZ = 50.0f;
    float nextSpawnZ = 50.0f;
    std::vector<Obstacle> obstacles;

    float coinRadius = 0.6f;
    float coinHeight = 0.15f;
    float coinFloatHeight = 1.0f;
    float initialCoinSpawnZ = 30.0f;
    float nextCoinSpawnZ = 30.0f;
    std::vector<Coin> coins;

    int score = 0;
    bool isGameOver = false;
    Camera3D camera;

    // Diagnostics telemetry
    float lastKeyboardSteer = 0.0f;
    float lastTouchSteer = 0.0f;
    float lastRawGyroSteer = 0.0f;
    float lastGyroSteer = 0.0f;
    float lastLateralSteer = 0.0f;

    void Reset() {
        player.position = (Vector3){ 0.0f, 0.0f, 0.0f };
        player.rotationY = 0.0f;
        player.verticalVelocity = 0.0f;
        player.isGrounded = true;
        player.animTime = 0.0f;
        gyro.filteredInput = 0.0f;
        touch.lateralInput = 0.0f;
        lastKeyboardSteer = 0.0f;
        lastTouchSteer = 0.0f;
        lastRawGyroSteer = 0.0f;
        lastGyroSteer = 0.0f;
        lastLateralSteer = 0.0f;
        obstacles.clear();
        coins.clear();
        nextSpawnZ = initialSpawnZ;
        nextCoinSpawnZ = initialCoinSpawnZ;
        score = 0;
        isGameOver = false;
    }
};

static GameState g_game;

void UpdateDrawFrame() {
    GameState& g = g_game;
    float deltaTime = GetFrameTime();
    if (deltaTime > 0.1f) deltaTime = 0.1f; // Cap delta time on slow frame or tab switch

    // Always update and track gyro input even if game is over
    float rawGyro = GetGyroSteeringInput();
    float gyroSteer = UpdateGyroSteering(g.gyro, deltaTime);
    float keyboardSteer = GetKeyboardSteering();
    float touchSteer = 0.0f;

    // Restart listener (R key or touch tap when Game Over)
    if (g.isGameOver) {
        touchSteer = UpdateTouchSteering(g.touch, true);
        if (IsKeyPressed(KEY_R) || g.touch.tapRestartTriggered) {
            g.Reset();
        }
    } else {
        touchSteer = UpdateTouchSteering(g.touch, false);
    }

    // Combined lateral steering: -1.0f (Left) to +1.0f (Right)
    float lateralSteer = Clamp(keyboardSteer + touchSteer + gyroSteer, -1.0f, 1.0f);

    g.lastKeyboardSteer = keyboardSteer;
    g.lastTouchSteer = touchSteer;
    g.lastRawGyroSteer = rawGyro;
    g.lastGyroSteer = gyroSteer;
    g.lastLateralSteer = lateralSteer;

    if (!g.isGameOver) {
        // 2. Road Progression & Direction Control
        bool isOnTurnedPath = (g.player.position.x >= 3.0f && g.player.position.z >= 495.0f);

        if (!isOnTurnedPath) {
            // Running forward along +Z on Straight Road (0m -> 500m)
            g.player.position.z += g.forwardSpeed * deltaTime;

            // On Straight Road (+Z): Steer Left is +X, Steer Right is -X
            g.player.position.x += (-lateralSteer) * g.horizontalSpeed * deltaTime;

            // Constrain player strictly within straight road width BEFORE reaching corner (Z < 496.5m)
            if (g.player.position.z < 496.5f) {
                g.player.position.x = Clamp(g.player.position.x, -3.0f, 3.0f);
            }
        } else {
            // Running forward along +X on Turned Horizontal Road
            g.player.position.x += g.forwardSpeed * deltaTime; // Auto forward running along +X!

            // On Turned Road (+X): Steer Left is -Z, Steer Right is +Z
            g.player.position.z += (lateralSteer) * g.horizontalSpeed * deltaTime;
            g.player.position.z = Clamp(g.player.position.z, 497.0f, 503.0f);
        }

        // 3. Dynamic visual rotation matching movement trajectory and turn progression
        // NOTE: Gyro and touch only control lateral position across road width; they NEVER rotate character or change road heading.
        float targetRotationY = 0.0f;
        if (!isOnTurnedPath) {
            if (g.player.position.z >= 495.0f && g.player.position.x > -1.0f) {
                // In turn zone: smoothly transition facing angle from 0 deg (+Z) to 90 deg (+X)
                float turnProgress = Clamp((g.player.position.x + 1.0f) / 4.0f, 0.0f, 1.0f);
                targetRotationY = turnProgress * 90.0f;
                if (lateralSteer < -0.1f) {
                    float moveYaw = atan2f(g.horizontalSpeed, g.forwardSpeed) * RAD2DEG; // ~35.5 deg
                    targetRotationY = fmaxf(targetRotationY, moveYaw);
                }
            } else {
                targetRotationY = 0.0f;
            }
        } else {
            targetRotationY = 90.0f;
        }

        // Smoothly rotate character model to face travel direction without snapping or lagging
        float rotLerpSpeed = Clamp(14.0f * deltaTime, 0.0f, 1.0f);
        float angleDiff = fmodf(targetRotationY - g.player.rotationY + 180.0f, 360.0f) - 180.0f;
        g.player.rotationY += angleDiff * rotLerpSpeed;

        // Check if player has run off the road surface
        if (g.player.position.y == 0.0f && !IsOnRoadSurface(g.player.position)) {
            g.player.isGrounded = false;
        }

        // 4. Vertical Jump Physics (SPACE Key or Touch Tap)
        if (g.player.isGrounded && (IsKeyPressed(KEY_SPACE) || g.touch.jumpTriggered)) {
            g.player.verticalVelocity = g.jumpVelocity;
            g.player.isGrounded = false;
        }

        if (!g.player.isGrounded) {
            g.player.verticalVelocity += g.gravity * deltaTime;
            g.player.position.y += g.player.verticalVelocity * deltaTime;

            if (g.player.position.y <= 0.0f && IsOnRoadSurface(g.player.position)) {
                g.player.position.y = 0.0f;
                g.player.verticalVelocity = 0.0f;
                g.player.isGrounded = true;
            } else if (g.player.position.y < -15.0f) {
                g.isGameOver = true;
            }
        }

        // Update Skeletal Model Running Animation continuously (In-Place loop)
        UpdatePlayerAnimation(g.player, true, deltaTime);

        // 5. Dynamic Coin Sequence Spawning
        const float spawnAheadDistance = 250.0f;
        while (g.nextCoinSpawnZ < g.player.position.z + spawnAheadDistance) {
            float coinX = (g.nextCoinSpawnZ <= 490.0f) ? 0.0f : (float)GetRandomValue(10, 250);
            float coinZ = (g.nextCoinSpawnZ <= 490.0f) ? g.nextCoinSpawnZ : 500.0f;
            int coinSequenceCount = GetRandomValue(3, 5);
            float spacing = 4.5f;

            for (int c = 0; c < coinSequenceCount; c++) {
                Coin coin;
                Vector3 cPos = (g.nextCoinSpawnZ <= 490.0f) ? (Vector3){ coinX, g.coinFloatHeight, coinZ + c * spacing } : (Vector3){ coinX + c * spacing, g.coinFloatHeight, coinZ };
                coin.position = cPos;
                coin.collected = false;
                coin.rotation = (float)GetRandomValue(0, 360);
                g.coins.push_back(coin);
            }

            g.nextCoinSpawnZ += coinSequenceCount * spacing + (float)GetRandomValue(15, 30);
        }

        // 6. Coin Rotation Animation & Collection Detection
        for (auto& coin : g.coins) {
            if (coin.collected) continue;

            coin.rotation += 150.0f * deltaTime;
            if (coin.rotation >= 360.0f) coin.rotation -= 360.0f;

            Vector3 currentCoinPos = coin.position;
            currentCoinPos.y += 0.15f * sinf(coin.rotation * (float)DEG2RAD * 2.0f);

            Vector3 playerCenter = { g.player.position.x, g.player.position.y + 1.0f, g.player.position.z };
            float hitRadius = g.player.collisionRadius + g.coinRadius;

            if (Vector3DistanceSqr(playerCenter, currentCoinPos) <= hitRadius * hitRadius) {
                coin.collected = true;
                g.score++;
            }
        }

        // Garbage collection
        for (size_t i = 0; i < g.coins.size(); ) {
            bool shouldErase = g.coins[i].collected;
            if (!isOnTurnedPath) {
                if (g.coins[i].position.z < g.player.position.z - 30.0f) shouldErase = true;
            } else {
                if (g.coins[i].position.x < g.player.position.x - 30.0f) shouldErase = true;
            }
            if (shouldErase) {
                g.coins.erase(g.coins.begin() + i);
            } else {
                i++;
            }
        }
    }

    // Camera Tracking
    Vector3 desiredCamPos;
    Vector3 desiredCamTarget;
    bool isOnTurnedPath = (g.player.position.x >= 3.0f && g.player.position.z >= 495.0f);

    if (!isOnTurnedPath) {
        desiredCamPos = (Vector3){ g.player.position.x, g.player.position.y + 4.0f, g.player.position.z - 8.0f };
        desiredCamTarget = (Vector3){ g.player.position.x, g.player.position.y + 1.0f, g.player.position.z + 8.0f };
    } else {
        desiredCamPos = (Vector3){ g.player.position.x - 8.0f, g.player.position.y + 4.0f, g.player.position.z };
        desiredCamTarget = (Vector3){ g.player.position.x + 8.0f, g.player.position.y + 1.0f, g.player.position.z };
    }

    float camLerpSpeed = Clamp(6.0f * deltaTime, 0.0f, 1.0f);
    g.camera.position = Vector3Lerp(g.camera.position, desiredCamPos, camLerpSpeed);
    g.camera.target   = Vector3Lerp(g.camera.target, desiredCamTarget, camLerpSpeed);

    // ------------------------------------------------------------------------------
    // Draw / Render
    // ------------------------------------------------------------------------------
    BeginDrawing();
        ClearBackground((Color){ 165, 205, 235, 255 });

        BeginMode3D(g.camera);
            DrawMountainValleyEnvironment(g.valley, g.player.position.z, g.roadWidth);

            for (const auto& coin : g.coins) {
                if (coin.collected) continue;

                float bobOffset = 0.15f * sinf(coin.rotation * (float)DEG2RAD * 2.0f);
                Vector3 drawPos = { coin.position.x, coin.position.y + bobOffset, coin.position.z };

                rlPushMatrix();
                    rlTranslatef(drawPos.x, drawPos.y, drawPos.z);
                    rlRotatef(coin.rotation, 0.0f, 1.0f, 0.0f);
                    rlRotatef(90.0f, 1.0f, 0.0f, 0.0f);

                    DrawCylinder((Vector3){ 0, -g.coinHeight / 2.0f, 0 }, g.coinRadius, g.coinRadius, g.coinHeight, 16, (Color){ 255, 215, 0, 255 });
                    DrawCylinderWires((Vector3){ 0, -g.coinHeight / 2.0f, 0 }, g.coinRadius + 0.01f, g.coinRadius + 0.01f, g.coinHeight + 0.01f, 16, (Color){ 220, 140, 0, 255 });
                    DrawCircle3D((Vector3){ 0, g.coinHeight / 2.0f + 0.005f, 0 }, g.coinRadius * 0.55f, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){ 255, 245, 140, 255 });
                    DrawCircle3D((Vector3){ 0, -g.coinHeight / 2.0f - 0.005f, 0 }, g.coinRadius * 0.55f, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){ 255, 245, 140, 255 });
                rlPopMatrix();

                DrawCircle3D((Vector3){ drawPos.x, 0.02f, drawPos.z }, g.coinRadius * 0.85f, (Vector3){ 1.0f, 0.0f, 0.0f }, 90.0f, (Color){ 255, 215, 0, 70 });
            }

            DrawPlayerCharacter(g.player);
        EndMode3D();

        // HUD Overlay
        DrawRectangle(15, 15, 340, 222, Fade((Color){ 15, 18, 30, 255 }, 0.85f));
        DrawRectangleLines(15, 15, 340, 222, (Color){ 210, 150, 40, 255 });

        DrawText("TEMPLE RUNNER 3D", 30, 25, 20, (Color){ 240, 170, 50, 255 });
        DrawText("A / D / Arrows : Move (Desktop)", 30, 50, 13, RAYWHITE);
        DrawText("Touch / Drag   : Move (Mobile)", 30, 68, 13, RAYWHITE);
        DrawText("Tilt Device    : Gyro Steering", 30, 86, 13, (Color){ 100, 240, 255, 255 });
        DrawText("SPACE / Tap Top: Jump", 30, 104, 13, (Color){ 255, 215, 0, 255 });

        float currentDistance = isOnTurnedPath ? (500.0f + (g.player.position.x - 3.0f)) : g.player.position.z;
        DrawText(TextFormat("Distance: %.1f m", currentDistance), 30, 128, 16, YELLOW);
        DrawText(TextFormat("Coins: %d", g.score), 30, 150, 16, (Color){ 255, 215, 0, 255 });
        DrawText(TextFormat("Speed: %.1f m/s", g.isGameOver ? 0.0f : g.forwardSpeed), 30, 172, 16, g.isGameOver ? RED : GREEN);
        DrawText(TextFormat("Player X: %.3f", g.player.position.x), 30, 194, 16, (Color){ 100, 230, 255, 255 });

        DrawFPS(GetScreenWidth() - 100, 20);

#if defined(__EMSCRIPTEN__)
        // Motion / Gyro Diagnostic Overlay Panel (top-left below main HUD)
        int diagY = 245;
        int diagW = 345;
        int diagH = 175;
        DrawRectangle(15, diagY, diagW, diagH, Fade((Color){ 10, 15, 26, 255 }, 0.92f));
        DrawRectangleLines(15, diagY, diagW, diagH, (Color){ 70, 160, 255, 255 });

        DrawText("MOTION / GYRO DIAGNOSTICS", 25, diagY + 8, 14, (Color){ 100, 225, 255, 255 });

        bool supported = IsWebMotionSupported();
        int permStatus = GetWebPermissionStatus();
        const char* permStr = "not requested";
        Color permColor = YELLOW;
        if (!supported || permStatus == 3) { permStr = "unavailable"; permColor = RED; }
        else if (permStatus == 1) { permStr = "granted"; permColor = GREEN; }
        else if (permStatus == 2) { permStr = "denied"; permColor = RED; }

        int evCount = GetWebOrientationEventCount();
        bool eventsReceived = (evCount > 0);
        float rawG = GetWebRawGamma();
        float rawB = GetWebRawBeta();
        int axisCode = GetWebSelectedAxisCode();
        const char* axisStr = "gamma (portrait 0 deg)";
        if (axisCode == 1) axisStr = "beta (landscape 90 deg)";
        else if (axisCode == 2) axisStr = "-beta (landscape 270 deg)";
        else if (axisCode == 3) axisStr = "-gamma (portrait 180 deg)";
        else if (axisCode == 4) axisStr = "accel (fallback)";

        DrawText(TextFormat("Motion API supported: %s", supported ? "YES" : "NO"), 25, diagY + 28, 12, supported ? GREEN : RED);
        DrawText(TextFormat("Permission state: %s", permStr), 25, diagY + 44, 12, permColor);
        DrawText(TextFormat("Events received: %s (%d total)", eventsReceived ? "YES" : "NO", evCount), 25, diagY + 60, 12, eventsReceived ? GREEN : ORANGE);
        DrawText(TextFormat("Raw gamma: %.2f | Raw beta: %.2f", rawG, rawB), 25, diagY + 76, 12, (Color){ 200, 225, 255, 255 });
        DrawText(TextFormat("Selected axis: %s", axisStr), 25, diagY + 92, 12, (Color){ 170, 215, 255, 255 });
        DrawText(TextFormat("Normalized gyro steer: %.3f", g.lastRawGyroSteer), 25, diagY + 108, 12, (Color){ 255, 220, 90, 255 });
        DrawText(TextFormat("Filtered gyro steer:   %.3f", g.lastGyroSteer), 25, diagY + 124, 12, (Color){ 255, 200, 60, 255 });
        DrawText(TextFormat("Final lateralSteer:    %.3f", g.lastLateralSteer), 25, diagY + 140, 12, (Color){ 60, 240, 180, 255 });
        DrawText(TextFormat("Touch: %.2f | Keys: %.2f", g.lastTouchSteer, g.lastKeyboardSteer), 25, diagY + 156, 11, GRAY);

        // Motion Controls Prompt Button (top-right next to FPS)
        Rectangle motionBtn = { (float)GetScreenWidth() - 220, 50, 205, 30 };
        Vector2 mousePos = GetMousePosition();
        bool hovered = CheckCollisionPointRec(mousePos, motionBtn);
        DrawRectangleRec(motionBtn, hovered ? (Color){ 45, 75, 120, 230 } : (Color){ 25, 40, 70, 200 });
        DrawRectangleLinesEx(motionBtn, 1, (Color){ 100, 200, 255, 255 });
        const char* btnLabel = (permStatus == 1 && eventsReceived) ? "Gyro: Active" : "Enable Motion / Gyro";
        DrawText(btnLabel, motionBtn.x + 15, motionBtn.y + 8, 13, (Color){ 200, 240, 255, 255 });
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            RequestDOMMotionPermission();
        }
#endif

        // Game Over Modal Overlay
        if (g.isGameOver) {
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

            const char* scoreText = TextFormat("Distance Survived: %.1f m", currentDistance);
            int scoreWidth = MeasureText(scoreText, 22);
            DrawText(scoreText, bannerX + (bannerWidth - scoreWidth) / 2, bannerY + 85, 22, YELLOW);

            const char* coinText = TextFormat("Coins Collected: %d", g.score);
            int coinWidth = MeasureText(coinText, 22);
            DrawText(coinText, bannerX + (bannerWidth - coinWidth) / 2, bannerY + 115, 22, (Color){ 255, 215, 0, 255 });

            const char* restartText = "Press [ R ] or Tap to Restart";
            int restartWidth = MeasureText(restartText, 20);
            DrawText(restartText, bannerX + (bannerWidth - restartWidth) / 2, bannerY + 175, 20, (Color){ 0, 230, 255, 255 });
        }

    EndDrawing();
}

int main() {
    // ----------------------------------------------------------------------------------
    // Initialization
    // ----------------------------------------------------------------------------------
    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "3D Temple Runner - Mountain Valley Canyon");

    TraceLog(LOG_INFO, "CONTINUOUS_MOVEMENT_BUILD_20260824");

#if defined(__EMSCRIPTEN__)
    InitWebOrientationBridge();
#endif

    // Load Rigged 3D Humanoid Character System with assets/player/character.glb
    const char* relativeModelPath = "assets/player/character.glb";
    if (!InitPlayerCharacter(g_game.player, relativeModelPath)) {
        TraceLog(LOG_ERROR, "FATAL ERROR: Could not load 3D rigged humanoid character from %s", relativeModelPath);
    }
    g_game.player.position = (Vector3){ 0.0f, 0.0f, 0.0f };

    // Generate Natural 3D Mountain Valley Canyon System
    InitMountainValleySystem(g_game.valley);

    // 3D Third-person camera setup
    g_game.camera = { 0 };
    g_game.camera.position = (Vector3){ 0.0f, 4.0f, -8.0f };
    g_game.camera.target   = (Vector3){ 0.0f, 1.0f, 8.0f };
    g_game.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    g_game.camera.fovy = 50.0f;
    g_game.camera.projection = CAMERA_PERSPECTIVE;

    SetTargetFPS(60);

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
    }
#endif

    // Unload 3D Models and Resources
    UnloadMountainValleySystem(g_game.valley);
    UnloadPlayerCharacter(g_game.player);
    CloseWindow();

    return 0;
}
