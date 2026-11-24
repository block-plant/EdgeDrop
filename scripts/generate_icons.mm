#import <Cocoa/Cocoa.h>
#include <iostream>
#include <vector>

void drawSquircleAndBars(CGContextRef ctx, CGFloat canvasSize, CGFloat squircleX, CGFloat squircleY, CGFloat squircleSize, CGFloat cornerRadius, bool withShadow) {
    CGContextSaveGState(ctx);

    if (withShadow) {
        CGContextSetShadowWithColor(ctx, CGSizeMake(0, -canvasSize * 0.012), canvasSize * 0.024, [[NSColor colorWithCalibratedWhite:0.0 alpha:0.35] CGColor]);
    }

    // Squircle path
    NSRect squircleRect = NSMakeRect(squircleX, squircleY, squircleSize, squircleSize);
    NSBezierPath *squirclePath = [NSBezierPath bezierPathWithRoundedRect:squircleRect xRadius:cornerRadius yRadius:cornerRadius];
    
    // Fill squircle #111317
    NSColor *bgCol = [NSColor colorWithCalibratedRed:17.0/255.0 green:19.0/255.0 blue:23.0/255.0 alpha:1.0];
    [bgCol setFill];
    [squirclePath fill];

    CGContextRestoreGState(ctx);

    // Subtle hairline top highlight on squircle
    CGContextSaveGState(ctx);
    NSColor *strokeCol = [NSColor colorWithCalibratedWhite:1.0 alpha:0.12];
    [strokeCol setStroke];
    squirclePath.lineWidth = (canvasSize >= 128) ? (canvasSize / 256.0) : 0.5;
    [squirclePath stroke];
    CGContextRestoreGState(ctx);

    // Stepped bars #F4F5F7
    NSColor *barCol = [NSColor colorWithCalibratedRed:244.0/255.0 green:245.0/255.0 blue:247.0/255.0 alpha:1.0];
    [barCol setFill];

    CGFloat unit = squircleSize / 28.0;

    // Bar 1 (top): x offset 5, y offset (from top) 6, w 18, h 3.5, rx 1.75
    // Note: CoreGraphics coordinate origin is bottom-left, so y from top is: squircleY + squircleSize - (y_offset + h)
    CGFloat h = 3.5 * unit;
    CGFloat rx = 1.75 * unit;

    CGFloat b1_x = squircleX + 5.0 * unit;
    CGFloat b1_y = squircleY + squircleSize - (6.0 * unit + h);
    CGFloat b1_w = 18.0 * unit;
    NSBezierPath *bar1 = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(b1_x, b1_y, b1_w, h) xRadius:rx yRadius:rx];
    [bar1 fill];

    // Bar 2 (middle): x offset 9.5, y offset (from top) 12.25, w 13.5, h 3.5
    CGFloat b2_x = squircleX + 9.5 * unit;
    CGFloat b2_y = squircleY + squircleSize - (12.25 * unit + h);
    CGFloat b2_w = 13.5 * unit;
    NSBezierPath *bar2 = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(b2_x, b2_y, b2_w, h) xRadius:rx yRadius:rx];
    [bar2 fill];

    // Bar 3 (bottom): x offset 15, y offset (from top) 18.5, w 8, h 3.5
    CGFloat b3_x = squircleX + 15.0 * unit;
    CGFloat b3_y = squircleY + squircleSize - (18.5 * unit + h);
    CGFloat b3_w = 8.0 * unit;
    NSBezierPath *bar3 = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(b3_x, b3_y, b3_w, h) xRadius:rx yRadius:rx];
    [bar3 fill];
}

void savePNG(NSBitmapImageRep *rep, NSString *path) {
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    [data writeToFile:path atomically:YES];
}

void renderMacMaster(NSString *outPath) {
    int size = 1024;
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                    pixelsWide:size
                                                                    pixelsHigh:size
                                                                 bitsPerSample:8
                                                               samplesPerPixel:4
                                                                      hasAlpha:YES
                                                                      isPlanar:NO
                                                                colorSpaceName:NSCalibratedRGBColorSpace
                                                                   bytesPerRow:0
                                                                  bitsPerPixel:0];
    NSGraphicsContext *nsCtx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext setCurrentContext:nsCtx];
    CGContextRef ctx = (CGContextRef)[nsCtx graphicsPort];

    CGContextClearRect(ctx, CGRectMake(0, 0, size, size));

    // macOS HIG: 824x824 squircle centered in 1024x1024 (100px padding on each side, corner radius ~185)
    drawSquircleAndBars(ctx, 1024, 100, 100, 824, 185, true);

    [NSGraphicsContext setCurrentContext:nil];
    savePNG(rep, outPath);
}

