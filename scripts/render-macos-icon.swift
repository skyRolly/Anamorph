import AppKit
import Foundation

func bitmap(_ image: NSImage, _ points: Int, _ scale: Int) -> NSBitmapImageRep {
    let pixels = points * scale
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: pixels * 4, bitsPerPixel: 32)!
    rep.size = NSSize(width: points, height: points)
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    NSGraphicsContext.current!.imageInterpolation = .high
    image.size = rep.size
    image.draw(in: NSRect(origin: .zero, size: rep.size), from: .zero, operation: .copy,
               fraction: 1, respectFlipped: false, hints: nil)
    NSGraphicsContext.restoreGraphicsState()
    return rep
}

// A device-RGB, premultiplied RGBA bitmap gives both draws the same native
// color-management path. Logical size and bitmap size separately select 1x/2x.
let args = CommandLine.arguments
guard args.count == 5 else {
    fputs("usage: render-macos-icon OUTPUT SMALL_PNG BIG_PNG APP_BUNDLE\n", stderr)
    exit(2)
}
let output = URL(fileURLWithPath: args[1], isDirectory: true)
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)
let small = URL(fileURLWithPath: args[2])
let big = URL(fileURLWithPath: args[3])
var results: [[String: Any]] = []
for path in args.dropFirst(4) {
    let url = URL(fileURLWithPath: path)
    let icon = NSWorkspace.shared.icon(forFile: url.path)
    for points in [16, 32, 64, 128, 256, 512] {
        for scale in [1, 2] {
            let n = points * scale
            let reference = NSImage(contentsOf: n <= 32 ? small : big)!
            let actual = bitmap(icon.copy() as! NSImage, points, scale)
            let expected = bitmap(reference, points, scale)
            var rgbError = 0.0, alphaError = 0.0
            var intersection = 0, union = 0, opaque = 0, transparent = 0
            for y in 0..<n {
                for x in 0..<n {
                    let i = y * actual.bytesPerRow + x * 4
                    let j = y * expected.bytesPerRow + x * 4
                    let a = actual.bitmapData!, e = expected.bitmapData!
                    for c in 0..<3 { rgbError += abs(Double(a[i+c]) - Double(e[j+c])) / 255.0 }
                    alphaError += abs(Double(a[i+3]) - Double(e[j+3])) / 255.0
                    if a[i+3] > 127 || e[j+3] > 127 { union += 1 }
                    if a[i+3] > 127 && e[j+3] > 127 { intersection += 1 }
                    if a[i+3] >= 253 { opaque += 1 }
                    if a[i+3] <= 2 { transparent += 1 }
                }
            }
            let stem = url.deletingPathExtension().lastPathComponent + "-\(points)pt-\(scale)x"
            try actual.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent(stem + ".png"))
            try expected.representation(using: .png, properties: [:])!.write(to: output.appendingPathComponent("reference-\(points)pt-\(scale)x.png"))
            results.append(["bundle": url.path, "points": points, "scale": scale, "pixels": n,
                "rgb_mae": rgbError / Double(n*n*3), "alpha_mae": alphaError / Double(n*n),
                "silhouette_iou": Double(intersection) / Double(max(union, 1)),
                "transparent_fraction": Double(transparent)/Double(n*n),
                "opaque_fraction": Double(opaque)/Double(n*n)])
        }
    }
}
let data = try JSONSerialization.data(withJSONObject: results, options: [.prettyPrinted, .sortedKeys])
try data.write(to: output.appendingPathComponent("measurements.json"))
print(String(data: data, encoding: .utf8)!)
