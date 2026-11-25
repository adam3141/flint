# 0010. flint has its own small XML parser

- Status: Superseded by [ADR-0011](0011-xml-with-pugixml.md)
- Date: 2026-11-25

## Context

agate's markup, styles and themes are near-XAML XML (agate ADR-0001), and the owner wanted them in milestone 3. Parsing XML is not UI code, so it belongs in flint. The options were a third-party library (pugixml, tinyxml2, expat) or a small parser of our own, as with JSON (ADR-0005).

## Decision

- `flint::xml::Parse` reads a document into an `Element` tree: names with prefixes, attributes, child elements and joined text.
- Every element and attribute records its line and column, so markup errors can point at the right place.
- It supports what markup and configuration files use: elements, attributes in either quote style, text, CDATA, comments, processing instructions, the XML declaration, and the predefined and numeric character entities.
- DTDs are rejected, so neither external entities nor entity-expansion attacks are possible.
- Nesting is limited to 256 levels.
- Errors are `std::expected` with a code, a location and detail ("expected </Grid>, found </Border>").

## Consequences

- No new dependency. The parser is about 400 lines, with every location the markup layer needs.
- Namespaces are not resolved; prefixes are kept as written (`x:Name`). agate's markup treats `x:` as directives and needs nothing more.
- Mixed content (text between child elements) is joined into one string, which suits markup but not documents. A full DOM can come later if something needs it.
