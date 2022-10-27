#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOTypes.h>
#include <mach/mach.h>
#include <dlfcn.h>
#include <CoreGraphics/CoreGraphics.h>
#define WHITE 0xffffffff
#define BLACK 0x00000000
typedef IOReturn IOMobileFramebufferReturn;
typedef struct __IOMobileFramebuffer *IOMobileFramebufferRef;
typedef CGSize IOMobileFramebufferDisplaySize;
typedef struct __IOSurface *IOSurfaceRef;

#include "iso_font.c" // https://github.com/apple/darwin-xnu/blob/main/osfmk/console/iso_font.c

#define ISO_CHAR_MIN    0x00
#define ISO_CHAR_MAX    0xFF
#define ISO_CHAR_WIDTH  8
#define ISO_CHAR_HEIGHT 16

void write_char(void *base, int bytesPerRow, int x, int y, char c, int color, int scale) {
    for (int i = 0; i < ISO_CHAR_HEIGHT; i++) {
        for (int j = 0; j < ISO_CHAR_WIDTH; j++) {
            if (iso_font[(c - ISO_CHAR_MIN) * ISO_CHAR_HEIGHT + i] & (1 << (ISO_CHAR_WIDTH - j - 1))) {
                int pixel = (x + j) * 4 + (y + i) * bytesPerRow;
                pixel = (x + (ISO_CHAR_WIDTH - j - 1)) * 4 + (y + i) * bytesPerRow;
                scale = 1;
                ((int *)base)[pixel / 4] = color;
            }
        }
    }
}

void write_string(void *base, int bytesPerRow, int x, int y, char *string, int color, int scale) {
    int i = 0;
    while (string[i] != '\0') {
        write_char(base, bytesPerRow, x + i * 8, y, string[i], color, scale);
        i++;
    }
}

void make_progress_bar(void *base, int bytesPerRow, int x, int y, int width, int height, int percent, int color) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int pixel = (x + j) * 4 + (y + i) * bytesPerRow;
            if (j < (width * percent) / 100) {
                ((int *)base)[pixel / 4] = color;
            } else {
                ((int *)base)[pixel / 4] = 0x00000000;
            }
        }
    }
}

