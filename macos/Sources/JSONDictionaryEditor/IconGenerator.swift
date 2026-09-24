import AppKit
import Foundation

enum IconGenerator {
    static func writeICNS(to url: URL) throws {
        let specifications: [(String, Int)] = [
            ("icp4", 16),
            ("icp5", 32),
            ("icp6", 64),
            ("ic07", 128),
            ("ic08", 256),
            ("ic09", 512),
            ("ic10", 1024),
            ("ic11", 32),
            ("ic12", 64),
            ("ic13", 256),
            ("ic14", 512)
        ]

        var chunks: [(String, Data)] = []
        for (type, size) in specifications {
            chunks.append((type, try renderPNG(size: size)))
        }

        let totalLength = 8 + chunks.reduce(0) { $0 + 8 + $1.1.count }
        var data = Data("icns".utf8)
        appendBigEndian(UInt32(totalLength), to: &data)
        for (type, png) in chunks {
            data.append(Data(type.utf8))
            appendBigEndian(UInt32(8 + png.count), to: &data)
            data.append(png)
        }
        try data.write(to: url, options: .atomic)
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
        for row in 0..<3 {
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

        let braces = "{ }" as NSString
        let paragraph = NSMutableParagraphStyle()
        paragraph.alignment = .center
        let attributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedSystemFont(ofSize: CGFloat(size) * 0.235, weight: .bold),
            .foregroundColor: NSColor(calibratedRed: 0.18, green: 0.30, blue: 0.78, alpha: 1),
            .paragraphStyle: paragraph
        ]
        braces.draw(
            in: NSRect(
                x: cardRect.minX,
                y: cardRect.minY + CGFloat(size) * 0.025,
                width: cardRect.width,
                height: CGFloat(size) * 0.26
            ),
            withAttributes: attributes
        )

        context.flushGraphics()
        guard let png = representation.representation(using: .png, properties: [:]) else {
            throw CocoaError(.fileWriteUnknown)
        }
        return png
    }

    private static func appendBigEndian(_ value: UInt32, to data: inout Data) {
        var bigEndian = value.bigEndian
        withUnsafeBytes(of: &bigEndian) { buffer in
            data.append(contentsOf: buffer)
        }
    }
}
