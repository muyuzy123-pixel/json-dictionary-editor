// Copyright (c) 2026 muyuzy123-pixel
// SPDX-License-Identifier: MIT
// Geometric layout/colors adapted from macos/.../IconGenerator.swift.
// Only the font-rendered braces are replaced with independent Bezier strokes.
// This optional maintainer tool is not invoked by Windows builds.
import AppKit
import Foundation
import Darwin

@main
enum WindowsIconGenerator {
    static let sizes = [16, 24, 32, 48, 64, 128, 256]

    static func main() {
        let args = CommandLine.arguments
        guard args.count == 2 || args.count == 3 else {
            fputs("usage: generate-icon OUTPUT.ico [PREVIEW_DIRECTORY]\n", stderr)
            exit(2)
        }
        do {
            let frames = try sizes.map { try renderPNG(size: $0) }
            var ico = Data()
            appendLE(UInt16(0), to: &ico)
            appendLE(UInt16(1), to: &ico)
            appendLE(UInt16(sizes.count), to: &ico)
            var offset = 6 + 16 * sizes.count
            for (size, png) in zip(sizes, frames) {
                ico.append(UInt8(size == 256 ? 0 : size))
                ico.append(UInt8(size == 256 ? 0 : size))
                ico.append(contentsOf: [0, 0])
                appendLE(UInt16(1), to: &ico)
                appendLE(UInt16(32), to: &ico)
                appendLE(UInt32(png.count), to: &ico)
                appendLE(UInt32(offset), to: &ico)
                offset += png.count
            }
            for png in frames { ico.append(png) }
            let output = URL(fileURLWithPath: args[1])
            try ico.write(to: output, options: .atomic)
            if args.count == 3 {
                let previews = URL(fileURLWithPath: args[2], isDirectory: true)
                try FileManager.default.createDirectory(at: previews, withIntermediateDirectories: true)
                for (size, png) in zip(sizes, frames) {
                    try png.write(to: previews.appendingPathComponent("icon-\(size).png"), options: .atomic)
                }
                try writePreview(frames: frames, to: previews.appendingPathComponent("icon-preview.png"))
            }
            print("ICO_GENERATED: 16,24,32,48,64,128,256; PNG RGBA; no font assets")
        } catch {
            fputs("icon generation failed: \(error.localizedDescription)\n", stderr)
            exit(1)
        }
    }

    private static func appendLE<T: FixedWidthInteger>(_ value: T, to data: inout Data) {
        var littleEndian = value.littleEndian
        withUnsafeBytes(of: &littleEndian) { data.append(contentsOf: $0) }
    }

    // Review-only sheet: each column is one size; top light, bottom dark.
    // First five sizes are magnified 3x with nearest-neighbor interpolation.
    // No text/font rendering is used, including in this optional preview.
    private static func writePreview(frames: [Data], to url: URL) throws {
        let width = 1192, height = 600
        guard let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: width,
            pixelsHigh: height, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
            isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
            let context = NSGraphicsContext(bitmapImageRep: rep) else {
            throw CocoaError(.fileWriteUnknown)
        }
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = context
        defer { NSGraphicsContext.restoreGraphicsState() }
        NSColor(calibratedWhite: 0.15, alpha: 1).setFill()
        NSRect(x: 0, y: 0, width: width, height: height / 2).fill()
        NSColor(calibratedWhite: 0.96, alpha: 1).setFill()
        NSRect(x: 0, y: height / 2, width: width, height: height / 2).fill()
        var x: CGFloat = 16
        for (size, png) in zip(sizes, frames) {
            guard let image = NSImage(data: png) else { throw CocoaError(.fileReadCorruptFile) }
            let displaySize = CGFloat(size <= 64 ? size * 3 : size)
            context.imageInterpolation = .none
            for y: CGFloat in [150, 450] {
                image.draw(in: NSRect(x: x, y: y - displaySize / 2, width: displaySize, height: displaySize),
                           from: .zero, operation: .sourceOver, fraction: 1)
            }
            x += displaySize + 24
        }
        context.flushGraphics()
        guard let png = rep.representation(using: .png, properties: [:]) else {
            throw CocoaError(.fileWriteUnknown)
        }
        try png.write(to: url, options: .atomic)
    }

