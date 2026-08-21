from pygltflib import GLTF2
import struct
import numpy as np

gltf = GLTF2.load("assets/player/character.glb")

print(f"Nodes count: {len(gltf.nodes)}")
for i, node in enumerate(gltf.nodes):
    if node.name and ("Hips" in node.name or "Root" in node.name or "Armature" in node.name or "mixamorig" in node.name):
        print(f"Node [{i}]: name='{node.name}'")

print(f"\nAnimations count: {len(gltf.animations)}")
for a_idx, anim in enumerate(gltf.animations):
    print(f"\nAnimation [{a_idx}]: '{anim.name}' ({len(anim.channels)} channels, {len(anim.samplers)} samplers)")
    for c_idx, ch in enumerate(anim.channels):
        node_name = gltf.nodes[ch.target.node].name if ch.target.node < len(gltf.nodes) else "Unknown"
        if ch.target.path == "translation":
            print(f"  Channel [{c_idx}]: Target Node [{ch.target.node}] '{node_name}', Path='{ch.target.path}'")

            # Extract translation keyframes from binary buffer
            sampler = anim.samplers[ch.sampler]
            input_accessor = gltf.accessors[sampler.input]
            output_accessor = gltf.accessors[sampler.output]
            print(f"    Keyframes count: {output_accessor.count}")

            # Read binary buffer
            bv = gltf.bufferViews[output_accessor.bufferView]
            buffer = gltf.buffers[bv.buffer]
            data = gltf.get_data_from_buffer_uri(buffer.uri) if buffer.uri else gltf.binary_blob()
            offset = (bv.byteOffset or 0) + (output_accessor.byteOffset or 0)
            
            # Extract vec3 positions
            translations = []
            for k in range(output_accessor.count):
                pos = struct.unpack_from("<fff", data, offset + k * 12)
                translations.append(pos)

            t_arr = np.array(translations)
            print(f"    Start Pos (Frame 0): ({t_arr[0][0]:.3f}, {t_arr[0][1]:.3f}, {t_arr[0][2]:.3f})")
            print(f"    End Pos   (Frame {output_accessor.count-1}): ({t_arr[-1][0]:.3f}, {t_arr[-1][1]:.3f}, {t_arr[-1][2]:.3f})")
            print(f"    Z-Displacement: {t_arr[-1][2] - t_arr[0][2]:.3f}m")
