import subprocess
import os

for filename in os.listdir("textures"):
    filename_segments = filename.split(".")
    assert(len(filename_segments) == 2)
    base_name, extension = filename_segments
    # skip existing dds files
    if extension != "png":
        continue

    if "normal" in base_name:
        subprocess.run(["nvcompress", "-bc5", "-normal", "-dds10", f"./textures/{filename}", f"{base_name}.dds"])
    elif "base_color" in base_name:
        subprocess.run(["nvcompress", "-bc7", "-color", "-dds10", f"./textures/{filename}", f"{base_name}.dds"])
    elif "roughness" in base_name or "metallic" in base_name:
        subprocess.run(["nvcompress", "-bc4", "-dds10", f"./textures/{filename}", f"{base_name}.dds"])
# python ../Exporter/compress_textures.py
