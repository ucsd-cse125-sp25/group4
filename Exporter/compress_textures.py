import subprocess
import os

INPUT_DIR = "textures_raw"
OUTPUT_DIR = "textures"

for filename in os.listdir(INPUT_DIR):
    filename_segments = filename.split(".")
    assert(len(filename_segments) == 2)
    base_name, extension = filename_segments

    input_path = f"./{INPUT_DIR}/{base_name}.png"
    output_path = f"./{OUTPUT_DIR}/{base_name}.dds"
    if os.path.exists(output_path):
        continue
    
    # skip existing dds files
    if extension != "png":
        continue

    
    if "normal" in base_name:
        subprocess.run(["nvcompress", "-bc5", "-normal", "-dds10", input_path, output_path])
    elif "base_color" in base_name:
        subprocess.run(["nvcompress", "-bc7", "-color", "-dds10", input_path, output_path])
    elif "roughness" in base_name or "metallic" in base_name:
        subprocess.run(["nvcompress", "-bc4", "-dds10", input_path, output_path])
# python ../Exporter/compress_textures.py
