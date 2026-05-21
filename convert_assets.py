import os
from PIL import Image

def convert_webp_to_png(directory):
    if not os.path.exists(directory):
        print(f"Directory {directory} does not exist.")
        return
        
    converted = 0
    for filename in os.listdir(directory):
        if filename.lower().endswith('.webp'):
            webp_path = os.path.join(directory, filename)
            png_filename = filename[:-5] + '.png'
            png_path = os.path.join(directory, png_filename)
            
            # Avoid re-converting if PNG already exists and is valid
            if os.path.exists(png_path):
                continue
                
            try:
                with Image.open(webp_path) as img:
                    img.save(png_path, 'PNG')
                converted += 1
            except Exception as e:
                print(f"Error converting {filename}: {e}")
                
    print(f"Converted {converted} webp files to png in {directory}")

if __name__ == '__main__':
    workspace_assets = r"c:\Users\lenor\Documents\ear-pc\native_tray\assets"
    build_assets = r"c:\Users\lenor\Documents\ear-pc\native_tray\build\Release\assets"
    
    convert_webp_to_png(workspace_assets)
    convert_webp_to_png(build_assets)
