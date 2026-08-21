from pygltflib import GLTF2
import struct
import numpy as np

print("Loading assets/player/character.glb...")
gltf = GLTF2.load("assets/player/character.glb")

anim = gltf.animations[1] if len(gltf.animations) > 1 else gltf.animations[0]
print(f"Modifying Animation '{anim.name}'...")

# Find Hips translation channel
hips_node_idx = None
for i, node in enumerate(gltf.nodes):
    if node.name == "mixamorig:Hips":
        hips_node_idx = i
        break

print(f"Hips Node Index: {hips_node_idx}")

for c_idx, ch in enumerate(anim.channels):
    if ch.target.node == hips_node_idx and ch.target.path == "translation":
        sampler = anim.samplers[ch.sampler]
        output_accessor = gltf.accessors[sampler.output]
        bv = gltf.bufferViews[output_accessor.bufferView]
        
        # Get binary blob
        blob = bytearray(gltf.binary_blob())
        offset = (bv.byteOffset or 0) + (output_accessor.byteOffset or 0)
        
        # Extract initial start Z position
        first_pos = struct.unpack_from("<fff", blob, offset)
        initial_z = first_pos[2]
        print(f"Locking Z-translation to initial Z = {initial_z:.3f}m across {output_accessor.count} keyframes...")
        
        for k in range(output_accessor.count):
            k_offset = offset + k * 12
            x, y, z = struct.unpack_from("<fff", blob, k_offset)
            # Preserve X swaying, Y bobbing, lock Z to initial_z (In-Place)
            struct.pack_into("<fff", blob, k_offset, x, y, initial_z)

        gltf.set_binary_blob(bytes(blob))
        print("Successfully baked out Z-translation root motion!")

gltf.save("assets/player/character.glb")
print("Saved modified in-place character.glb successfully!")
