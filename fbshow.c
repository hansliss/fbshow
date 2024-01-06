#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/ioctl.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<linux/fb.h>
#include<stdint.h>
#include<assert.h>
#include<ft2build.h>
#include FT_FREETYPE_H
#include<wand/magick_wand.h>

#define BUFSIZE 8192

#undef DEBUG

#define min(x, y) (((x) > (y))?(y):(x))
#define max(x, y) (((x) < (y))?(y):(x))

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -F <framebuffer> [-i <image file>] [-f <fontfile> -s <fontsize> <x:y:col:text> ...]\n", progname);
}

// Render a single character on the screen using the selected font and colour.
// The screen_x and screen_y parameters get updated automatically.
int showChar(uint16_t *fbdata,
	     int width,
	     int height,
	     int screen_bpp,
	     int *screen_x,
	     int *screen_y,
	     FT_Face face,
	     FT_ULong character,
	     int col) {
  // Find the requested character in the font
  FT_UInt gi;
  if ((gi = FT_Get_Char_Index(face, character)) == 0) {
    fprintf(stderr, "Error getting char index\n");
    return -11;
  }
  
  // Load the glyph and extract metrics from it
  FT_Load_Glyph(face, gi, FT_LOAD_DEFAULT);
  int bbox_ymax = face->bbox.yMax / 64;
  int bbox_ymin = face->bbox.yMin / 64;
  int glyph_width = face->glyph->metrics.width / 64;
  int advance = face->glyph->metrics.horiAdvance / 64;
  int x_off = (advance - glyph_width) / 2;
  int y_off = bbox_ymax - bbox_ymin - face->glyph->metrics.horiBearingY / 64;

  // Check whether it's time to advance to a new line
  if (*screen_x + advance > width) {
    *screen_x = 0;
    *screen_y += face->glyph->metrics.vertAdvance / 64;
  }

  // Render the glyph
  FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

  //  fprintf(stderr, "Glyph: %c, bbox_ymin = %d, bbox_ymax = %d, glyph_width = %d, advance = %d, x_off = %d, horiBearingY = %d, y_off = %d\n",
  //   (char)character, face->bbox.yMin / 64, bbox_ymax, glyph_width, advance, x_off, face->glyph->metrics.horiBearingY / 64, y_off);

  // Now traverse the rendered glyph and transfer the pixels to the framebuffer
  // using the provided colour value
  for(int glyph_y = 0; glyph_y < (int)face->glyph->bitmap.rows; glyph_y++) {
    // row_offset is the distance from the top of the framebuffer
    //   of the text bounding box
    int row_offset = *screen_y + glyph_y + y_off;
    for(int glyph_x = 0; glyph_x < (int)face->glyph->bitmap.width; glyph_x++) {
      unsigned char p = face->glyph->bitmap.buffer [glyph_y * face->glyph->bitmap.pitch + glyph_x];
      
      // Don't draw a zero value, unless you want to fill the bounding
      //   box with black.
      int fb_offs;
      fb_offs = *screen_x + glyph_x + x_off + row_offset * width;
      if (fb_offs >=0 && fb_offs < width * height) {
	if (p) {
	  if (screen_bpp == 16) {
	    fbdata[fb_offs] = col;
	  } else if (screen_bpp == 1) {
	    if (col) {
	      fbdata[fb_offs / 16] |= 1 << (fb_offs % 16);
	    } else {
	      fbdata[fb_offs / 16] &= (~(1 << (fb_offs % 16)));
	    }
	  }
	} else {
	  //	  fbdata[fb_offs] = 0x7bef;
	}
      }
    }
  }
  *screen_x += advance;
  // Move the screen_x position, ready for the next character.
  return 1;
}

// Use showChar to draw all the characters from the given string on the framebuffer
// The screen_x and screen_y values will be advanced to after the drawn string.
int showString(uint16_t *fbdata,
	       int width,
	       int height,
	       int screen_bpp,
	       int *screen_x,
	       int *screen_y,
	       FT_Face face,
	       char *s,
	       int col) {
  for(int i=0; i<strlen(s); i++) {
    if (showChar(fbdata, width, height, screen_bpp, screen_x, screen_y, face, s[i], col) < 0) {
      fprintf(stderr, "Error rendering character\n");
      exit(-13);
    }
  }
  return 1;
}

