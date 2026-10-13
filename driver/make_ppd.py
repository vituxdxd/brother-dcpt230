#!/usr/bin/env python3
"""Generate the CUPS PPD for the Brother DCP-T230 open driver.

Page sizes and their dimensions come from papers.tsv, which the brother-t230
filter also uses at run time, so the PPD and the filter always agree.

ImageableArea values are the printer's hardware margins in points (9 pt sides,
34 pt top for envelopes, full bleed for the borderless "B" sizes).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAPERS = os.path.join(HERE, "papers.tsv")
OUT = os.path.join(HERE, "ppd", "brother-t230.ppd")

# Margens de hardware por tamanho: (esquerda, baixo, direita, topo) em pontos.
MARGINS = {
    "195x270mm": (9, 9, 544, 756),
    "A4": (9, 9, 586, 833),
    "A5": (9, 9, 411, 586),
    "A6": (9, 9, 288, 411),
    "B5": (9, 9, 507, 720),
    "BrA4_B": (0, 0, 595, 842),
    "BrA6_B": (0, 0, 297, 420),
    "BrIndexC5x8_B": (0, 0, 360, 576),
    "BrIndexC5x8_S": (9, 9, 351, 567),
    "BrLetter_B": (0, 0, 612, 792),
    "BrPhoto2L_B": (0, 0, 359, 505),
    "BrPhoto2L_S": (9, 9, 350, 496),
    "BrPhotoL_B": (0, 0, 252, 360),
    "BrPhotoL_S": (9, 9, 243, 351),
    "BrPostC4x6_B": (0, 0, 288, 432),
    "BrPostC4x6_S": (9, 9, 279, 423),
    "Env10": (9, 34, 288, 650),
    "EnvC5": (9, 34, 450, 615),
    "EnvDL": (9, 34, 303, 590),
    "EnvMonarch": (9, 34, 270, 506),
    "Executive": (9, 9, 513, 747),
    "FanFoldGermanLegal": (9, 9, 603, 927),
    "IndianLegal": (9, 9, 600, 969),
    "JISB6": (9, 9, 354, 507),
    "Legal": (9, 9, 603, 999),
    "Letter": (9, 9, 603, 783),
    "MexicanLegal": (9, 9, 603, 954),
}

HEADER = """*PPD-Adobe: "4.3"
*FormatVersion: "4.3"
*FileVersion: "1.0"
*LanguageVersion: English
*LanguageEncoding: ISOLatin1
*PCFileName: "BRT230O.PPD"
*Manufacturer: "Brother"
*Product: "(DCP-T230)"
*ModelName: "Brother DCP-T230 open driver"
*ShortNickName: "DCP-T230 open"
*NickName: "Brother DCP-T230 (open driver)"
*PSVersion: "(3010.000) 0"
*LanguageLevel: "3"
*ColorDevice: True
*DefaultColorSpace: RGB
*FileSystem: False
*Throughput: "1"
*LandscapeOrientation: Plus90
*TTRasterizer: Type42
*cupsFilter: "application/vnd.cups-postscript 0 brother-t230"
*cupsFilter: "application/vnd.cups-pdf 0 brother-t230"
*cupsManualCopies: True
*DefaultPageSize: A4
"""

# Driver options. The filter reads the values from argv[5]; the PostScript
# snippets only keep the PPD syntactically valid for CUPS.
OPTIONS = """
*OpenUI *BRResolution/Print Quality: PickOne
*OrderDependency: 10 AnySetup *BRResolution
*DefaultBRResolution: Normal
*BRResolution Draft/Draft: " "
*BRResolution Normal/Normal: " "
*BRResolution Fine/Fine: " "
*CloseUI: *BRResolution

*OpenUI *BRMonoColor/Color Mode: PickOne
*OrderDependency: 10 AnySetup *BRMonoColor
*DefaultBRMonoColor: Auto
*BRMonoColor Auto/Auto: " "
*BRMonoColor FullColor/Full Color: " "
*BRMonoColor Mono/Mono: " "
*CloseUI: *BRMonoColor

