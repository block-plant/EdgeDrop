import struct
import os
import sys

def create_ico(png_paths, output_ico_path):
    images = []
    for path in png_paths:
        with open(path, "rb") as f:
            data = f.read()
        # Parse PNG header to confirm width and height
        # PNG signature: 8 bytes, IHDR chunk: 4 bytes len, 4 bytes type ('IHDR'), 4 bytes w, 4 bytes h
        if data[:8] != b'\x89PNG\r\n\x1a\n':
            raise ValueError(f"File {path} is not a valid PNG")
        w, h = struct.unpack(">II", data[16:24])
        images.append({
            "width": w,
            "height": h,
            "data": data,
            "size": len(data)
        })

    num_images = len(images)
    header = struct.pack("<HHH", 0, 1, num_images)

    offset = 6 + 16 * num_images
    entries = []
    for img in images:
        w_byte = 0 if img["width"] >= 256 else img["width"]
        h_byte = 0 if img["height"] >= 256 else img["height"]
        entry = struct.pack(
            "<BBBBHHII",
            w_byte,
            h_byte,
            0, # color palette count
            0, # reserved
            1, # planes
            32, # bit depth
            img["size"],
            offset
        )
        entries.append(entry)
        offset += img["size"]

    with open(output_ico_path, "wb") as f:
        f.write(header)
        for entry in entries:
            f.write(entry)
        for img in images:
            f.write(img["data"])

    print(f"Created {output_ico_path} with {num_images} images (sizes: {[img['width'] for img in images]}).")

if __name__ == "__main__":
    base = "assets/icons"
    sizes = [16, 24, 32, 48, 64, 128, 256]
    png_paths = [os.path.join(base, f"EdgeDrop-{s}.png") for s in sizes]
    output_ico = os.path.join(base, "EdgeDrop.ico")
    create_ico(png_paths, output_ico)
