# Simple sector friendly directory wrapper

You may create an image file as follows:

```sh
./swrap -i directory -o swrap.img
```

All files are aligned up and padded by a defined sector size and there is a
512 byte gap in the beginning to allow a whole sector to be read for metadata.

The first quad (64-bit quad word) contains the size of all files in bytes
summed, all that is needed is to divide it down by the sector size.
