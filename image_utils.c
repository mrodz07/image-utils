#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <string.h>
#include <time.h>

#define OMP_NUM_THREADS 75
#define METADATA_SIZE 138

typedef struct pixel {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
} pixelType;

typedef struct image {
  unsigned char *metadata;
  long pixel_num;
  pixelType *pixels;
  char *img_path;
  long width, height;
} imageType;

int die(char *mssg)
{
  fprintf(stderr, "%s\n", mssg);
  exit(1);
}

pixelType *pixelCreate(unsigned char red, unsigned char green, unsigned char blue)
{
  pixelType *tmp_pixel = malloc(sizeof(pixelType));

  tmp_pixel -> red = red;
  tmp_pixel -> green = green;
  tmp_pixel -> blue = blue;

  return tmp_pixel;
}

int pixelTrash(pixelType *pixel)
{
  if (pixel == NULL) die("Null argument passed to rgbTrash");

  pixel = NULL;
  free(pixel);

  return 0;
}

int imageTrash(imageType *image)
{
  if (image == NULL) die("Null argument passed to imageTrash");

  image -> img_path = NULL;
  image -> width = 0;
  image -> height = 0;

  for (long i = 0; i < image -> pixel_num; i++) {
    pixelTrash(&image -> pixels[i]);
  }

  image = NULL;

  free(image);

  return 0;
}

unsigned char* getImageMetaData(imageType *image)
{
  FILE *ip = fopen(image -> img_path, "rb");

  if (image == NULL) die("getImageMetaData called with null argument");
  if (ip == NULL) die("Invalid image path on getImageMetaData");

  unsigned char *metadata = malloc(sizeof(unsigned char) * METADATA_SIZE);

  for (int i = 0; i < METADATA_SIZE; i++) {
    metadata[i] = fgetc(ip);
  }

  fclose(ip);

  return metadata;
}

pixelType* getImagePixels(imageType *image)
{
  FILE *ip = fopen(image -> img_path, "rb");

  if (image == NULL || image -> img_path == NULL) die("getImagePixels with null arguments");
  if (image -> pixel_num == 0) die("pixel_num not valid on getImagePixels");
  if (ip == NULL) die("Invalid image path on getImagePixels");

  pixelType *pixels = malloc(image -> pixel_num * sizeof(pixelType));

  unsigned char blue, green, red;

  // Posicionamos el apuntador de la imagen a la posición después de leer los metadatos
  fseek(ip, METADATA_SIZE, SEEK_SET);

  for (long i = 0; i < image -> pixel_num; i++) {
    red = fgetc(ip);
    green = fgetc(ip);
    blue = fgetc(ip);

    pixels[i] = *pixelCreate(red, green, blue); 
  }

  fclose(ip);

  return pixels;
}

imageType *imageCreate(char *image_name)
{
  imageType *tmp_img = malloc(sizeof(imageType));

  if ((fopen(image_name,"rb")) == 0) die("Image not found"); 

  tmp_img -> img_path = image_name;

  tmp_img -> metadata = getImageMetaData(tmp_img);

  tmp_img -> width = (long)tmp_img -> metadata[20] * 65536 + (long)tmp_img -> metadata[19] * 256 + (long)tmp_img -> metadata[18];
  tmp_img -> height = (long)tmp_img -> metadata[24] * 65536 + (long)tmp_img -> metadata[23] * 256+ (long)tmp_img -> metadata[22];
  tmp_img -> pixel_num = tmp_img -> width * tmp_img -> height;

  tmp_img -> pixels = getImagePixels(tmp_img);

  return tmp_img;
}

imageType *imageCopy(const imageType *image)
{
  imageType *img_tmp = malloc(sizeof(imageType));
  img_tmp -> metadata = malloc(sizeof(unsigned char) * METADATA_SIZE);
  img_tmp -> pixels = malloc(sizeof(pixelType) * image -> pixel_num);
  img_tmp -> img_path = malloc(sizeof(char) * strlen(image -> img_path));

  strcpy(img_tmp -> img_path, image -> img_path);
  memcpy(img_tmp -> metadata, image -> metadata, METADATA_SIZE * sizeof(char));
  memcpy(img_tmp -> pixels, image -> pixels, image -> pixel_num * sizeof(pixelType));

  img_tmp -> pixel_num = image -> pixel_num;
  img_tmp -> height  = image -> height;
  img_tmp -> width  = image -> width;

  return img_tmp;
}

imageType *imageModifyGray(const imageType *image)
{
  imageType *img_tmp = imageCopy(image);

  #pragma omp parallel for
  for (long i = 0; i < image -> pixel_num; i++) {
    unsigned char pixel = 0.21 * image -> pixels[i].red + 0.72 * image -> pixels[i].green + 0.07 * image -> pixels[i].blue;
    img_tmp -> pixels[i].red = pixel;
    img_tmp -> pixels[i].green = pixel;
    img_tmp -> pixels[i].blue = pixel;
  }

  return img_tmp;
}

