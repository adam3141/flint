# 0011. XML is parsed with pugixml behind flint::xml

- Status: Accepted (supersedes ADR-0010)
- Date: 2026-11-25

## Context

ADR-0010 gave flint its own XML parser for agate's markup. The owner asked why we wrote one when good libraries exist, and decided (2026-11-25) that flint uses established libraries for commodity parsing. agate's markup loader uses `flint::xml::Element`, `Attribute` and `Location`, and its error messages depend on line and column positions.

Candidates were pugixml, tinyxml2 and expat. pugixml (MIT) is a fast DOM parser that is widely used, builds cleanly with GCC 16, and can parse in place. Its nodes report their offsets in the source, and strings parsed in place point into the buffer, so line and column can be recovered for elements, attributes and text.

## Decision

- `flint::xml::Parse` keeps its signature and types; pugixml v1.16 does the parsing (FetchContent, XPath disabled). pugixml is a private dependency: it does not appear in flint's headers.
- The text is parsed in place in a copy, and offsets are mapped to 1-based lines and byte columns.
- flint stays stricter than pugixml alone, because these are the mistakes people make when editing markup:
  - entities are decoded by flint, so an unknown or malformed entity is an error at its location;
  - `<` in an attribute value, duplicate attributes, a second root element and text outside the root are errors;
  - a mismatched end tag names the element that is still open.
- `<!DOCTYPE` is still rejected (new error code `DocumentTypeNotAllowed`), and elements still nest at most 256 deep.

## Consequences

- About 200 fewer lines of hand-written parsing to maintain; pugixml's well-tested tokenizer handles the edge cases.
- agate's markup loader did not change.
- The detail text of some errors changed (it now comes from pugixml's descriptions), but codes and locations did not, apart from DOCTYPE.
