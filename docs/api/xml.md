# XML

```cpp
#include "flint/xml/parser.hpp"   // Parse, ParseError
#include "flint/xml/element.hpp"  // Element, Attribute, Location
```

A small XML parser for markup and configuration files (ADR-0010). It reads a document into a tree of elements, and every element and attribute keeps its line and column for error messages.

```cpp
auto document = flint::xml::Parse(text);
if (!document.has_value()) {
  std::println("{}", document.error().Describe());   // "line 3, column 7: mismatched end tag: expected </Grid>, found </Border>"
  return;
}
const flint::xml::Element& root = *document;
std::string_view columns = root.GetAttribute("ColumnDefinitions", "*");
for (const flint::xml::Element& child : root.children) {
  std::println("<{}> at {}:{}", child.name, child.location.line, child.location.column);
}
```

## Elements

| Member | Meaning |
|---|---|
| `name` | Qualified name as written: `Button`, `x:String`, `Border.Background` |
| `GetLocalName()`, `GetPrefix()` | `String` and `x` for `x:String` |
| `attributes` | In document order, with values already decoded (`&amp;` becomes `&`) |
| `FindAttribute(name)`, `GetAttribute(name, fallback)` | Look up an attribute by qualified name |
| `children` | Child elements |
| `text` | Text directly inside the element, joined; text that is only whitespace is dropped |
| `location` | Where the start tag begins |

Attributes record where their name starts (`location`) and where their value starts (`valueLocation`), so an error can point at the value.

## What is supported

The parser handles:

- elements and attributes, in either quote style;
- text and CDATA;
- comments and processing instructions, which are skipped (including the XML declaration);
- the five predefined entities, and decimal and hexadecimal character references.

`<!DOCTYPE` is an error, so no external or expanding entities can be loaded. Nesting deeper than 256 elements is an error. Namespace prefixes are kept as written, not resolved.
