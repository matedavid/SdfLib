import mitsuba as mi
import sys

if "cuda_ad_rgb" in mi.variants():
    mi.set_variant("cuda_ad_rgb")
else:
    mi.set_variant("llvm_ad_rgb")

print(f"Using variant: {mi.variant()}")

scene = mi.load_file(sys.argv[1])

img = mi.render(scene)

bmp = mi.Bitmap(img)
bmp = bmp.convert(mi.Bitmap.PixelFormat.RGBA, mi.Struct.Type.UInt8, srgb_gamma=True)
print(bmp)

bmp.write("output.png")