void renderIconsetSize(int size, NSString *outPath) {
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                    pixelsWide:size
                                                                    pixelsHigh:size
                                                                 bitsPerSample:8
                                                               samplesPerPixel:4
                                                                      hasAlpha:YES
                                                                      isPlanar:NO
                                                                colorSpaceName:NSCalibratedRGBColorSpace
                                                                   bytesPerRow:0
                                                                  bitsPerPixel:0];
    NSGraphicsContext *nsCtx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext setCurrentContext:nsCtx];
    CGContextRef ctx = (CGContextRef)[nsCtx graphicsPort];

    CGContextClearRect(ctx, CGRectMake(0, 0, size, size));

    // Proportional to 1024 macOS master: squircle = size * (824.0 / 1024.0), pad = size * (100.0 / 1024.0), radius = size * (185.0 / 1024.0)
    CGFloat pad = size * (100.0 / 1024.0);
    CGFloat sqSize = size * (824.0 / 1024.0);
    CGFloat radius = size * (185.0 / 1024.0);
    bool shadow = (size >= 64);
    drawSquircleAndBars(ctx, size, pad, pad, sqSize, radius, shadow);

    [NSGraphicsContext setCurrentContext:nil];
    savePNG(rep, outPath);
}

void renderSquareLogoSize(int size, NSString *outPath) {
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                                    pixelsWide:size
                                                                    pixelsHigh:size
                                                                 bitsPerSample:8
                                                               samplesPerPixel:4
                                                                      hasAlpha:YES
                                                                      isPlanar:NO
                                                                colorSpaceName:NSCalibratedRGBColorSpace
                                                                   bytesPerRow:0
                                                                  bitsPerPixel:0];
    NSGraphicsContext *nsCtx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    [NSGraphicsContext setCurrentContext:nsCtx];
    CGContextRef ctx = (CGContextRef)[nsCtx graphicsPort];

    CGContextClearRect(ctx, CGRectMake(0, 0, size, size));

    // Standard brand squircle (matching docs/favicon.svg: 28/32 size, 2/32 margin, 7.5/32 radius)
    CGFloat pad = size * (2.0 / 32.0);
    CGFloat sqSize = size * (28.0 / 32.0);
    CGFloat radius = size * (7.5 / 32.0);
    drawSquircleAndBars(ctx, size, pad, pad, sqSize, radius, false);

    [NSGraphicsContext setCurrentContext:nil];
    savePNG(rep, outPath);
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSString *baseDir = @"assets/icons";
        NSString *iconsetDir = [baseDir stringByAppendingPathComponent:@"EdgeDrop.iconset"];
        [[NSFileManager defaultManager] createDirectoryAtPath:iconsetDir withIntermediateDirectories:YES attributes:nil error:nil];

        // 1. macOS Master PNG
        NSLog(@"Rendering macOS master PNG...");
        renderMacMaster([baseDir stringByAppendingPathComponent:@"EdgeDrop-macOS-1024.png"]);

        // 2. macOS iconset sizes
        NSLog(@"Rendering macOS iconset...");
        renderIconsetSize(16, [iconsetDir stringByAppendingPathComponent:@"icon_16x16.png"]);
        renderIconsetSize(32, [iconsetDir stringByAppendingPathComponent:@"icon_16x16@2x.png"]);
        renderIconsetSize(32, [iconsetDir stringByAppendingPathComponent:@"icon_32x32.png"]);
        renderIconsetSize(64, [iconsetDir stringByAppendingPathComponent:@"icon_32x32@2x.png"]);
        renderIconsetSize(128, [iconsetDir stringByAppendingPathComponent:@"icon_128x128.png"]);
        renderIconsetSize(256, [iconsetDir stringByAppendingPathComponent:@"icon_128x128@2x.png"]);
        renderIconsetSize(256, [iconsetDir stringByAppendingPathComponent:@"icon_256x256.png"]);
        renderIconsetSize(512, [iconsetDir stringByAppendingPathComponent:@"icon_256x256@2x.png"]);
        renderIconsetSize(512, [iconsetDir stringByAppendingPathComponent:@"icon_512x512.png"]);
        renderIconsetSize(1024, [iconsetDir stringByAppendingPathComponent:@"icon_512x512@2x.png"]);

        // 3. Square brand PNGs for Windows, Linux, and app icons
        NSLog(@"Rendering standard brand PNGs...");
        std::vector<int> squareSizes = {16, 24, 32, 48, 64, 128, 256, 512, 1024};
        for (int s : squareSizes) {
            NSString *filename = [NSString stringWithFormat:@"EdgeDrop-%d.png", s];
            renderSquareLogoSize(s, [baseDir stringByAppendingPathComponent:filename]);
        }

        // Copy 256 to resources/icons/app_icon.png
        NSString *resIcon = @"resources/icons/app_icon.png";
        [[NSFileManager defaultManager] copyItemAtPath:[baseDir stringByAppendingPathComponent:@"EdgeDrop-256.png"] toPath:resIcon error:nil];
        NSLog(@"Done rendering images.");
    }
    return 0;
}
