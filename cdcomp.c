#include "voyager_decomp.h"

int main(int argc, char **argv)
{
	int i;
	char outpath[PATH_MAX*4];

	fprintf_rl(stdout, "Voyager Image Decompression Program (modernised)\n");

	if (argc == 1)
	{
		fprintf_rl(stderr, "Provide a list of files to convert as arguments, drag-and-drop works too. The output files will be given a .tif extension.\n");
		return 0;
	}

	// Convert each file
	for (int ia=1; ia < argc; ia++)
	{
		fprintf_rl(stdout, "Converting %s", argv[ia]);

		// Decompress into array
		buffer_t inbuf = buf_load_raw_file(argv[ia]);
		uint8_t *data = voyager_decompress_buffer_to_array(inbuf);
		free_buf(&inbuf);

		// Convert to raster structure
		raster_t r = make_raster(NULL, xyi(800, 800), XYI0, IMAGE_USE_FRGB);
		for (i=0; i < 800*800; i++)
			r.f[i] = make_grey_f((double) data[i] * (1./255.));

		// Save to TIFF file
		remove_extension_from_path(outpath, argv[ia]);
		sprintf(&outpath[strlen(outpath)], ".tif");
		save_image_tiff(outpath, r.f, r.dim, 4, 1, 32);

		free_raster(&r);
		free(data);
		fprintf_rl(stdout, "\n");
	}
}
