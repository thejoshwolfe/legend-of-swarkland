# simplepng.py

Simple (and inefficient) png manipulation library in a single python3 file.

0. Reads and writes the binary structure of PNG image files in pure python.
0. Can flip/rotate images.
0. Can composite images together using alpha blending.
0. Runs pretty slowly due to some heavy python code running for each pixel.

## Status

Still in development.

## Example Usage

```py
import simplepng

# read two png files
with open("background.png", "rb") as f:
  image = simplepng.read_png(f, verbose=True)
with open("sprite.png", "rb") as f:
  other_image = simplepng.read_png(f, verbose=True)

# paste the second image onto the first one using alpha blending
image.paste(other_image, 0, 0, 0, 0)

# write the result to a new file
with open("scene.png", "wb") as f:
  simplepng.write_png(f, image)
```

## Running the tests

Run this command in this project's root directory:

```
python3 test/test.py
```

## Limitations

In memory, images are always stored in RGBA format with 8 bits per channel (32 bits per pixel).
This means that reading an image with 16-bit channels will lose information.

When reading images, only chunk types IHDR, IPLT, IDAT, and IEND are recognized; all others are ignored.
TODO: Recognize the tRNS ancillary chunk for describing the alpha channel.

When encoding png images, this library makes very simple and naive (i.e. suboptimal) encoding decisions:

* Channel layout is always RGBA with 8 bits per channel (32 bits per pixel).
* Every scanline uses filter type 1 (difference from previous value).

Some experimental evidence using GIMP to re-encode images created with this library shows
that this naivety inflates images by about 20% for some images.
Of course, this depends heavily on the image being encoded, so YMMV.