int init_display()
{
    void *IOMobileFramebuffer = dlopen("/System/Library/PrivateFrameworks/IOMobileFramebuffer.framework/IOMobileFramebuffer", RTLD_LAZY);
    IOMobileFramebufferReturn (*IOMobileFramebufferGetMainDisplay)(IOMobileFramebufferRef *pointer) = dlsym(IOMobileFramebuffer, "IOMobileFramebufferGetMainDisplay");
    IOMobileFramebufferRef display;
    IOMobileFramebufferGetMainDisplay(&display);
    IOMobileFramebufferReturn (*IOMobileFramebufferGetDisplaySize)(IOMobileFramebufferRef pointer, IOMobileFramebufferDisplaySize *size) = dlsym(IOMobileFramebuffer, "IOMobileFramebufferGetDisplaySize");
    IOMobileFramebufferDisplaySize size;
    IOMobileFramebufferGetDisplaySize(display, &size);
    IOMobileFramebufferReturn (*IOMobileFramebufferGetLayerDefaultSurface)(IOMobileFramebufferRef pointer, int surface, IOSurfaceRef *buffer) = dlsym(IOMobileFramebuffer, "IOMobileFramebufferGetLayerDefaultSurface");
    IOSurfaceRef buffer;
    IOMobileFramebufferGetLayerDefaultSurface(display, 0, &buffer);
    IOMobileFramebufferReturn (*IOMobileFramebufferSwapBegin)(IOMobileFramebufferRef pointer, int *token) = dlsym(IOMobileFramebuffer, "IOMobileFramebufferSwapBegin");
    IOMobileFramebufferReturn (*IOMobileFramebufferSwapEnd)(IOMobileFramebufferRef pointer) = dlsym(IOMobileFramebuffer, "IOMobileFramebufferSwapEnd");
    dlclose(IOMobileFramebuffer);
    printf("got display %p\n", display);
    int width = 0;
    int height = 0;
    width = size.width;
    height = size.height;
    printf("width: %d, height: %d", width, height);
    printf("\n\n\n");
    printf("got buffer at: %p", buffer);
    printf("\n\n\n");
    void *IOSurface = dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_LAZY);
    void (*IOSurfaceLock)(IOSurfaceRef buffer, int something, int something2) = dlsym(IOSurface, "IOSurfaceLock");
    void *(*IOSurfaceGetBaseAddress)(IOSurfaceRef buffer) = dlsym(IOSurface, "IOSurfaceGetBaseAddress");
    int (*IOSurfaceGetBytesPerRow)(IOSurfaceRef buffer) = dlsym(IOSurface, "IOSurfaceGetBytesPerRow");
    void (*IOSurfaceUnlock)(IOSurfaceRef buffer, int something, int something2) = dlsym(IOSurface, "IOSurfaceUnlock");
    dlclose(IOSurface);
    IOSurfaceLock(buffer, 0, 0);
    printf("locked buffer\n");
    void *base = IOSurfaceGetBaseAddress(buffer);
    printf("got base address at: %p", base);
    printf("\n\n\n");
    int bytesPerRow = IOSurfaceGetBytesPerRow(buffer);
    printf("got bytes per row: %d", bytesPerRow);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int offset = i * bytesPerRow + j * 4;
            *(int *)(base + offset) = 0xFF0000FF;
        }
    }
    write_string(base, bytesPerRow, width / 2 - 100, height / 2 + 30, "text...", 0xFFFFFFFF, 1);
    for (int i = 0; i < 100; i++) {
        make_progress_bar(base, bytesPerRow, width + 15, height / 2 + 100, width, 40, i, 0xFF00FF00);
        usleep(10000);
    }
    printf("wrote to buffer\n");
    IOSurfaceUnlock(buffer, 0, 0);
    printf("unlocked buffer\n");

    printf("pls work\n");
    int token;
    IOMobileFramebufferSwapBegin(display, &token);
    IOMobileFramebufferSwapEnd(display);
    char input[100];
    int prevX = 0;
    int prevY = 0;
    while (1) {
        printf("> ");
        scanf("%s", input);
        write_string(base, bytesPerRow, prevX, prevY, input, 0xFFFFFFFF, 0);
        prevY += 30;
        if (strcmp(input, "exit") == 0) {
            break;
        }
        if (strcmp(input, "clear") == 0) {
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    int offset = i * bytesPerRow + j * 4;
                    *(int *)(base + offset) = 0xFF0000FF;
                }
            }
            prevX = 0;
            prevY = 0;
        }
        if (strcmp(input, "progress_bar") == 0) {
            for (int i = 0; i < 100; i++) {
                make_progress_bar(base, bytesPerRow, width + 15, height / 2 + 100, width, 40, i, 0xFF00FF00);
                usleep(10000);
            }
        }
        if (strcmp(input, "draw_image") == 0) {
            FILE *image = fopen("/mnt1/private/var/root/image.tga", "r");
            if (image == NULL) {
                printf("image not found\n");
                continue;
            }
            unsigned char header[18];
            fread(header, 1, 18, image);
            int imageWidth = header[13] * 256 + header[12];
            int imageHeight = header[15] * 256 + header[14];
            int imageBPP = header[16];
            printf("image width: %d, image height: %d, image bpp: %d\n", imageWidth, imageHeight, imageBPP);
            unsigned char *imageData = malloc(imageWidth * imageHeight * 4);
            fread(imageData, 1, imageWidth * imageHeight * 4, image);
            fclose(image);
            for (int i = 0; i < imageHeight; i++) {
                for (int j = 0; j < imageWidth; j++) {
                    int offset = (i + height / 2 - imageHeight / 2) * bytesPerRow + (j + width / 2 - imageWidth / 2) * 4;
                    int imageOffset = i * imageWidth * 4 + j * 4;
                    *(int *)(base + offset) = *(int *)(imageData + imageOffset);
                }
            }
            free(imageData);

        }
        if (strcmp(input, "help") == 0) {
            write_string(base, bytesPerRow, prevX, prevY, "help", 0xFFFFFFFF, 0);
            prevY += 30;
            write_string(base, bytesPerRow, prevX, prevY, "exit", 0xFFFFFFFF, 0);
            prevY += 30;
            write_string(base, bytesPerRow, prevX, prevY, "clear", 0xFFFFFFFF, 0);
            prevY += 30;
            write_string(base, bytesPerRow, prevX, prevY, "progress_bar", 0xFFFFFFFF, 0);
            prevY += 30;
            write_string(base, bytesPerRow, prevX, prevY, "draw_image", 0xFFFFFFFF, 0);
            prevY += 30;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    printf("restored_banana: hello!\n");
    printf("restored_banana: init_display now!\n");
    init_display();
    printf("restored_banana: goodbye!\n");
    return 0;
}