int main(int argc, char *argv[]) {
  FT_Library ft;
  FT_Face face;
  char *imageFile=NULL;
  int o;
  int x, y, col;
  int doresize = 1;
  static char buf[BUFSIZE];
  char *fbpath = NULL;
  char *fontpath = NULL;
  int fontsize = 0;

  while ((o=getopt(argc, argv, "i:rf:F:s:")) != -1) {
    switch(o) {
    case 'i':
      imageFile=optarg;
      break;
    case 'r':
      doresize = 0;
      break;
    case 'F':
      fbpath = optarg;
      break;
    case 'f':
      fontpath = optarg;
      break;
    case 's':
      fontsize = atoi(optarg);
      break;
    default:
      usage(argv[0]);
      return -1;
    }
  }

  if (fbpath == NULL) {
    usage(argv[0]);
    return -1;
  }

  // Some extra flags when opening the framebuffer to make sure the data is written
  // back properly.
  int fbfd = open(fbpath, O_RDWR | O_SYNC | O_DSYNC);
  if (fbfd < 0) {
    perror(fbpath);
    return -2;
  }
  struct fb_var_screeninfo vinfo;
  struct fb_fix_screeninfo finfo;
    
  ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
  ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);

  int fb_width = vinfo.xres;
  int fb_height = vinfo.yres;
  int fb_bpp = vinfo.bits_per_pixel;

  vinfo.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;

  ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo);

  //    assert((fb_bpp==16));

  // Total length of the framebuffer in bytes
  int fb_data_size = finfo.smem_len;

#ifdef DEBUG
  fprintf(stderr, "%d x %d, %d bpp, %d total bytes\n", fb_width, fb_height, fb_bpp, fb_data_size);