    private static func renderPNG(size: Int) throws -> Data {
        guard let representation = NSBitmapImageRep(
            bitmapDataPlanes: nil,
            pixelsWide: size,
            pixelsHigh: size,
            bitsPerSample: 8,
            samplesPerPixel: 4,
            hasAlpha: true,
            isPlanar: false,
            colorSpaceName: .deviceRGB,
            bytesPerRow: 0,
            bitsPerPixel: 0
        ), let context = NSGraphicsContext(bitmapImageRep: representation) else {
            throw CocoaError(.fileWriteUnknown)
        }

        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = context
        defer { NSGraphicsContext.restoreGraphicsState() }

        let canvas = NSRect(x: 0, y: 0, width: size, height: size)
        NSColor.clear.setFill()
        canvas.fill()

        let inset = CGFloat(size) * 0.055
        let iconRect = canvas.insetBy(dx: inset, dy: inset)
        let radius = CGFloat(size) * 0.215
        let background = NSBezierPath(roundedRect: iconRect, xRadius: radius, yRadius: radius)
        let gradient = NSGradient(colors: [
            NSColor(calibratedRed: 0.18, green: 0.30, blue: 0.92, alpha: 1),
            NSColor(calibratedRed: 0.16, green: 0.70, blue: 0.84, alpha: 1)
        ])!
        gradient.draw(in: background, angle: -45)

        let innerInset = CGFloat(size) * 0.17
        let cardRect = canvas.insetBy(dx: innerInset, dy: innerInset * 1.08)
        let card = NSBezierPath(roundedRect: cardRect, xRadius: CGFloat(size) * 0.085, yRadius: CGFloat(size) * 0.085)
        NSColor.white.withAlphaComponent(0.96).setFill()
        card.fill()

        let rowColor = NSColor(calibratedRed: 0.22, green: 0.35, blue: 0.82, alpha: 0.78)
        let dotColor = NSColor(calibratedRed: 0.12, green: 0.66, blue: 0.72, alpha: 1)
        for row in 0..<(size <= 32 ? 2 : 3) {
            let y = cardRect.maxY - CGFloat(size) * (0.20 + CGFloat(row) * 0.145)
            let dotRect = NSRect(
                x: cardRect.minX + CGFloat(size) * 0.09,
                y: y - CGFloat(size) * 0.022,
                width: CGFloat(size) * 0.044,
                height: CGFloat(size) * 0.044
            )
            dotColor.setFill()
            NSBezierPath(ovalIn: dotRect).fill()

            let lineRect = NSRect(
                x: dotRect.maxX + CGFloat(size) * 0.045,
                y: y - CGFloat(size) * 0.012,
                width: CGFloat(size) * (row == 1 ? 0.29 : 0.36),
                height: max(1, CGFloat(size) * 0.024)
            )
            rowColor.setFill()
            NSBezierPath(roundedRect: lineRect, xRadius: lineRect.height / 2, yRadius: lineRect.height / 2).fill()
        }

        // Independently defined geometric braces. No font API or extracted glyph outline.
        let braceColor = NSColor(calibratedRed: 0.18, green: 0.30, blue: 0.78, alpha: 1)
        braceColor.setStroke()
        for mirrored in [false, true] {
            func p(_ x: CGFloat, _ y: CGFloat) -> NSPoint {
                NSPoint(x: (mirrored ? 1 - x : x) * CGFloat(size), y: y * CGFloat(size))
            }
            let brace = NSBezierPath()
            brace.move(to: p(0.39, 0.425))
            brace.curve(to: p(0.352, 0.385), controlPoint1: p(0.352, 0.425), controlPoint2: p(0.352, 0.415))
            brace.curve(to: p(0.325, 0.325), controlPoint1: p(0.352, 0.345), controlPoint2: p(0.352, 0.325))
            brace.curve(to: p(0.352, 0.265), controlPoint1: p(0.352, 0.325), controlPoint2: p(0.352, 0.305))
            brace.curve(to: p(0.39, 0.225), controlPoint1: p(0.352, 0.235), controlPoint2: p(0.352, 0.225))
            brace.lineWidth = max(1, CGFloat(size) * 0.028)
            brace.lineCapStyle = .round
            brace.lineJoinStyle = .round
            brace.stroke()
        }

        context.flushGraphics()
        guard let png = representation.representation(using: .png, properties: [:]) else {
            throw CocoaError(.fileWriteUnknown)
        }
        return png
    }

}
