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

    // Hanging Animation for Zipline
    ModelAnimation* hangingAnimations;
    int hangingAnimsCount;
    float hangingAnimTime;

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
    player.hangingAnimations = nullptr;
    player.hangingAnimsCount = 0;
    player.runningAnimIndex = 0;
    player.idleAnimIndex = 0;
    player.currentAnimIndex = 0;
    player.animTime = 0.0f;
    player.hangingAnimTime = 0.0f;

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

    // Load separate Hanging animation asset for Zipline
    const char* hangingAnimPath = "assets/player/Hanging.glb";
    if (FileExists(hangingAnimPath)) {
        player.hangingAnimations = LoadModelAnimations(hangingAnimPath, &player.hangingAnimsCount);
        TraceLog(LOG_INFO, "=== Loaded %d Hanging Animations from %s ===", player.hangingAnimsCount, hangingAnimPath);
        for (int i = 0; i < player.hangingAnimsCount; i++) {
            TraceLog(LOG_INFO, "Hanging Animation [%d]: '%s' (%d frames, %d bones)",
                     i, player.hangingAnimations[i].name, player.hangingAnimations[i].frameCount, player.hangingAnimations[i].boneCount);
        }
    } else {
        TraceLog(LOG_WARNING, "WARNING: Hanging animation file not found at: %s", hangingAnimPath);
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

// Update Skeletal Model Animation Frame for running sprint, hanging loop, or holding pose
void UpdatePlayerAnimation(PlayerCharacter& player, bool isRunning, bool isZiplining, float deltaTime) {
    if (!player.isModelLoaded) return;

    if (isZiplining) {
        // Play 78-frame hanging animation loop while holding onto overhead zipline
        if (player.hangingAnimations != nullptr && player.hangingAnimsCount > 0) {
            ModelAnimation anim = player.hangingAnimations[0];
            const float animSpeed = 30.0f; // 30 FPS playback rate for 2.6s (78 frames) clip
            player.hangingAnimTime += deltaTime * animSpeed;
            int currentFrame = (int)player.hangingAnimTime % anim.frameCount;
            UpdateModelAnimation(player.model, anim, currentFrame);
        }
    } else if (isRunning) {
        if (player.animations != nullptr && player.animsCount > 0) {
            ModelAnimation anim = player.animations[player.runningAnimIndex];
            const float animSpeed = 30.0f;
            player.animTime += deltaTime * animSpeed;
            int currentFrame = (int)player.animTime % anim.frameCount;
            UpdateModelAnimation(player.model, anim, currentFrame);
        }
    } else {
        if (player.animations != nullptr && player.animsCount > 0) {
            ModelAnimation anim = player.animations[player.runningAnimIndex];
            UpdateModelAnimation(player.model, anim, 0);
        }
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
        if (player.hangingAnimations != nullptr && player.hangingAnimsCount > 0) {
            UnloadModelAnimations(player.hangingAnimations, player.hangingAnimsCount);
            player.hangingAnimations = nullptr;
            player.hangingAnimsCount = 0;
        }
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

// Helper function to check if player position is on solid road ground for 90-Degree T-Junction Dual Route Map
// Helper function to check if player position is on solid road ground for Multi-Turn Route Map
inline bool IsOnRoadSurface(Vector3 pos) {
    // 1. Straight Main Road (Z in [-10, 503], X centered at 0 within 3.5m)
    if (pos.z >= -10.0f && pos.z <= 503.0f && pos.x >= -3.5f && pos.x <= 3.5f) {
        return true;
    }
    // 2. Horizontal Left (+X up to 353.5m) and Right (-X down to -350m) Routes (Z around 500m)
    if (pos.z >= 496.5f && pos.z <= 503.5f && pos.x >= -350.0f && pos.x <= 353.5f) {
        return true;
    }
    // 3. Extended Continued Route after 90° Right Turn (X centered at 350m within 3.5m, Z from 497m to 855m)
    if (pos.z >= 496.5f && pos.z <= 855.0f && pos.x >= 346.5f && pos.x <= 353.5f) {
        return true;
    }
    // 4. Post-Zipline Destination Road (X centered at -700m within 3.5m, Z from 795m to 1205m)
    if (pos.z >= 795.0f && pos.z <= 1205.0f && pos.x >= -703.5f && pos.x <= -696.5f) {
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------
// Curved 3D Zipline Route Geometry (~500m smooth sweeping catenary curve)
// Starts at Right Road End (-350m, 6m, 500m) -> Finishes at Destination Road (-700m, 0m, 800m)
// ------------------------------------------------------------------------------
inline Vector3 GetZiplinePoint(float s) {
    s = Clamp(s, 0.0f, 1.0f);
    // Smooth S-curve across canyon in X and Z
    float x = -350.0f - 350.0f * s - 40.0f * sinf(s * PI);
    float z = 500.0f + 300.0f * s + 50.0f * sinf(s * PI);

    // Gradual smooth descent with natural catenary cable sag
    float baseHeight = 6.0f * (1.0f - s);
    float sag = -1.0f * sinf(s * PI);
    float y = baseHeight + sag;
    if (y < 0.0f) y = 0.0f;

    return (Vector3){ x, y, z };
}

inline Vector3 GetZiplineTangent(float s) {
    float delta = 0.01f;
    Vector3 p1 = GetZiplinePoint(fmaxf(0.0f, s - delta));
    Vector3 p2 = GetZiplinePoint(fminf(1.0f, s + delta));
    return Vector3Normalize(Vector3Subtract(p2, p1));
}

// Render 3D Curved Zipline Cable, Support Towers, and Arrival Gantry
void DrawZiplineRoute() {
    // 1. Start Launch Tower at right road endpoint (X = -350, Z = 500)
    DrawCube((Vector3){ -350.0f, 3.75f, 497.0f }, 0.8f, 8.5f, 0.8f, (Color){ 85, 65, 45, 255 });
    DrawCube((Vector3){ -350.0f, 3.75f, 503.0f }, 0.8f, 8.5f, 0.8f, (Color){ 85, 65, 45, 255 });
    DrawCube((Vector3){ -350.0f, 7.5f, 500.0f }, 1.0f, 0.8f, 6.8f, (Color){ 135, 100, 65, 255 });
    DrawCubeWires((Vector3){ -350.0f, 7.5f, 500.0f }, 1.0f, 0.8f, 6.8f, (Color){ 60, 40, 25, 255 });

    // Glowing target sphere at cable start
    DrawSphere((Vector3){ -350.0f, 6.0f, 500.0f }, 0.45f, (Color){ 255, 215, 80, 240 });

    // 2. End Arrival Gantry at destination road (X = -700, Z = 800)
    DrawCube((Vector3){ -700.0f, 2.5f, 797.5f }, 0.8f, 6.0f, 0.8f, (Color){ 85, 65, 45, 255 });
    DrawCube((Vector3){ -700.0f, 2.5f, 802.5f }, 0.8f, 6.0f, 0.8f, (Color){ 85, 65, 45, 255 });
    DrawCube((Vector3){ -700.0f, 5.0f, 800.0f }, 1.0f, 0.8f, 5.8f, (Color){ 135, 100, 65, 255 });
    DrawCubeWires((Vector3){ -700.0f, 5.0f, 800.0f }, 1.0f, 0.8f, 5.8f, (Color){ 60, 40, 25, 255 });

    // 3. Smooth Curved 3D Steel Cable Segments (~500m long)
    const int numCableSegs = 75;
    for (int i = 0; i < numCableSegs; i++) {
        float s1 = (float)i / (float)numCableSegs;
        float s2 = (float)(i + 1) / (float)numCableSegs;
        Vector3 p1 = GetZiplinePoint(s1);
        Vector3 p2 = GetZiplinePoint(s2);

        // Main steel cable
        DrawLine3D(p1, p2, (Color){ 210, 190, 140, 255 });
        DrawLine3D((Vector3){ p1.x, p1.y + 0.035f, p1.z }, (Vector3){ p2.x, p2.y + 0.035f, p2.z }, (Color){ 255, 235, 180, 255 });
    }
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
        if (segZ > 490.0f) continue; // End river cleanly before the turned road intersection at Z=497m

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
        if (z1 > 490.0f) continue;

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

// Render Clean Road Environment: Main Route, Left & Right Branches, Left-Extension, and Post-Zipline Destination
void DrawMountainValleyEnvironment(const MountainValleySystem& valley, float playerZ, float roadWidth) {
    const float narrowPathWidth = 6.0f; // 6.0m path width
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

        // West curb (Right side when running +Z, X = -3.45m) - stops cleanly at Z = 497.0m for right turn opening!
        DrawCurb((Vector3){ -3.45f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });

        // East curb (Left side when running +Z, X = +3.45m) - stops cleanly at Z = 497.0m for left turn opening!
        DrawCurb((Vector3){ 3.45f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });
    }

    // --- 2. Clean 90-Degree T-Junction Corner Tile (Z in [497.0m, 503.0m], X in [-3.0m, 3.0m]) ---
    DrawTileSurface((Vector3){ 0.0f, -0.5f, 500.0f }, narrowPathWidth, narrowPathWidth, 83);
    DrawCurb((Vector3){ 0.0f, 0.35f, 503.45f }, (Vector3){ 6.9f, 0.7f, 0.9f });

    // --- 3. Turned Left Horizontal Road (+X: 3.0m to 347.0m, centered at Z = 500.0m) ---
    for (float x = 3.0f; x < 347.0f; x += tileLength) {
        float nextX = fminf(x + tileLength, 347.0f);
        float len = nextX - x;
        float centerX = x + len / 2.0f;
        int tileIdx = (int)(x / tileLength);

        DrawTileSurface((Vector3){ centerX, -0.5f, 500.0f }, len, narrowPathWidth, tileIdx);
        DrawCurb((Vector3){ centerX, 0.35f, 503.45f }, (Vector3){ len, 0.7f, 0.9f });
        DrawCurb((Vector3){ centerX, 0.35f, 496.55f }, (Vector3){ len, 0.7f, 0.9f });
    }

    // --- 3b. 90-Degree Right Turn Corner Tile at X = 350.0m, Z = 500.0m ---
    DrawTileSurface((Vector3){ 350.0f, -0.5f, 500.0f }, narrowPathWidth, narrowPathWidth, 142);
    DrawCurb((Vector3){ 350.0f, 0.35f, 496.55f }, (Vector3){ 6.9f, 0.7f, 0.9f });
    DrawCurb((Vector3){ 353.45f, 0.35f, 500.0f }, (Vector3){ 0.9f, 0.7f, 6.9f });

    // --- 3c. Extended Continued Route after 90° Right Turn (+Z: 503.0m to 850.0m, centered at X = 350.0m) ---
    for (float z = 503.0f; z < 850.0f; z += tileLength) {
        float nextZ = fminf(z + tileLength, 850.0f);
        float len = nextZ - z;
        float centerZ = z + len / 2.0f;
        int tileIdx = (int)(z / tileLength);

        DrawTileSurface((Vector3){ 350.0f, -0.5f, centerZ }, narrowPathWidth, len, tileIdx);
        DrawCurb((Vector3){ 346.55f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });
        DrawCurb((Vector3){ 353.45f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });
    }

    // --- 4. Mirrored Turned Right Horizontal Road (-X: -3.0m to -350.0m, centered at Z = 500.0m) ---
    for (float x = -3.0f; x >= -350.0f; x -= tileLength) {
        float nextX = fmaxf(x - tileLength, -350.0f);
        float len = fabsf(nextX - x);
        float centerX = (x + nextX) / 2.0f;
        int tileIdx = (int)(fabsf(x) / tileLength);

        DrawTileSurface((Vector3){ centerX, -0.5f, 500.0f }, len, narrowPathWidth, tileIdx);
        DrawCurb((Vector3){ centerX, 0.35f, 503.45f }, (Vector3){ len, 0.7f, 0.9f });
        DrawCurb((Vector3){ centerX, 0.35f, 496.55f }, (Vector3){ len, 0.7f, 0.9f });
    }

    // --- 5. Post-Zipline Destination Road (+Z: 800.0m to 1200.0m, centered at X = -700.0m) ---
    for (float z = 800.0f; z < 1200.0f; z += tileLength) {
        float nextZ = fminf(z + tileLength, 1200.0f);
        float len = nextZ - z;
        float centerZ = z + len / 2.0f;
        int tileIdx = (int)(z / tileLength);

        DrawTileSurface((Vector3){ -700.0f, -0.5f, centerZ }, narrowPathWidth, len, tileIdx);
        DrawCurb((Vector3){ -703.45f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });
        DrawCurb((Vector3){ -696.55f, 0.35f, centerZ }, (Vector3){ 0.9f, 0.7f, len });
    }

    // --- 6. Tile Seams ---
    float startSeamZ = floorf((playerZ - 30.0f) / 6.0f) * 6.0f;
    float endSeamZ = fminf(playerZ + 220.0f, 497.0f);
    for (float z = startSeamZ; z <= endSeamZ; z += 6.0f) {
        if (z < 0.0f || z > 497.0f) continue;
        Color seamColor = (fmodf(z, 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
        DrawLine3D((Vector3){ -narrowPathWidth / 2.0f, 0.015f, z }, (Vector3){ narrowPathWidth / 2.0f, 0.015f, z }, seamColor);
    }

    // Left road seams (+X)
    for (float x = 3.0f; x <= 347.0f; x += 6.0f) {
        Color seamColor = (fmodf(x, 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
        DrawLine3D((Vector3){ x, 0.015f, 497.0f }, (Vector3){ x, 0.015f, 503.0f }, seamColor);
    }
    // Extended Segment 3 seams (+Z at X=350)
    for (float z = 504.0f; z <= 850.0f; z += 6.0f) {
        Color seamColor = (fmodf(z, 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
        DrawLine3D((Vector3){ 347.0f, 0.015f, z }, (Vector3){ 353.0f, 0.015f, z }, seamColor);
    }
    // Right road seams (-X)
    for (float x = -3.0f; x >= -350.0f; x -= 6.0f) {
        Color seamColor = (fmodf(fabsf(x), 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
        DrawLine3D((Vector3){ x, 0.015f, 497.0f }, (Vector3){ x, 0.015f, 503.0f }, seamColor);
    }
    // Post-zipline destination road seams (+Z at X=-700)
    for (float z = 804.0f; z <= 1200.0f; z += 6.0f) {
        Color seamColor = (fmodf(z, 24.0f) == 0.0f) ? (Color){ 220, 165, 60, 230 } : (Color){ 60, 50, 40, 180 };
        DrawLine3D((Vector3){ -703.0f, 0.015f, z }, (Vector3){ -697.0f, 0.015f, z }, seamColor);
    }
}

struct GameState {
    PlayerCharacter player;
    MountainValleySystem valley;
    GyroSteeringSystem gyro;
    TouchSteeringState touch;

    // Forward Running Speed & Acceleration Tuning Constants
    static constexpr float BASE_FORWARD_SPEED = 14.0f;       // Initial starting speed (m/s)
    static constexpr float FORWARD_ACCELERATION = 0.25f;      // Smooth acceleration rate (m/s^2)
    static constexpr float MAX_FORWARD_SPEED = 28.0f;         // Capped maximum running speed (m/s)

    // Zipline Route Tuning Constants
    static constexpr float ZIPLINE_LENGTH = 500.0f;          // Approximate cable curved length (m)
    static constexpr float ZIPLINE_SPEED = 24.0f;           // Gliding travel speed along cable (m/s)
    static constexpr float ZIPLINE_GRAB_RADIUS = 4.0f;      // Airborne grab radius around cable start
    static constexpr float ZIPLINE_HEIGHT_START = 6.0f;     // Starting cable elevation (m)
    static constexpr float ZIPLINE_HEIGHT_END = 0.0f;       // Ending destination elevation (m)

    float forwardSpeed = BASE_FORWARD_SPEED;
    float jumpVelocity = 10.5f;
    float gravity = -24.0f;
    float roadWidth = 10.0f;
    float horizontalSpeed = 10.0f;

    // Zipline Gameplay State
    bool isZiplining = false;
    float ziplineProgress = 0.0f; // 0.0f (start) -> 1.0f (destination)

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
        forwardSpeed = BASE_FORWARD_SPEED;
        isZiplining = false;
        ziplineProgress = 0.0f;
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

    // --------------------------------------------------------------------------
    // 1. Zipline Grab Trigger (When jumping off right road endpoint X=-350, Z=500)
    // --------------------------------------------------------------------------
    if (!g.isGameOver && !g.isZiplining && !g.player.isGrounded) {
        Vector3 cableStart = GetZiplinePoint(0.0f);
        // Player model height is 2.5m, reach/hands are at y + 2.0m
        Vector3 playerReach = { g.player.position.x, g.player.position.y + 2.0f, g.player.position.z };
        if (Vector3DistanceSqr(playerReach, cableStart) <= GameState::ZIPLINE_GRAB_RADIUS * GameState::ZIPLINE_GRAB_RADIUS) {
            g.isZiplining = true;
            g.ziplineProgress = 0.0f;
            g.player.verticalVelocity = 0.0f;
            g.player.hangingAnimTime = 0.0f;
        }
    }

    // --------------------------------------------------------------------------
    // 2. Active Zipline Riding Simulation (~500m curved gliding ride)
    // --------------------------------------------------------------------------
    if (g.isZiplining) {
        float progressDelta = (GameState::ZIPLINE_SPEED / GameState::ZIPLINE_LENGTH) * deltaTime;
        g.ziplineProgress += progressDelta;

        if (g.ziplineProgress >= 1.0f) {
            // Reached Destination Road at X = -700m, Z = 800m!
            g.ziplineProgress = 1.0f;
            g.isZiplining = false;
            g.player.position = (Vector3){ -700.0f, 0.0f, 800.0f };
            g.player.rotationY = 0.0f; // Facing +Z on Destination Road
            g.player.verticalVelocity = 0.0f;
            g.player.isGrounded = true;
            g.player.animTime = 0.0f;
        } else {
            Vector3 cablePos = GetZiplinePoint(g.ziplineProgress);
            // Model origin is at feet (y=0) and height is 2.5m.
            // When hanging by hands from the trolley, feet are ~2.35m below cable centerline.
            const float ZIPLINE_HANG_OFFSET_Y = 2.35f;
            g.player.position = (Vector3){ cablePos.x, cablePos.y - ZIPLINE_HANG_OFFSET_Y, cablePos.z };

            // Rotate character to face forward along curved cable 3D tangent
            Vector3 tangent = GetZiplineTangent(g.ziplineProgress);
            float targetYaw = atan2f(tangent.x, tangent.z) * RAD2DEG;
            g.player.rotationY = targetYaw;
            g.player.verticalVelocity = 0.0f;
            g.player.isGrounded = false;
        }
    } else if (!g.isGameOver) {
        // ----------------------------------------------------------------------
        // 3. Normal Ground & Airborne Movement across all Road Routes
        // ----------------------------------------------------------------------
        g.forwardSpeed = fminf(g.forwardSpeed + GameState::FORWARD_ACCELERATION * deltaTime, GameState::MAX_FORWARD_SPEED);

        bool isOnDestinationRoad = (g.player.position.x <= -650.0f && g.player.position.z >= 795.0f);
        bool isOnSegment3 = (!isOnDestinationRoad && g.player.position.x >= 347.0f && g.player.position.z >= 503.0f);
        bool isOnTurnedLeftPath  = (!isOnDestinationRoad && !isOnSegment3 && g.player.position.x >= 3.0f && g.player.position.z >= 495.0f);
        bool isOnTurnedRightPath = (!isOnDestinationRoad && g.player.position.x <= -3.0f && g.player.position.z >= 495.0f);
        bool isOnMainStraight = (!isOnDestinationRoad && !isOnSegment3 && !isOnTurnedLeftPath && !isOnTurnedRightPath);

        if (isOnMainStraight) {
            // Running forward along +Z on Main Straight Road (0m -> 500m)
            g.player.position.z += g.forwardSpeed * deltaTime;

            // On Straight Road (+Z): Steer Left is +X, Steer Right is -X
            g.player.position.x += (-lateralSteer) * g.horizontalSpeed * deltaTime;

            // Constrain player strictly within straight road width ONLY WHILE GROUNDED
            if (g.player.isGrounded && g.player.position.z < 496.5f) {
                g.player.position.x = Clamp(g.player.position.x, -3.0f, 3.0f);
            }
        } else if (isOnTurnedLeftPath) {
            // Running forward along +X on Left Turned Horizontal Road (3m -> 350m)
            g.player.position.x += g.forwardSpeed * deltaTime; // Auto forward running along +X!

            // On Left Turned Road (+X): Steer Left is -Z, Steer Right is +Z
            g.player.position.z += (lateralSteer) * g.horizontalSpeed * deltaTime;

            // Constrain player strictly within left road width ONLY WHILE GROUNDED
            if (g.player.isGrounded && g.player.position.x < 346.5f) {
                g.player.position.z = Clamp(g.player.position.z, 497.0f, 503.0f);
            }
        } else if (isOnSegment3) {
            // Running forward along +Z on Extended Continued Road (503m -> 850m) after 90° Right Turn
            g.player.position.z += g.forwardSpeed * deltaTime; // Auto forward running along +Z!

            // On Extended Road (+Z): Steer Left is +X, Steer Right is -X
            g.player.position.x += (-lateralSteer) * g.horizontalSpeed * deltaTime;

            // Constrain player strictly within extended road width ONLY WHILE GROUNDED
            if (g.player.isGrounded) {
                g.player.position.x = Clamp(g.player.position.x, 347.0f, 353.0f);
            }
        } else if (isOnDestinationRoad) {
            // Running forward along +Z on Post-Zipline Destination Road (-700m, 800m -> 1200m)
            g.player.position.z += g.forwardSpeed * deltaTime;

            // On Destination Road (+Z): Steer Left is +X, Steer Right is -X
            g.player.position.x += (-lateralSteer) * g.horizontalSpeed * deltaTime;

            // Constrain player strictly within destination road width ONLY WHILE GROUNDED
            if (g.player.isGrounded) {
                g.player.position.x = Clamp(g.player.position.x, -703.0f, -697.0f);
            }
        } else {
            // Running forward along -X on Right Turned Horizontal Road (-3m -> -350m)
            g.player.position.x -= g.forwardSpeed * deltaTime; // Auto forward running along -X!

            // On Right Turned Road (-X): Steer Left is +Z, Steer Right is -Z
            g.player.position.z += (-lateralSteer) * g.horizontalSpeed * deltaTime;

            // Constrain player strictly within right road width ONLY WHILE GROUNDED
            if (g.player.isGrounded) {
                g.player.position.z = Clamp(g.player.position.z, 497.0f, 503.0f);
            }
        }

        // Dynamic visual rotation matching movement trajectory and turn progression
        float targetRotationY = 0.0f;
        if (isOnMainStraight) {
            if (g.player.position.z >= 495.0f) {
                if (g.player.position.x > 0.5f) {
                    float turnProgress = Clamp((g.player.position.x - 0.5f) / 2.5f, 0.0f, 1.0f);
                    targetRotationY = turnProgress * 90.0f;
                    if (lateralSteer < -0.1f) {
                        float moveYaw = atan2f(g.horizontalSpeed, g.forwardSpeed) * RAD2DEG;
                        targetRotationY = fmaxf(targetRotationY, moveYaw);
                    }
                } else if (g.player.position.x < -0.5f) {
                    float turnProgress = Clamp((-g.player.position.x - 0.5f) / 2.5f, 0.0f, 1.0f);
                    targetRotationY = -turnProgress * 90.0f;
                    if (lateralSteer > 0.1f) {
                        float moveYaw = -atan2f(g.horizontalSpeed, g.forwardSpeed) * RAD2DEG;
                        targetRotationY = fminf(targetRotationY, moveYaw);
                    }
                } else {
                    targetRotationY = 0.0f;
                }
            } else {
                targetRotationY = 0.0f;
            }
        } else if (isOnTurnedLeftPath) {
            if (g.player.position.x >= 345.0f) {
                float turnProgress = Clamp((g.player.position.x - 345.0f) / 4.0f, 0.0f, 1.0f);
                targetRotationY = (1.0f - turnProgress) * 90.0f;
            } else {
                targetRotationY = 90.0f; // Facing +X
            }
        } else if (isOnSegment3 || isOnDestinationRoad) {
            targetRotationY = 0.0f; // Facing +Z on extended and destination routes
        } else {
            targetRotationY = -90.0f; // Facing -X on right route
        }

        float rotLerpSpeed = Clamp(14.0f * deltaTime, 0.0f, 1.0f);
        float angleDiff = fmodf(targetRotationY - g.player.rotationY + 180.0f, 360.0f) - 180.0f;
        g.player.rotationY += angleDiff * rotLerpSpeed;

        // Check if grounded player has run off the road surface
        if (g.player.position.y == 0.0f && !IsOnRoadSurface(g.player.position)) {
            g.player.isGrounded = false;
        }

        // Vertical Jump Physics (SPACE Key or Touch Tap)
        if (g.player.isGrounded && (IsKeyPressed(KEY_SPACE) || g.touch.jumpTriggered)) {
            g.player.verticalVelocity = g.jumpVelocity;
            g.player.isGrounded = false;
        }

        if (!g.player.isGrounded) {
            g.player.verticalVelocity += g.gravity * deltaTime;
            g.player.position.y += g.player.verticalVelocity * deltaTime;

            // When descending back down to ground level (y <= 0)
            if (g.player.position.y <= 0.0f) {
                if (IsOnRoadSurface(g.player.position)) {
                    g.player.position.y = 0.0f;
                    g.player.verticalVelocity = 0.0f;
                    g.player.isGrounded = true;
                } else {
                    if (g.player.position.y < -15.0f) {
                        g.isGameOver = true;
                    }
                }
            }
        }

        // Calculate current total distance traveled along all routes
        float currentDistance = 0.0f;
        if (isOnDestinationRoad) {
            currentDistance = 500.0f + 350.0f + GameState::ZIPLINE_LENGTH + (g.player.position.z - 800.0f);
        } else if (isOnSegment3) {
            currentDistance = 500.0f + 347.0f + (g.player.position.z - 503.0f);
        } else if (isOnTurnedLeftPath) {
            currentDistance = 500.0f + (g.player.position.x - 3.0f);
        } else if (isOnTurnedRightPath) {
            currentDistance = 500.0f + (-g.player.position.x - 3.0f);
        } else {
            currentDistance = g.player.position.z;
        }

        // Dynamic Coin Sequence Spawning
        const float spawnAheadDistance = 250.0f;
        while (g.nextCoinSpawnZ < currentDistance + spawnAheadDistance) {
            float coinDist = g.nextCoinSpawnZ;
            int coinSequenceCount = GetRandomValue(3, 5);
            float spacing = 4.5f;

            if (coinDist <= 490.0f) {
                for (int c = 0; c < coinSequenceCount; c++) {
                    Coin coin;
                    coin.position = (Vector3){ 0.0f, g.coinFloatHeight, coinDist + c * spacing };
                    coin.collected = false;
                    coin.rotation = (float)GetRandomValue(0, 360);
                    g.coins.push_back(coin);
                }
            } else if (coinDist <= 840.0f) {
                float offsetFromTurn = coinDist - 490.0f;
                float startX = 6.0f + offsetFromTurn;
                for (int c = 0; c < coinSequenceCount; c++) {
                    if (startX + c * spacing <= 345.0f) {
                        Coin leftCoin;
                        leftCoin.position = (Vector3){ startX + c * spacing, g.coinFloatHeight, 500.0f };
                        leftCoin.collected = false;
                        leftCoin.rotation = (float)GetRandomValue(0, 360);
                        g.coins.push_back(leftCoin);
                    }

                    Coin rightCoin;
                    rightCoin.position = (Vector3){ -(startX + c * spacing), g.coinFloatHeight, 500.0f };
                    rightCoin.collected = false;
                    rightCoin.rotation = (float)GetRandomValue(0, 360);
                    g.coins.push_back(rightCoin);
                }
            } else if (coinDist <= 1350.0f) {
                float offset = coinDist - 840.0f;
                float startZ = 505.0f + offset;
                for (int c = 0; c < coinSequenceCount; c++) {
                    Coin extCoin;
                    extCoin.position = (Vector3){ 350.0f, g.coinFloatHeight, startZ + c * spacing };
                    extCoin.collected = false;
                    extCoin.rotation = (float)GetRandomValue(0, 360);
                    g.coins.push_back(extCoin);
                }
            } else {
                float offset = coinDist - 1350.0f;
                float startZ = 805.0f + offset;
                for (int c = 0; c < coinSequenceCount; c++) {
                    Coin destCoin;
                    destCoin.position = (Vector3){ -700.0f, g.coinFloatHeight, startZ + c * spacing };
                    destCoin.collected = false;
                    destCoin.rotation = (float)GetRandomValue(0, 360);
                    g.coins.push_back(destCoin);
                }
            }

            g.nextCoinSpawnZ += coinSequenceCount * spacing + (float)GetRandomValue(15, 30);
        }

        // Coin Rotation Animation & Collection Detection
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
            if (isOnDestinationRoad) {
                if (g.coins[i].position.z < g.player.position.z - 30.0f) shouldErase = true;
            } else if (isOnSegment3) {
                if (g.coins[i].position.z < g.player.position.z - 30.0f) shouldErase = true;
            } else if (isOnTurnedLeftPath) {
                if (g.coins[i].position.x < g.player.position.x - 30.0f) shouldErase = true;
            } else if (isOnTurnedRightPath) {
                if (g.coins[i].position.x > g.player.position.x + 30.0f) shouldErase = true;
            } else {
                if (g.coins[i].position.z < g.player.position.z - 30.0f) shouldErase = true;
            }
            if (shouldErase) {
                g.coins.erase(g.coins.begin() + i);
            } else {
                i++;
            }
        }
    }

    // Update Skeletal Model Running or Hanging Animation across all gameplay states
    if (!g.isGameOver) {
        UpdatePlayerAnimation(g.player, !g.isZiplining, g.isZiplining, deltaTime);
    }

    // --------------------------------------------------------------------------
    // 4. Camera Tracking (Smooth follow across all routes & during zipline)
    // --------------------------------------------------------------------------
    Vector3 desiredCamPos;
    Vector3 desiredCamTarget;
    bool isOnDestinationRoad = (g.player.position.x <= -650.0f && g.player.position.z >= 795.0f);
    bool isOnSegment3 = (!isOnDestinationRoad && g.player.position.x >= 347.0f && g.player.position.z >= 503.0f);
    bool isOnTurnedLeftPath  = (!isOnDestinationRoad && !isOnSegment3 && g.player.position.x >= 3.0f && g.player.position.z >= 495.0f);
    bool isOnTurnedRightPath = (!isOnDestinationRoad && g.player.position.x <= -3.0f && g.player.position.z >= 495.0f);

    if (g.isZiplining) {
        Vector3 tangent = GetZiplineTangent(g.ziplineProgress);
        desiredCamPos = Vector3Add(g.player.position, (Vector3){ -tangent.x * 8.5f, 4.0f, -tangent.z * 8.5f });
        desiredCamTarget = Vector3Add(g.player.position, (Vector3){ tangent.x * 6.0f, 1.8f, tangent.z * 6.0f });
    } else if (isOnDestinationRoad) {
        desiredCamPos = (Vector3){ g.player.position.x, g.player.position.y + 4.0f, g.player.position.z - 8.0f };
        desiredCamTarget = (Vector3){ g.player.position.x, g.player.position.y + 1.0f, g.player.position.z + 8.0f };
    } else if (isOnSegment3) {
        desiredCamPos = (Vector3){ g.player.position.x, g.player.position.y + 4.0f, g.player.position.z - 8.0f };
        desiredCamTarget = (Vector3){ g.player.position.x, g.player.position.y + 1.0f, g.player.position.z + 8.0f };
    } else if (isOnTurnedLeftPath) {
        desiredCamPos = (Vector3){ g.player.position.x - 8.0f, g.player.position.y + 4.0f, g.player.position.z };
        desiredCamTarget = (Vector3){ g.player.position.x + 8.0f, g.player.position.y + 1.0f, g.player.position.z };
    } else if (isOnTurnedRightPath) {
        desiredCamPos = (Vector3){ g.player.position.x + 8.0f, g.player.position.y + 4.0f, g.player.position.z };
        desiredCamTarget = (Vector3){ g.player.position.x - 8.0f, g.player.position.y + 1.0f, g.player.position.z };
    } else {
        desiredCamPos = (Vector3){ g.player.position.x, g.player.position.y + 4.0f, g.player.position.z - 8.0f };
        desiredCamTarget = (Vector3){ g.player.position.x, g.player.position.y + 1.0f, g.player.position.z + 8.0f };
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
            DrawZiplineRoute();

            if (g.isZiplining) {
                // Trolley wheel & housing mounted on the cable
                Vector3 cablePos = GetZiplinePoint(g.ziplineProgress);
                DrawCube(cablePos, 0.4f, 0.18f, 0.4f, (Color){ 240, 185, 50, 255 });
                DrawCubeWires(cablePos, 0.4f, 0.18f, 0.4f, (Color){ 70, 45, 20, 255 });

                // Handle bar connecting down from trolley to hands (~0.25m below cable)
                Vector3 handlePos = { cablePos.x, cablePos.y - 0.25f, cablePos.z };
                DrawCube(handlePos, 0.65f, 0.08f, 0.08f, (Color){ 190, 150, 60, 255 });
                DrawCubeWires(handlePos, 0.65f, 0.08f, 0.08f, (Color){ 50, 35, 15, 255 });

                // Dual suspension cables from trolley to handle
                DrawLine3D((Vector3){ cablePos.x - 0.2f, cablePos.y, cablePos.z }, (Vector3){ handlePos.x - 0.2f, handlePos.y, handlePos.z }, (Color){ 160, 140, 100, 255 });
                DrawLine3D((Vector3){ cablePos.x + 0.2f, cablePos.y, cablePos.z }, (Vector3){ handlePos.x + 0.2f, handlePos.y, handlePos.z }, (Color){ 160, 140, 100, 255 });
            }

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
        DrawText("SPACE / Tap Top: Jump / Grab Zipline", 30, 104, 13, (Color){ 255, 215, 0, 255 });

        float currentDistance = 0.0f;
        if (g.isZiplining) {
            currentDistance = 500.0f + 350.0f + (g.ziplineProgress * GameState::ZIPLINE_LENGTH);
        } else if (isOnDestinationRoad) {
            currentDistance = 500.0f + 350.0f + GameState::ZIPLINE_LENGTH + (g.player.position.z - 800.0f);
        } else if (isOnSegment3) {
            currentDistance = 500.0f + 347.0f + (g.player.position.z - 503.0f);
        } else if (isOnTurnedLeftPath) {
            currentDistance = 500.0f + (g.player.position.x - 3.0f);
        } else if (isOnTurnedRightPath) {
            currentDistance = 500.0f + (-g.player.position.x - 3.0f);
        } else {
            currentDistance = g.player.position.z;
        }
        DrawText(TextFormat("Distance: %.1f m", currentDistance), 30, 128, 16, YELLOW);
        DrawText(TextFormat("Coins: %d", g.score), 30, 150, 16, (Color){ 255, 215, 0, 255 });
        const char* modeStr = g.isGameOver ? "GAME OVER" : (g.isZiplining ? "ZIPLINING" : TextFormat("%.1f m/s", g.forwardSpeed));
        DrawText(TextFormat("Speed: %s", modeStr), 30, 172, 16, g.isGameOver ? RED : (g.isZiplining ? (Color){ 100, 240, 255, 255 } : GREEN));
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