#endif

  // Map the framebuffer
  uint16_t *fbdata = mmap(0, fb_data_size, 
			  PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, (off_t)0);

  if (fbdata == MAP_FAILED) {
    perror("mmap()");
    return -20;
  }

  // An image file was given, so we need to open it, possibly resize it and draw it on
  // the framebuffer.
  if (imageFile) {
    MagickWand *m_wand = NULL;
      
    int im_width, im_height, new_width, new_height;
      
    MagickWandGenesis();
      
    m_wand = NewMagickWand();

    MagickReadImage(m_wand,imageFile);
	
    // Get the image's width and height
    im_width = MagickGetImageWidth(m_wand);
    im_height = MagickGetImageHeight(m_wand);
    float im_ratio = (float)im_width / im_height;
    float fb_ratio = (float)fb_width / fb_height;
    if (fb_ratio > im_ratio) {
      new_width = fb_width;
      new_height = im_height * fb_width / im_width;
    } else {
      new_height = fb_height;
      new_width = im_width * fb_height / im_height;
    }
    if (doresize) {
      // Resize the image using the Lanczos filter
      // The blur factor is a "double", where > 1 is blurry, < 1 is sharp
      // I haven't figured out how you would change the blur parameter of MagickResizeImage
      // on the command line so I have set it to its default of one.
#ifdef DEBUG
      fprintf(stderr, "Resizing from (%d,%d) to (%d,%d)\n", im_width, im_height, new_width, new_height);
#endif
      if (MagickResizeImage(m_wand,new_width,new_height,LanczosFilter,0.5) == MagickFalse) {
	fprintf(stderr, "Resize failed\n");
	return -21;
      }
#ifdef DEBUG
      fprintf(stderr, "Size is %ld x %ld\n", MagickGetImageWidth(m_wand), MagickGetImageHeight(m_wand));
#endif
    }

    if (new_height > fb_height) {
      if (MagickCropImage(m_wand, fb_width, fb_height, (new_width - fb_width) / 2, (new_height - fb_height) / 2) == MagickFalse) {
	fprintf(stderr, "Crop failed\n");
	return -21;
      }
    }
#ifdef DEBUG
    fprintf(stderr, "Size is %ld x %ld\n", MagickGetImageWidth(m_wand), MagickGetImageHeight(m_wand));
#endif

    // Handle 16-bit framebuffers and 1-bit framebuffers slightly different. No other depths
    // are supported at this point.
    if (fb_bpp == 16) {
      if (MagickSetImageFormat(m_wand, "RGB") == MagickFalse) {
	fprintf(stderr, "SetImageFormat failed\n");
	return -21;
      }
      if (MagickSetImageDepth(m_wand, 24) == MagickFalse) {
	fprintf(stderr, "SetImageDepth failed\n");
	return -21;
      }
      unsigned char *xxx = (unsigned char *)malloc(fb_width * fb_height * 3);
	
      if (MagickExportImagePixels(m_wand, 0, 0, fb_width, fb_height, "RGB", CharPixel, xxx) == MagickFalse) {
	fprintf(stderr, "Export failed\n");
	return -21;
      }
      for (int i=0; i < fb_width * fb_height; i++) {
	fbdata[i] = ((xxx[i*3] & 0xF8) >> 3) << 11 | ((xxx[i*3+1] & 0xFC) >> 2) << 5 | (xxx[i*3 + 2] & 0xF8) >> 3;
	// fbdata[i] = ((fbdata[i] & 0xff00) >> 8) | ((fbdata[i] & 0x00ff) << 8);
      }
    } else if (fb_bpp == 1) {
      if (MagickSetImageFormat(m_wand, "MONO") == MagickFalse) {
	fprintf(stderr, "SetImageFormat failed\n");
	return -21;
      }
      if (MagickSetImageDepth(m_wand, 1) == MagickFalse) {
	fprintf(stderr, "SetImageDepth failed\n");
	return -21;
      }
	
      unsigned char *xxx = (unsigned char *)malloc(fb_width * fb_height);

      if (MagickExportImagePixels(m_wand, 0, 0, fb_width, fb_height, "I", CharPixel, xxx) == MagickFalse) {
	fprintf(stderr, "Export failed\n");
	return -21;
      }

      //	FILE *x = fopen("foo", "w");
      //	fwrite(xxx, 1, fb_width * fb_height, x);
      //	fclose(x);

      for (int i=0; i < fb_width * fb_height / 16; i++) {
	fbdata[i] = 0;
	for (int j=7; j>=0; j--) {
	  fbdata[i] = (fbdata[i] << 1) | ((xxx[i * 16 + j + 8] & 0x80) >> 7);
	}
	for (int j=15; j>=8; j--) {
	  fbdata[i] = (fbdata[i] << 1) | ((xxx[i * 16 + j - 8] & 0x80) >> 7);
	}
      }
      //	x = fopen("bar", "w");
      //	fwrite(fbdata, 1, fb_width * fb_height / 8, x);
      //	fclose(x);
    }

    MagickWandTerminus();
  } else {
    // If no image file was given, just clear the framebuffer.
    memset(fbdata, 0xff, fb_data_size);
  }

  // If any text output was specified, load a font file and output the text
  if (optind < argc) {
    if (fontpath == NULL || fontsize <= 0) {
      fprintf(stderr, "Font file path or fontsize missing\n");
      usage(argv[0]);
      return -1;
    }
    if (FT_Init_FreeType(&ft) != 0) {
      fprintf(stderr, "Error initializing Freetype\n");
      return -10;
    }
    if (FT_New_Face(ft, fontpath, 0, &face) != 0) {
      fprintf(stderr, "Error loading font\n");
      return -11;
    }
    if (FT_Set_Pixel_Sizes(face, 0, fontsize) != 0) {
      fprintf(stderr, "Error setting font size\n");
      return -12;
    }
    for (int i=optind; i<argc; i++) {
      memset(buf, 0, BUFSIZE);
      if (sscanf(argv[i], "%i:%i:%i:%40c", &x, &y, &col, buf) == 4) {
#ifdef DEBUG
	fprintf(stderr, "Printing \"%s\" in color %d at (%d,%d)\n", buf, col, x, y);
#endif
	showString(fbdata, fb_width, fb_height, fb_bpp, &x, &y, face, buf, col);
      }
    }
    FT_Done_FreeType(ft);

    // Everything done, so unmap the framebuffer
    if (munmap(fbdata, fb_data_size) < 0) {
      perror("munmap()");
      return -40;
    }

    // Here comes the black magic. At one point, the OLED display stopped being updated
    // automatically, unless something was actively written to it. So this is the
    // kludge to fix that.
    lseek(fbfd, 0, SEEK_SET);
    write(fbfd, "\0", 1);
    close(fbfd);
  }
  return 0;
}