*OpenUI *BRMediaType/Media Type: PickOne
*OrderDependency: 10 AnySetup *BRMediaType
*DefaultBRMediaType: Plain
*BRMediaType Plain/Plain Paper: " "
*BRMediaType Thick/Thick Paper: " "
*BRMediaType Thicker/Thicker Paper: " "
*BRMediaType BOND/Bond Paper: " "
*BRMediaType Recycled/Recycled Paper: " "
*BRMediaType Env/Envelopes: " "
*BRMediaType EnvThin/Envelope (thin): " "
*BRMediaType EnvThick/Envelope (thick): " "
*BRMediaType Label/Label: " "
*BRMediaType PostCard/Postcard: " "
*BRMediaType Inkjet/Inkjet Paper: " "
*BRMediaType Glossy/Glossy Paper: " "
*CloseUI: *BRMediaType

*OpenUI *BRInputSlot/Paper Source: PickOne
*OrderDependency: 10 AnySetup *BRInputSlot
*DefaultBRInputSlot: AutoSelect
*BRInputSlot AutoSelect/Auto Select: " "
*BRInputSlot Tray1/Tray 1: " "
*BRInputSlot Manual/Manual Feed: " "
*CloseUI: *BRInputSlot

*OpenUI *BRDuplex/Manual Duplex: PickOne
*OrderDependency: 10 AnySetup *BRDuplex
*DefaultBRDuplex: None
*BRDuplex None/Off: " "
*BRDuplex DuplexTumble/Short-edge binding: " "
*BRDuplex DuplexNoTumble/Long-edge binding: " "
*CloseUI: *BRDuplex
"""


def main():
    rows = []
    with open(PAPERS) as f:
        header = f.readline().rstrip("\n").split("\t")
        for line in f:
            fields = line.rstrip("\n").split("\t")
            rows.append(dict(zip(header, fields)))

    # A4 first, then alphabetical
    rows.sort(key=lambda r: (r["ppd"] != "A4", r["ppd"]))

    parts = [HEADER]
    parts.append("*OpenUI *PageSize/Page Size: PickOne\n")
    parts.append("*OrderDependency: 10 AnySetup *PageSize\n")
    for r in rows:
        pt_w, pt_h = int(r["ptw"]), int(r["pth"])
        iml, imt, imr, imb = MARGINS[r["ppd"]]
        snippet = ('<</PageSize[%d %d]/ImagingBBox[%d %d %d %d]'
                   '>>setpagedevice' % (pt_w, pt_h, iml, imt, imr, imb))
        parts.append('*PageSize %s/%s: "%s"\n' % (r["ppd"], r["ppd"], snippet))
    parts.append("*CloseUI: *PageSize\n")

    parts.append("*OpenUI *PageRegion/Page Region: PickOne\n")
    parts.append("*OrderDependency: 10 AnySetup *PageRegion\n")
    for r in rows:
        pt_w, pt_h = int(r["ptw"]), int(r["pth"])
        iml, imt, imr, imb = MARGINS[r["ppd"]]
        snippet = ('<</PageSize[%d %d]/ImagingBBox[%d %d %d %d]'
                   '>>setpagedevice' % (pt_w, pt_h, iml, imt, imr, imb))
        parts.append('*PageRegion %s/%s: "%s"\n' % (r["ppd"], r["ppd"], snippet))
    parts.append("*CloseUI: *PageRegion\n")

    parts.append(OPTIONS)

    # Defaults required by the PPD spec
    parts.append("*DefaultPageRegion: A4\n")
    parts.append("*DefaultPaperDimension: A4\n")
    parts.append("*DefaultImageableArea: A4\n")

    for r in rows:
        pt_w, pt_h = int(r["ptw"]), int(r["pth"])
        iml, imt, imr, imb = MARGINS[r["ppd"]]
        parts.append('*PaperDimension %s: "%d %d"\n' % (r["ppd"], pt_w, pt_h))
        parts.append('*ImageableArea %s: "%d %d %d %d"\n'
                     % (r["ppd"], iml, imt, imr, imb))

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write("".join(parts))
    print("wrote %s (%d page sizes)" % (OUT, len(rows)))


if __name__ == "__main__":
    sys.exit(main())
