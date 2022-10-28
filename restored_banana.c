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

void *base = NULL;
int bytesPerRow = 0;
int height = 0;
int width = 0;

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
    base = IOSurfaceGetBaseAddress(buffer);
    printf("got base address at: %p", base);
    printf("\n\n\n");
    bytesPerRow = IOSurfaceGetBytesPerRow(buffer);
    printf("got bytes per row: %d", bytesPerRow);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int offset = i * bytesPerRow + j * 4;
            *(int *)(base + offset) = 0xFF0000FF;
        }
    }
    printf("wrote to buffer\n");
    IOSurfaceUnlock(buffer, 0, 0);
    printf("unlocked buffer\n");

    printf("pls work\n");
    int token;
    IOMobileFramebufferSwapBegin(display, &token);
    IOMobileFramebufferSwapEnd(display);
    return 0;
}

int main(int argc, char *argv[])
{
    printf("restored_banana: hello!\n");
    printf("restored_banana: init_display now!\n");
    if (argc == 2 && strcmp(argv[1], "child") == 0) {
        init_display();
        return 0;
    }
    pid_t pid = fork();
    if (pid == 0) {
        // you probably hate me for this, but it's the only way to get the display to work as far as I can tell
        char *args[] = {argv[0], "child", NULL};
        execve(argv[0], args, NULL);
    }
    sleep(1);
    printf("restored_banana: init_display done, doing our part...\n");
    init_display();
    printf("restored_banana: done!\n");
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int offset = i * bytesPerRow + j * 4;
            *(int *)(base + offset) = 0x00000000;
        }
    }
    // open shell
    char *input = malloc(100);
    int prevX = 0;
    int prevY = 0;
    while (1) {
        printf("restored_banana: ");
        scanf("%s", input);
        if (strcmp(input, "exit") == 0) {
            break;
        } else if (strcmp(input, "clear") == 0) {
            for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                    int offset = i * bytesPerRow + j * 4;
                    *(int *)(base + offset) = 0x00000000;
                }
            }
        } else if (strcmp(input, "echo") == 0) {
            scanf("%s", input);
            write_string(base, bytesPerRow, prevX, prevY, input, 0xFFFFFFFF, 1);
            prevY += 30;
            printf("restored_banana: wrote string: %s\n", input);
        } else if (strcmp(input, "drawimage") == 0) {
            // read a TGA 32-bit image from /mnt1/private/var/root/image.tga and draw it to the middle of the screen
            FILE *image = fopen("/mnt1/private/var/root/image.tga", "rb");
            if (image == NULL) {
                printf("restored_banana: failed to open image\n");
                continue;
            }
            // header
            char header[18];
            fread(header, 1, 18, image);
            int imageWidth = header[13] * 256 + header[12];
            int imageHeight = header[15] * 256 + header[14];
            int imageBytesPerRow = imageWidth * 4;
            int imageBase = (height / 2 - imageHeight / 2) * bytesPerRow + (width / 2 - imageWidth / 2) * 4;
            printf("restored_banana: image width: %d, image height: %d, image bytes per row: %d, image base: %d\n", imageWidth, imageHeight, imageBytesPerRow, imageBase);
            // image data
            char *imageData = malloc(imageHeight * imageBytesPerRow);
            fread(imageData, 1, imageHeight * imageBytesPerRow, image);
            for (int i = 0; i < imageHeight; i++) {
                for (int j = 0; j < imageWidth; j++) {
                    int offset = i * imageBytesPerRow + j * 4;
                    int offset2 = i * bytesPerRow + j * 4;
                    *(int *)(base + imageBase + offset2) = *(int *)(imageData + offset);
                }
            }
            free(imageData);
            fclose(image);

        }
    }
    printf("restored_banana: goodbye!\n");
    return 0;
}