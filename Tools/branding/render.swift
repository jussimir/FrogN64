// Original code-drawn FrogN64 mark. Run: swift Tools/branding/render.swift
import Foundation
import CoreGraphics
import CoreText
import ImageIO
import UniformTypeIdentifiers

let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
let output = root.appendingPathComponent("Source/SysPSP/Resources/frogn64")
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let background = CGColor(red: 64/255, green: 120/255, blue: 96/255, alpha: 1)
let info = CGColor(red: 144/255, green: 224/255, blue: 176/255, alpha: 1)
let cream = CGColor(red: 248/255, green: 248/255, blue: 232/255, alpha: 1)

func render(_ width: Int, _ height: Int, _ filename: String) {
    let ctx = CGContext(data: nil, width: width, height: height, bitsPerComponent: 8,
        bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
    func rect(_ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat, _ color: CGColor) {
        ctx.setFillColor(color)
        ctx.fill(CGRect(x: x, y: CGFloat(height)-y-h, width: w, height: h))
    }
    func text(_ value: String, _ size: CGFloat, _ baseline: CGFloat, _ color: CGColor) {
        let font = CTFontCreateWithName("Menlo-Bold" as CFString, size, nil)
        let line = CTLineCreateWithAttributedString(NSAttributedString(string: value, attributes: [
            NSAttributedString.Key(kCTFontAttributeName as String): font,
            NSAttributedString.Key(kCTForegroundColorAttributeName as String): color]))
        let w = CTLineGetTypographicBounds(line, nil, nil, nil)
        ctx.textPosition = CGPoint(x: (CGFloat(width)-w)/2, y: CGFloat(height)-baseline)
        CTLineDraw(line, ctx)
    }
    rect(0, 0, CGFloat(width), CGFloat(height), background)
    let large = width > 144
    let scale: CGFloat = large ? 1.2 : 0.5
    let ox = CGFloat(width)/2 - 61*scale
    let oy: CGFloat = large ? 65 : 13
    func frog(_ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat, _ c: CGColor) {
        rect(ox+x*scale, oy+y*scale, w*scale, h*scale, c)
    }
    frog(12,0,26,28,info); frog(84,0,26,28,info)
    frog(12,20,98,48,info); frog(0,36,122,24,info)
    frog(20,8,10,12,background); frog(92,8,10,12,background)
    frog(36,50,50,4,background)
    text("FrogN64", large ? 30 : 15, large ? 190 : 68, cream)
    if large { text("Nintendo 64 on PSP", 12, 220, info) }
    let image = ctx.makeImage()!
    let url = output.appendingPathComponent(filename)
    let dest = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil)!
    CGImageDestinationAddImage(dest, image, nil)
    precondition(CGImageDestinationFinalize(dest))
    print(url.path)
}
render(144, 80, "icon0.png")
render(480, 272, "pic1.png")