imageType *imageModifyMirror(const imageType *image, char *axis)
{
  imageType *img_tmp = imageCopy(image);

  if (strcmp(axis, "horizontal") == 0) {
    #pragma omp parallel for
    for (long i = 0; i < image -> height; i++) {
      for (long j = i * image -> width + image -> width, k = i * image -> width; j >= i * image -> width ; j--, k++) {
        img_tmp -> pixels[k].red = image -> pixels[j].red;
        img_tmp -> pixels[k].green = image -> pixels[j].green;
        img_tmp -> pixels[k].blue = image -> pixels[j].blue;
      }
    }
  } else if (strcmp(axis, "vertical") == 0) {
    //#pragma omp parallel for reduction(+ : t)
    for (long i = image -> height - 1, t = 0; i >= 0; i--, t++) {
      for (long j = i * image -> width, k = t * image -> width; j <= i * image -> width + image -> width; j++, k++) {
        img_tmp -> pixels[k].red = image -> pixels[j].red;
        img_tmp -> pixels[k].green = image -> pixels[j].green;
        img_tmp -> pixels[k].blue = image -> pixels[j].blue;
      }
    }
  } else {
    die("image imageModifyMirror called with unvalid axis");
  }

  return img_tmp;

}

pixelType *pixelGetRadiousMean(long index, long radious, const pixelType *pixels, long pixel_width, long pixel_height, char *components)
{
  if (pixels == NULL) die("Null value on pixels sent to pixelGetRadiousMean");
  if (index < 0) die("Position is negative on pixelGetRadiousMean");
  if (components == NULL) die("Null component sent to pixelGetRadiousMean");

  int mean_red = 0, mean_blue = 0, mean_green = 0;
  int x = (index % pixel_width) - radious / 2;
  int y = (index / pixel_width) - radious / 2;
  int total_pixels = radious * radious;

  #pragma omp parallel sections
  {
    #pragma omp section
    {
      for (long i = y; i < y + radious; i++) {
        for (long j = x; j < x + radious; j++) {
          if (i >= 0 && i < pixel_height && j >= 0 && j < pixel_width) {
            mean_red += pixels[i * pixel_width + j].red;
          }
        }
      }
    }
    #pragma omp section
    {
      for (long i = y; i < y + radious; i++) {
        for (long j = x; j < x + radious; j++) {
          if (i >= 0 && i < pixel_height && j >= 0 && j < pixel_width) {
            mean_green += pixels[i * pixel_width + j].green;
          }
        }
      }
    }
    #pragma omp section
    {
      for (long i = y; i < y + radious; i++) {
        for (long j = x; j < x + radious; j++) {
          if (i >= 0 && i < pixel_height && j >= 0 && j < pixel_width) {
            mean_blue += pixels[i * pixel_width + j].blue;
          }
        }
      }
    }
  }

  return pixelCreate(mean_red / total_pixels, mean_green / total_pixels, mean_blue / total_pixels);
}

imageType *imageModifyRotate(const imageType *image, int degrees)
{
  if (image == NULL) die("Null sent to imageModifyRotate");
  
  imageType *img_tmp = imageCopy(image);
  double radians = (degrees * M_PI) / 180;
  int x_center = image -> width / 2;
  int y_center = image -> height / 2;

  #pragma omp parallel for collapse(2)
  for (long i = 0; i < image -> height; i++) {
    for (long j = 0; j < image -> width; j++) {
      int x = -sin(radians) * (i - y_center) + cos(radians) * (j - x_center) + x_center;
      int y = cos(radians) * (i - y_center) + sin(radians) * (j - x_center) + y_center;
      if (x >= 0 && x < image -> width && y >= 0 && y < image -> height) {
        img_tmp -> pixels[i * image -> width + j] = image -> pixels[y * image -> width + x];
      } else {
        img_tmp -> pixels[i * image -> width + j] = *pixelCreate(0, 0, 0);
      }
    }
  }

  return img_tmp;
}

imageType *imageModifyBlur(const imageType *image, int blur_factor)
{
  if (blur_factor <= 0) die("Negative blur_factor sent in imageModifyBlur");
  
  imageType *img_tmp = imageCopy(image);

  #pragma omp parallel for
  for (long i = 0; i < image -> pixel_num; i++) {
    img_tmp -> pixels[i] = *pixelGetRadiousMean(i, blur_factor, image -> pixels, image -> width, image -> height, "rgb"); 
  }

  return img_tmp;
}

void imageWrite(const imageType *image)
{
  FILE *fp = fopen(image -> img_path, "w");

  if (image == NULL) die("Null value sent to imageWrite");
  if (image -> img_path == NULL) die("Null img_path sent to imageWrite");
  if (fp == NULL) die("Null filePointer on imageWrite");
 
  fwrite(image -> metadata, sizeof(char), METADATA_SIZE, fp);
  fwrite(image -> pixels, sizeof(pixelType), image -> pixel_num, fp);
}

int main()
{
  const double timeStart = omp_get_wtime();

  imageType *img = imageCreate("casitas.bmp");
  imageType *img_rotate = imageModifyRotate(img, 20);
  img_rotate -> img_path = "image_rotated.bmp";

  imageWrite(img_rotate);

  imageTrash(img);
  imageTrash(img_rotate);

  const double timeEnd = omp_get_wtime();
  
  printf("time: %f\n", timeEnd - timeStart);

  return 0;
}
